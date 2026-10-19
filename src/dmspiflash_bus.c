#include <errno.h>
#include "dmod.h"
#include "dmspiflash_internal.h"

/* Bus dispatcher: a switch on ctx->config.bus (no table of function pointers - see dmspiflash_internal.h). */

int dmspiflash_qspi_open(struct dmdrvi_context *ctx);
void dmspiflash_qspi_close(struct dmdrvi_context *ctx);
int dmspiflash_qspi_command(struct dmdrvi_context *ctx, const dmspiflash_cmd_t *cmd);
int dmspiflash_qspi_poll(struct dmdrvi_context *ctx, uint8_t op, uint32_t mask, uint32_t match, uint32_t timeout_ms);
int dmspiflash_qspi_map(struct dmdrvi_context *ctx, bool enable);
uintptr_t dmspiflash_qspi_mapped_base(struct dmdrvi_context *ctx);
void dmspiflash_qspi_invalidate(struct dmdrvi_context *ctx, uint32_t offset, uint32_t size);
int dmspiflash_spi_open(struct dmdrvi_context *ctx);
void dmspiflash_spi_close(struct dmdrvi_context *ctx);
int dmspiflash_spi_command(struct dmdrvi_context *ctx, const dmspiflash_cmd_t *cmd);
int dmspiflash_spi_poll(struct dmdrvi_context *ctx, uint8_t op, uint32_t mask, uint32_t match, uint32_t timeout_ms);

#define IS_QSPI(ctx)    ((ctx)->config.bus == dmspiflash_bus_qspi)

int dmspiflash_bus_open(struct dmdrvi_context *ctx)
{
    return IS_QSPI(ctx) ? dmspiflash_qspi_open(ctx) : dmspiflash_spi_open(ctx);
}

void dmspiflash_bus_close(struct dmdrvi_context *ctx)
{
    if (IS_QSPI(ctx)) dmspiflash_qspi_close(ctx); else dmspiflash_spi_close(ctx);
}

int dmspiflash_bus_command(struct dmdrvi_context *ctx, const dmspiflash_cmd_t *cmd)
{
    return IS_QSPI(ctx) ? dmspiflash_qspi_command(ctx, cmd) : dmspiflash_spi_command(ctx, cmd);
}

int dmspiflash_bus_poll(struct dmdrvi_context *ctx, uint8_t op, uint32_t mask, uint32_t match, uint32_t timeout_ms)
{
    return IS_QSPI(ctx) ? dmspiflash_qspi_poll(ctx, op, mask, match, timeout_ms)
                        : dmspiflash_spi_poll(ctx, op, mask, match, timeout_ms);
}

int dmspiflash_bus_map(struct dmdrvi_context *ctx, bool enable)
{
    return IS_QSPI(ctx) ? dmspiflash_qspi_map(ctx, enable) : -ENOTSUP;
}

uintptr_t dmspiflash_bus_mapped_base(struct dmdrvi_context *ctx)
{
    return IS_QSPI(ctx) ? dmspiflash_qspi_mapped_base(ctx) : 0;
}

void dmspiflash_bus_invalidate(struct dmdrvi_context *ctx, uint32_t offset, uint32_t size)
{
    if (IS_QSPI(ctx)) dmspiflash_qspi_invalidate(ctx, offset, size);
}
