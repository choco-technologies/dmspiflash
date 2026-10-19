#define DMOD_ENABLE_REGISTRATION ON
#include <errno.h>
#include "dmod.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmspiflash.h"
#include "dmspiflash_internal.h"

/*
 * Module entry, pure helpers and the dmdrvi driver ("driver_name=dmspiflash"
 * in the board .ini). Registration (DMOD_ENABLE_REGISTRATION) lives only in this file.
 *
 * The dmdrvi functions are NOT IMPLEMENTED yet (docs/tasks.md, step 4).
 * Copy the structure of dmsdio/src/dmsdio.c (block device) and
 * dmspi/src/dmspi.c (friends). What each must do:
 *
 *   _create   : read keys (docs/configuration.md) -> dmspiflash_config_t,
 *               dmspiflash_validate_config; allocate the context (magic guard,
 *               Dmod_Malloc); find the bus node through the friends_group
 *               (DMDRVI_IOCTL_DEVFS_GET_FRIEND) and wait for it in
 *               _friend_changed; dmspiflash_bus_open; dmspiflash_core_open.
 *               dev_num minor = index of the flash. Node: /dev/dmspiflash<N>.
 *   _free     : leave memory-mapped mode, dmspiflash_bus_close, free
 *   _open/_close/_flush/_stat : trivial (no per-handle state; flush = no-op,
 *               writes finish before returning)
 *   _read     : dmspiflash_core_read(offset, buf, size) - byte offsets
 *   _write    : dmspiflash_core_write - area must be erased (block-device contract)
 *   _ioctl    : DMDRVI_IOCTL_BLOCK_GET_INFO -> logical_block_size 512,
 *                   erase_block_size = chip->sector_size, block_count = size/512,
 *                   flags = DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED;
 *               DMDRVI_IOCTL_BLOCK_ERASE -> dmspiflash_core_erase (aligned, else -EINVAL);
 *               DMSPIFLASH_IOCTL_* -> see dmspiflash_types.h.
 */

dmod_dmspiflash_api_declaration(1.0, bool, _validate_config, ( const dmspiflash_config_t *config ))
{
    if (config == NULL)
        return false;
    if (config->bus != dmspiflash_bus_spi && config->bus != dmspiflash_bus_qspi)
        return false;
    if (config->bus == dmspiflash_bus_spi && (config->use_quad || config->memory_mapped))
        return false;       /* quad and XIP need the qspi bus */
    return true;
}

dmod_dmspiflash_api_declaration(1.0, uint32_t, _page_chunk, ( uint32_t page_size, uint32_t offset, uint32_t remaining ))
{
    if (page_size == 0)
        return 0;
    uint32_t room = page_size - (offset % page_size);
    return remaining < room ? remaining : room;
}

int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    Dmod_Printf("dmspiflash: skeleton, not implemented yet\n");
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
