#include <errno.h>
#include <string.h>
#include "dmod.h"
#include "dmspiflash_internal.h"
#include "dmqspi_types.h"

/*
 * Bus layer, QSPI backend: the flash behind a dmqspi bus node
 * (/dev/dmqspix/qspi). A flash command becomes one DMQSPI_IOCTL_TRANSFER;
 * waiting for the chip is the controller's hardware auto-poll.
 */

static dmqspi_command_t to_qspi(const struct dmdrvi_context *ctx, const dmspiflash_cmd_t *c)
{
    dmqspi_command_t q;
    memset(&q, 0, sizeof(q));
    q.instruction = c->opcode;
    q.instruction_lines = dmqspi_lines_1;
    if (c->has_address)
    {
        q.address = c->address;
        q.address_lines = (dmqspi_lines_t)c->address_lines;
        q.address_size = (ctx->chip != NULL && ctx->chip->size_bytes > (16u << 20)) ? dmqspi_addr_32bit : dmqspi_addr_24bit;
    }
    if (c->has_alternate)
    {
        q.alternate = c->alternate;
        q.alternate_lines = q.address_lines;
        q.alternate_size = 1;
    }
    q.dummy_cycles = c->dummy_cycles;
    q.data_lines = (c->tx != NULL || c->rx != NULL) ? (dmqspi_lines_t)c->data_lines : dmqspi_lines_none;
    q.data_size = (c->tx != NULL || c->rx != NULL) ? c->size : 0;
    return q;
}

int dmspiflash_qspi_open(struct dmdrvi_context *ctx)
{
    if (ctx->bus_path == NULL)
        return -ENODEV;
    ctx->bus_handle = Dmod_FileOpen(ctx->bus_path, "r+");
    return ctx->bus_handle != NULL ? 0 : -ENODEV;
}

void dmspiflash_qspi_close(struct dmdrvi_context *ctx)
{
    if (ctx->bus_handle != NULL)
        Dmod_FileClose(ctx->bus_handle);
    ctx->bus_handle = NULL;
}

int dmspiflash_qspi_command(struct dmdrvi_context *ctx, const dmspiflash_cmd_t *cmd)
{
    dmqspi_transfer_t t;
    memset(&t, 0, sizeof(t));
    t.cmd = to_qspi(ctx, cmd);
    if (cmd->rx != NULL)
    {
        t.dir = dmqspi_dir_read;
        t.data = cmd->rx;
    }
    else if (cmd->tx != NULL)
    {
        t.dir = dmqspi_dir_write;
        t.data = (void *)cmd->tx;
    }
    return Dmod_Ioctl(ctx->bus_handle, DMQSPI_IOCTL_TRANSFER, &t);
}

int dmspiflash_qspi_poll(struct dmdrvi_context *ctx, uint8_t status_opcode, uint32_t mask, uint32_t match, uint32_t timeout_ms)
{
    dmqspi_auto_poll_t p;
    memset(&p, 0, sizeof(p));
    p.cmd.instruction = status_opcode;
    p.cmd.instruction_lines = dmqspi_lines_1;
    p.cmd.data_lines = dmqspi_lines_1;
    p.cmd.data_size = 1;
    p.poll.mask = mask;
    p.poll.match = match;
    p.poll.timeout_ms = timeout_ms;
    return Dmod_Ioctl(ctx->bus_handle, DMQSPI_IOCTL_AUTO_POLL, &p);
}

int dmspiflash_qspi_map(struct dmdrvi_context *ctx, bool enable)
{
    if (!enable)
        return Dmod_Ioctl(ctx->bus_handle, DMQSPI_IOCTL_EXIT_MAPPED, NULL);
    dmspiflash_cmd_t read = { .opcode = ctx->config.use_quad ? DMSPIFLASH_CMD_FAST_READ_QUAD_IO : DMSPIFLASH_CMD_FAST_READ,
                              .has_address = true,
                              .address_lines = ctx->config.use_quad ? dmspiflash_lines_4 : dmspiflash_lines_1,
                              .dummy_cycles = ctx->config.use_quad ? ctx->chip->read_dummy_cycles : 8,
                              .data_lines = ctx->config.use_quad ? dmspiflash_lines_4 : dmspiflash_lines_1,
                              .size = 1, .rx = (void *)1 };
    dmqspi_command_t q = to_qspi(ctx, &read);
    return Dmod_Ioctl(ctx->bus_handle, DMQSPI_IOCTL_ENTER_MAPPED, &q);
}

uintptr_t dmspiflash_qspi_mapped_base(struct dmdrvi_context *ctx)
{
    uintptr_t base = 0;
    return Dmod_Ioctl(ctx->bus_handle, DMQSPI_IOCTL_GET_MAPPED_BASE, &base) == 0 ? base : 0;
}

void dmspiflash_qspi_invalidate(struct dmdrvi_context *ctx, uint32_t offset, uint32_t size)
{
    uint32_t range[2] = { offset, size };
    (void)Dmod_Ioctl(ctx->bus_handle, DMQSPI_IOCTL_INVALIDATE_MAPPED, range);
}
