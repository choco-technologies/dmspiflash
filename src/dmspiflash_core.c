#include <errno.h>
#include <string.h>
#include "dmod.h"
#include "dmosi.h"
#include "dmspiflash.h"
#include "dmspiflash_internal.h"

/*
 * Flash core - chip logic on top of the bus layer (dmspiflash_bus_*). The
 * caller (the driver in dmspiflash.c) holds ctx->lock for the whole call, so
 * WREN + program + poll never interleave with another thread's command.
 */

#define READ_CHUNK          (32u * 1024u)   /* A bus transfer: DMA-able on the controller */
#define ERASE_64K_MS        3000u           /* Worst case of a 64 KB erase */

static dmspiflash_cmd_t cmd_plain(uint8_t opcode)
{
    dmspiflash_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.opcode = opcode;
    return c;
}

static dmspiflash_cmd_t cmd_addr(uint8_t opcode, uint32_t address, dmspiflash_lines_t lines)
{
    dmspiflash_cmd_t c = cmd_plain(opcode);
    c.has_address = true;
    c.address = address;
    c.address_lines = lines;
    return c;
}

static int cmd_read(struct dmdrvi_context *ctx, uint8_t opcode, void *out, uint32_t size)
{
    dmspiflash_cmd_t c = cmd_plain(opcode);
    c.data_lines = dmspiflash_lines_1;
    c.rx = out;
    c.size = size;
    return dmspiflash_bus_command(ctx, &c);
}

static int write_enable(struct dmdrvi_context *ctx)
{
    dmspiflash_cmd_t c = cmd_plain(DMSPIFLASH_CMD_WRITE_ENABLE);
    return dmspiflash_bus_command(ctx, &c);
}

/* After a program/erase: wait for WIP to clear, then (Micron) check the flag status register. */
static int wait_done(struct dmdrvi_context *ctx, uint32_t timeout_ms, uint8_t error_bit)
{
    int ret = dmspiflash_bus_poll(ctx, DMSPIFLASH_CMD_READ_STATUS, DMSPIFLASH_STATUS_WIP, 0, timeout_ms);
    if (ret != 0 || (ctx->chip->flags & DMSPIFLASH_CHIP_FLAG_STATUS) == 0)
        return ret;
    uint8_t flag = 0;
    ret = cmd_read(ctx, DMSPIFLASH_CMD_READ_FLAG_STATUS, &flag, 1);
    if (ret == 0 && (flag & error_bit) != 0)
    {
        dmspiflash_cmd_t clear = cmd_plain(DMSPIFLASH_CMD_CLEAR_FLAG_STATUS);
        (void)dmspiflash_bus_command(ctx, &clear);
        ret = -EIO;
    }
    return ret;
}

static int select_chip(struct dmdrvi_context *ctx, const dmspiflash_jedec_id_t *id)
{
    ctx->chip = ctx->config.chip_name[0] != '\0' ? dmspiflash_find_chip_by_name(ctx->config.chip_name)
                                                 : dmspiflash_find_chip_by_id(id);
    if (ctx->chip == NULL)
    {
        DMOD_LOG_ERROR("dmspiflash: unknown flash chip (JEDEC %02x %02x %02x, chip='%s') - add a row to the chip table\n",
                       id->manufacturer, id->memory_type, id->capacity, ctx->config.chip_name);
        return -ENODEV;
    }
    return 0;
}

/* The chip's quad mode. Micron N25Q: 0xEB works in the reset default (extended SPI protocol,
 * 10 dummy cycles), nothing to enable. The other vendors need their QE bit - not implemented. */
static int enable_quad(struct dmdrvi_context *ctx)
{
    if (!ctx->config.use_quad || ctx->chip->quad_enable == dmspiflash_quad_micron_vcr)
        return 0;
    DMOD_LOG_ERROR("dmspiflash: quad enable of '%s' is not implemented\n", ctx->chip->name);
    return -ENOTSUP;
}

int dmspiflash_core_open(struct dmdrvi_context *ctx)
{
    (void)dmspiflash_bus_map(ctx, false);       /* A leftover memory-mapped mode: back to indirect */
    dmspiflash_cmd_t reset_enable = cmd_plain(DMSPIFLASH_CMD_RESET_ENABLE);
    dmspiflash_cmd_t reset = cmd_plain(DMSPIFLASH_CMD_RESET_MEMORY);
    int ret = dmspiflash_bus_command(ctx, &reset_enable);
    if (ret == 0)
        ret = dmspiflash_bus_command(ctx, &reset);
    if (ret != 0)
        return ret;
    dmosi_thread_sleep(2);                      /* The chip needs ~30 us after a reset; a tick is plenty */

    dmspiflash_jedec_id_t id;
    uint8_t raw[3] = { 0 };
    ret = cmd_read(ctx, DMSPIFLASH_CMD_READ_ID, raw, 3);
    if (ret != 0)
        return ret;
    id.manufacturer = raw[0];
    id.memory_type = raw[1];
    id.capacity = raw[2];
    ret = select_chip(ctx, &id);
    if (ret == 0)
        ret = enable_quad(ctx);
    if (ret == 0 && ctx->config.memory_mapped)
    {
        ret = dmspiflash_bus_map(ctx, true);
        ctx->mapped = (ret == 0);
    }
    if (ret == 0)
        DMOD_LOG_INFO("dmspiflash: %s, %u KiB, JEDEC %02x %02x %02x\n", ctx->chip->name,
                      (unsigned)(ctx->chip->size_bytes / 1024u), id.manufacturer, id.memory_type, id.capacity);
    return ret;
}

/* Leave memory-mapped mode for an indirect operation; the caller re-enters it afterwards. */
static int suspend_mapped(struct dmdrvi_context *ctx)
{
    return ctx->mapped ? dmspiflash_bus_map(ctx, false) : 0;
}

static int resume_mapped(struct dmdrvi_context *ctx, uint32_t offset, uint32_t size, bool changed)
{
    if (!ctx->mapped)
        return 0;
    if (changed)
        dmspiflash_bus_invalidate(ctx, offset, size);
    return dmspiflash_bus_map(ctx, true);
}

static int read_chunk(struct dmdrvi_context *ctx, uint32_t offset, uint8_t *out, uint32_t size)
{
    bool quad = ctx->config.use_quad;
    dmspiflash_cmd_t c = cmd_addr(quad ? DMSPIFLASH_CMD_FAST_READ_QUAD_IO : DMSPIFLASH_CMD_FAST_READ,
                                  offset, quad ? dmspiflash_lines_4 : dmspiflash_lines_1);
    c.dummy_cycles = quad ? ctx->chip->read_dummy_cycles : 8;
    c.data_lines = quad ? dmspiflash_lines_4 : dmspiflash_lines_1;
    c.rx = out;
    c.size = size;
    return dmspiflash_bus_command(ctx, &c);
}

int dmspiflash_core_read(struct dmdrvi_context *ctx, uint32_t offset, void *buffer, size_t size)
{
    if ((uint64_t)offset + size > ctx->chip->size_bytes)
        return -EINVAL;
    int ret = suspend_mapped(ctx);
    for (uint32_t done = 0; ret == 0 && done < size; )
    {
        uint32_t n = (size - done) < READ_CHUNK ? (uint32_t)(size - done) : READ_CHUNK;
        ret = read_chunk(ctx, offset + done, (uint8_t *)buffer + done, n);
        done += n;
    }
    int resumed = resume_mapped(ctx, 0, 0, false);
    return ret != 0 ? ret : (resumed != 0 ? resumed : (int)size);
}

static int program_chunk(struct dmdrvi_context *ctx, uint32_t offset, const uint8_t *data, uint32_t size)
{
    bool quad = ctx->config.use_quad && (ctx->chip->flags & DMSPIFLASH_CHIP_QUAD_PROGRAM) != 0;
    dmspiflash_cmd_t c = cmd_addr(quad ? DMSPIFLASH_CMD_QUAD_PAGE_PROGRAM : DMSPIFLASH_CMD_PAGE_PROGRAM,
                                  offset, dmspiflash_lines_1);
    c.data_lines = quad ? dmspiflash_lines_4 : dmspiflash_lines_1;
    c.tx = data;
    c.size = size;
    int ret = write_enable(ctx);
    if (ret == 0)
        ret = dmspiflash_bus_command(ctx, &c);
    if (ret == 0)
        ret = wait_done(ctx, ctx->chip->page_program_ms + 5u, 1u << 4);
    return ret;
}

int dmspiflash_core_write(struct dmdrvi_context *ctx, uint32_t offset, const void *buffer, size_t size)
{
    if ((uint64_t)offset + size > ctx->chip->size_bytes)
        return -EINVAL;
    int ret = suspend_mapped(ctx);
    for (uint32_t done = 0; ret == 0 && done < size; )
    {
        uint32_t n = dmspiflash_page_chunk(ctx->chip->page_size, offset + done, (uint32_t)(size - done));
        ret = program_chunk(ctx, offset + done, (const uint8_t *)buffer + done, n);
        done += n;
    }
    int resumed = resume_mapped(ctx, offset, (uint32_t)size, true);
    return ret != 0 ? ret : (resumed != 0 ? resumed : (int)size);
}

static int erase_one(struct dmdrvi_context *ctx, uint32_t offset, bool big)
{
    dmspiflash_cmd_t c = cmd_addr(big ? DMSPIFLASH_CMD_ERASE_64K : DMSPIFLASH_CMD_ERASE_4K, offset, dmspiflash_lines_1);
    int ret = write_enable(ctx);
    if (ret == 0)
        ret = dmspiflash_bus_command(ctx, &c);
    if (ret == 0)
        ret = wait_done(ctx, big ? ERASE_64K_MS : ctx->chip->sector_erase_ms, 1u << 5);
    return ret;
}

int dmspiflash_core_erase(struct dmdrvi_context *ctx, uint32_t offset, uint32_t size)
{
    const dmspiflash_chip_t *chip = ctx->chip;
    if (size == 0 || offset % chip->sector_size != 0 || size % chip->sector_size != 0 ||
        (uint64_t)offset + size > chip->size_bytes)
        return -EINVAL;
    int ret = suspend_mapped(ctx);
    for (uint32_t done = 0; ret == 0 && done < size; )
    {
        uint32_t at = offset + done;
        bool big = (chip->flags & DMSPIFLASH_CHIP_ERASE_64K) != 0 && chip->block_size != 0 &&
                   at % chip->block_size == 0 && size - done >= chip->block_size;
        ret = erase_one(ctx, at, big);
        done += big ? chip->block_size : chip->sector_size;
    }
    int resumed = resume_mapped(ctx, offset, size, true);
    return ret != 0 ? ret : resumed;
}

int dmspiflash_core_erase_chip(struct dmdrvi_context *ctx)
{
    dmspiflash_cmd_t c = cmd_plain(DMSPIFLASH_CMD_CHIP_ERASE);
    int ret = suspend_mapped(ctx);
    if (ret == 0)
        ret = write_enable(ctx);
    if (ret == 0)
        ret = dmspiflash_bus_command(ctx, &c);
    if (ret == 0)
        ret = wait_done(ctx, ctx->chip->chip_erase_ms, 1u << 5);
    int resumed = resume_mapped(ctx, 0, ctx->chip->size_bytes, true);
    return ret != 0 ? ret : resumed;
}
