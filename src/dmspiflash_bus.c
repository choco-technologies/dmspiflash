#include <errno.h>
#include "dmod.h"
#include "dmspiflash_internal.h"

/*
 * Bus dispatcher. NOT IMPLEMENTED: stubs. Each function must switch on
 * ctx->config.bus and call dmspiflash_qspi_<name> (src/dmspiflash_bus_qspi.c)
 * or dmspiflash_spi_<name> (src/dmspiflash_bus_spi.c); an unknown value is -EINVAL.
 */

int dmspiflash_bus_open(struct dmspiflash *ctx)
{
    (void)ctx;
    return -ENOSYS;
}

void dmspiflash_bus_close(struct dmspiflash *ctx)
{
    (void)ctx;
}

int dmspiflash_bus_command(struct dmspiflash *ctx, const dmspiflash_cmd_t *cmd)
{
    (void)ctx; (void)cmd;
    return -ENOSYS;
}

int dmspiflash_bus_poll(struct dmspiflash *ctx, uint8_t status_opcode, uint32_t mask, uint32_t match, uint32_t timeout_ms)
{
    (void)ctx; (void)status_opcode; (void)mask; (void)match; (void)timeout_ms;
    return -ENOSYS;
}

int dmspiflash_bus_map(struct dmspiflash *ctx, bool enable)
{
    (void)ctx; (void)enable;
    return -ENOTSUP;
}

uintptr_t dmspiflash_bus_mapped_base(struct dmspiflash *ctx)
{
    (void)ctx;
    return 0;
}
