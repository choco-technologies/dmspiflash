#ifndef DMSPIFLASH_H
#define DMSPIFLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmod_types.h"
#include "dmspiflash_defs.h"
#include "dmspiflash_types.h"

/*
 * dmspiflash = NOR flash chip driver. It sits on a bus node (/dev/dmqspi<N>
 * or /dev/dmspi<N>) and exports a dmdrvi BLOCK DEVICE /dev/dmspiflash<N>.
 * Clients (littlefs, automount, shell) use the node; they do not call this
 * header. What is exported here are the pure helpers (no hardware), so they
 * can be unit-tested on the host and reused by tools.
 * See docs/architecture.md.
 */

/** Check a configuration without touching hardware. */
dmod_dmspiflash_api(1.0, bool, _validate_config, ( const dmspiflash_config_t *config ));

/** Find a chip-table row by JEDEC id. NULL = unknown chip. Valid for the module's lifetime. */
dmod_dmspiflash_api(1.0, const dmspiflash_chip_t*, _find_chip_by_id, ( const dmspiflash_jedec_id_t *id ));

/** Find a chip-table row by name (case-insensitive), for `chip=` in the .ini. NULL = unknown. */
dmod_dmspiflash_api(1.0, const dmspiflash_chip_t*, _find_chip_by_name, ( const char *name ));

/**
 * Size of the next program chunk: the bytes that fit in the page that
 * `offset` is in, capped at `remaining` (page program wraps inside a page,
 * it must never cross a boundary). Pure arithmetic.
 */
dmod_dmspiflash_api(1.0, uint32_t, _page_chunk, ( uint32_t page_size, uint32_t offset, uint32_t remaining ));

#endif // DMSPIFLASH_H
