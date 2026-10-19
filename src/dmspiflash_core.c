#include <errno.h>
#include "dmod.h"
#include "dmspiflash.h"
#include "dmspiflash_internal.h"

/*
 * Flash core - chip logic on top of the bus layer. NOT IMPLEMENTED: every
 * function is a stub (docs/tasks.md, steps 2-3; algorithms in docs/architecture.md).
 */

int dmspiflash_core_open(struct dmspiflash *ctx)
{
    /* TODO: reset (0x66,0x99, wait ~1 ms) -> READ ID 0x9F (always 1 line) ->
     *       chip = find_chip_by_name(config.chip_name) or find_chip_by_id (-ENODEV if unknown) ->
     *       if use_quad: enable per chip->quad_enable, set dummy cycles ->
     *       if memory_mapped: dmspiflash_bus_map(ctx, true). */
    (void)ctx;
    return -ENOSYS;
}

int dmspiflash_core_read(struct dmspiflash *ctx, uint32_t offset, void *buffer, size_t size)
{
    /* TODO: bounds check; leave memory-mapped if on (re-enter after); lock; one
     *       0xEB (quad) or 0x0B (1 line) command with data size = size; returns bytes. */
    (void)ctx; (void)offset; (void)buffer; (void)size;
    return -ENOSYS;
}

int dmspiflash_core_write(struct dmspiflash *ctx, uint32_t offset, const void *buffer, size_t size)
{
    /* TODO: bounds check; lock; per chunk (dmspiflash_page_chunk): WREN, 0x32 or 0x02,
     *       bus_poll WIP==0 within chip->page_program_ms; if FLAG_STATUS chip: read 0x70,
     *       bit 4 set -> clear 0x50 and return -EIO; invalidate mapped cache; returns bytes. */
    (void)ctx; (void)offset; (void)buffer; (void)size;
    return -ENOSYS;
}

int dmspiflash_core_erase(struct dmspiflash *ctx, uint32_t offset, uint32_t size)
{
    /* TODO: offset and size multiples of chip->sector_size (-EINVAL); lock; per 4 KB
     *       sector WREN + 0x20 + poll WIP (chip->sector_erase_ms); an aligned 64 KB run may
     *       use 0xD8; check flag status bit 5 on Micron. */
    (void)ctx; (void)offset; (void)size;
    return -ENOSYS;
}

int dmspiflash_core_erase_chip(struct dmspiflash *ctx)
{
    /* TODO: WREN + 0xC7 + poll WIP with chip->chip_erase_ms. */
    (void)ctx;
    return -ENOSYS;
}
