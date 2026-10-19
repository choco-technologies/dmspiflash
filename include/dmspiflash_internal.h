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
struct dmdrvi_context
{
    uint32_t                magic;
    dmspiflash_config_t     config;
    const dmspiflash_chip_t *chip;          /**< Row of the chip table, set by dmspiflash_core_open */
    void                   *bus_handle;     /**< Handle of the bus node (Dmod_Open) */
    char                   *bus_path;       /**< Heap string: "/dev/dmqspi1" - found through the friends_group */
    char                   *cs_path;        /**< SPI only: chip-select GPIO node, or NULL */
    void                   *lock;           /**< dmosi mutex: one flash operation at a time */
    bool                    mapped;         /* memory-mapped mode is on (qspi) */
    bool                    ready;          /* bus opened and the chip identified */
};

#define DMSPIFLASH_MAGIC    0x44534650u     /* "DSFP" */

/* --- bus layer (qspi / spi implementations; both return 0 or -errno) --- */
int  dmspiflash_bus_open(struct dmdrvi_context *ctx);                       /**< Find + open the bus node */
void dmspiflash_bus_close(struct dmdrvi_context *ctx);
int  dmspiflash_bus_command(struct dmdrvi_context *ctx, const dmspiflash_cmd_t *cmd);
/** Poll status (cmd = READ_STATUS) until (status & mask) == match or timeout_ms. */
int  dmspiflash_bus_poll(struct dmdrvi_context *ctx, uint8_t status_opcode, uint32_t mask, uint32_t match, uint32_t timeout_ms);
int  dmspiflash_bus_map(struct dmdrvi_context *ctx, bool enable);           /**< qspi only; spi returns -ENOTSUP */
uintptr_t dmspiflash_bus_mapped_base(struct dmdrvi_context *ctx);           /**< 0 when not mapped */
void dmspiflash_bus_invalidate(struct dmdrvi_context *ctx, uint32_t offset, uint32_t size); /**< D-cache of the mapped window */

/* --- flash core --- */
int  dmspiflash_core_open(struct dmdrvi_context *ctx);                      /**< reset, read id, find chip, enable quad */
int  dmspiflash_core_read(struct dmdrvi_context *ctx, uint32_t offset, void *buffer, size_t size);
int  dmspiflash_core_write(struct dmdrvi_context *ctx, uint32_t offset, const void *buffer, size_t size);
int  dmspiflash_core_erase(struct dmdrvi_context *ctx, uint32_t offset, uint32_t size);
int  dmspiflash_core_erase_chip(struct dmdrvi_context *ctx);

#endif // DMSPIFLASH_INTERNAL_H
