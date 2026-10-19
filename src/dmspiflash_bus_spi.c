#include <errno.h>
#include "dmod.h"
#include "dmspiflash_internal.h"

/*
 * Bus layer, plain SPI backend. NOT IMPLEMENTED (no board with SPI flash yet) -
 * every function says -ENOTSUP, dmspiflash_validate_config accepts bus=spi.
 * To implement, against /dev/dmspi<N> (repo dmspi) and its chip-select GPIO friend:
 *   open    : Dmod_FileOpen the SPI node; find the chip_select friend (_friend_changed)
 *   command : CS low, write [opcode][address bytes][dummy BYTES = cycles/8], read or write the
 *             data (dmspi is full duplex: send 0xFF while reading), CS high; 1 line only
 *             (lines other than 1/none -> -ENOTSUP)
 *   poll    : loop READ_STATUS with dmosi_thread_sleep(1) until the match or timeout (-ETIMEDOUT)
 *   map     : -ENOTSUP
 */

int dmspiflash_spi_open(struct dmdrvi_context *ctx) { (void)ctx; return -ENOTSUP; }
void dmspiflash_spi_close(struct dmdrvi_context *ctx) { (void)ctx; }
int dmspiflash_spi_command(struct dmdrvi_context *ctx, const dmspiflash_cmd_t *cmd) { (void)ctx; (void)cmd; return -ENOTSUP; }
int dmspiflash_spi_poll(struct dmdrvi_context *ctx, uint8_t op, uint32_t mask, uint32_t match, uint32_t timeout_ms)
{
    (void)ctx; (void)op; (void)mask; (void)match; (void)timeout_ms;
    return -ENOTSUP;
}
