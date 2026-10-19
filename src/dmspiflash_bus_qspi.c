#include <errno.h>
#include "dmod.h"
#include "dmspiflash_internal.h"

/*
 * Bus layer, QSPI backend (ctx->config.bus == dmspiflash_bus_qspi). NOT
 * IMPLEMENTED. Talks to /dev/dmqspi<N> (repo dmqspi) with Dmod_Ioctl:
 *   bus_open   : find the node via the friends_group, Dmod_Open it
 *   bus_command: fill dmqspi_transfer_t from dmspiflash_cmd_t (opcode on 1 line, address /
 *                data lines from the cmd, dir = read when rx != NULL, write when tx != NULL)
 *                -> DMQSPI_IOCTL_TRANSFER
 *   bus_poll   : dmqspi_auto_poll_t (status read, mask/match/timeout) -> DMQSPI_IOCTL_AUTO_POLL
 *   bus_map    : ENTER_MAPPED with the quad fast-read command / EXIT_MAPPED
 *   mapped_base: DMQSPI_IOCTL_GET_MAPPED_BASE
 * When implementing, add dmqspi to dmod_link_modules() in CMakeLists.txt (headers: dmqspi_types.h).
 *
 * Functions to write here: dmspiflash_qspi_open / _close / _command / _poll / _map /
 * _mapped_base (same arguments as dmspiflash_bus_* in dmspiflash_internal.h);
 * src/dmspiflash_bus.c dispatches to them.
 */
