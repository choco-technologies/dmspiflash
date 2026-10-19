#ifndef DMSPIFLASH_INTERNAL_H
#define DMSPIFLASH_INTERNAL_H

/*
 * Internal interface between the files of this module (NOT a dmod API, not
 * exported). Two layers:
 *
 *   flash core  (src/dmspiflash_core.c)  - chip logic: open/identify, read, program, erase
 *   bus layer   (src/dmspiflash_bus_qspi.c, src/dmspiflash_bus_spi.c) - how one
 *               flash command reaches the chip over a given bus
 *
 * The core builds a dmspiflash_cmd_t and hands it to the bus layer. The bus
 * layer dispatches on ctx->config.bus with a plain switch (no table of
 * function pointers: such const data would need relocation, which a module
 * cannot have).
 */

#include "dmspiflash_types.h"
#include "dmspiflash_cmds.h"

/** Lines used by a phase (maps to 1/2/4 on QSPI; on plain SPI only `1` or `none` are valid). */
typedef enum { dmspiflash_lines_none = 0, dmspiflash_lines_1, dmspiflash_lines_2, dmspiflash_lines_4 } dmspiflash_lines_t;

/** One flash command in the bus-independent form. */
typedef struct
{
    uint8_t             opcode;
    bool                has_address;
    uint32_t            address;            /**< 24 bit for 16 MB parts */
    dmspiflash_lines_t  address_lines;
    bool                has_alternate;      /**< "mode" byte (0xEB continuous read) */
    uint8_t             alternate;
    uint8_t             dummy_cycles;
    dmspiflash_lines_t  data_lines;
    const void         *tx;                 /**< data to write (or NULL) */
    void               *rx;                 /**< data to read (or NULL) */
    uint32_t            size;               /**< data bytes */
} dmspiflash_cmd_t;

/** The opened device. Allocated with Dmod_Malloc in _create, magic-guarded. */
struct dmspiflash
{
    uint32_t                magic;
    dmspiflash_config_t     config;
    const dmspiflash_chip_t *chip;          /**< Row of the chip table, set by dmspiflash_core_open */
    void                   *bus_handle;     /**< Handle of the bus node (Dmod_Open) */
    char                   *bus_path;       /**< Heap string: "/dev/dmqspi1" - found through the friends_group */
    char                   *cs_path;        /**< SPI only: chip-select GPIO node, or NULL */
    void                   *lock;           /**< dmosi mutex: one flash operation at a time */
    bool                    mapped;
};

/* --- bus layer (qspi / spi implementations; both return 0 or -errno) --- */
int  dmspiflash_bus_open(struct dmspiflash *ctx);                       /**< Find + open the bus node */
void dmspiflash_bus_close(struct dmspiflash *ctx);
int  dmspiflash_bus_command(struct dmspiflash *ctx, const dmspiflash_cmd_t *cmd);
/** Poll status (cmd = READ_STATUS) until (status & mask) == match or timeout_ms. */
int  dmspiflash_bus_poll(struct dmspiflash *ctx, uint8_t status_opcode, uint32_t mask, uint32_t match, uint32_t timeout_ms);
int  dmspiflash_bus_map(struct dmspiflash *ctx, bool enable);           /**< qspi only; spi returns -ENOTSUP */
uintptr_t dmspiflash_bus_mapped_base(struct dmspiflash *ctx);           /**< 0 when not mapped */

/* --- flash core --- */
int  dmspiflash_core_open(struct dmspiflash *ctx);                      /**< reset, read id, find chip, enable quad */
int  dmspiflash_core_read(struct dmspiflash *ctx, uint32_t offset, void *buffer, size_t size);
int  dmspiflash_core_write(struct dmspiflash *ctx, uint32_t offset, const void *buffer, size_t size);
int  dmspiflash_core_erase(struct dmspiflash *ctx, uint32_t offset, uint32_t size);
int  dmspiflash_core_erase_chip(struct dmspiflash *ctx);

#endif // DMSPIFLASH_INTERNAL_H
