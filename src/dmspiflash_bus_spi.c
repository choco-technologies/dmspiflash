#include <errno.h>
#include "dmod.h"
#include "dmspiflash_internal.h"

/*
 * Bus layer, plain SPI backend (ctx->config.bus == dmspiflash_bus_spi). NOT
 * IMPLEMENTED. Talks to /dev/dmspi<N> (repo dmspi) and a chip-select GPIO:
 *   bus_open   : find the SPI node and the chip_select friend through the friends_group
 *   bus_command: CS low, write [opcode][address bytes][alternate][dummy bytes], then
 *                read or write the data (dmspi is full duplex: send 0xFF while reading),
 *                CS high (the CS ioctl of dmgpio, as dmspi does). 1 line only:
 *                a cmd with lines other than 1/none -> -ENOTSUP. Dummy CYCLES become
 *                dummy BYTES (cycles / 8, the chip's plain-SPI fast read has 8).
 *   bus_poll   : loop READ_STATUS with a short sleep until the match or timeout (-ETIMEDOUT)
 *   bus_map    : -ENOTSUP
 * When implementing, add dmspi to dmod_link_modules() in CMakeLists.txt.
 *
 * Functions to write here: dmspiflash_spi_open / _close / _command / _poll / _map /
 * _mapped_base (same arguments as dmspiflash_bus_* in dmspiflash_internal.h);
 * src/dmspiflash_bus.c dispatches to them.
 */
