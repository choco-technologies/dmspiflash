#ifndef DMSPIFLASH_TYPES_H
#define DMSPIFLASH_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmdrvi_ioctl.h"

/* Types of dmspiflash: a NOR flash chip behind a bus (SPI or QSPI). See docs/architecture.md. */

/** Which bus driver the chip is behind. */
typedef enum
{
    dmspiflash_bus_spi = 0,     /**< plain SPI: node of dmspi (+ a chip-select GPIO friend) */
    dmspiflash_bus_qspi,        /**< quad SPI: node of dmqspi (DMQSPI_IOCTL_*) */
} dmspiflash_bus_kind_t;

/** How the chip is switched to 4-line mode (differs per vendor). */
typedef enum
{
    dmspiflash_quad_none = 0,       /**< No quad mode / always on (1-line operation only) */
    dmspiflash_quad_micron_vcr,     /**< Micron N25Q: volatile (0x81) + enhanced volatile (0x61) config registers */
    dmspiflash_quad_status_qe,      /**< Winbond/GigaDevice/ISSI: QE bit in the status register (0x01/0x31) */
    dmspiflash_quad_macronix_qe,    /**< Macronix: QE bit in status register 1 */
} dmspiflash_quad_enable_t;

/* dmspiflash_chip_t.flags */
#define DMSPIFLASH_CHIP_ERASE_4K        (1u << 0)   /**< Opcode 0x20 works */
#define DMSPIFLASH_CHIP_ERASE_64K       (1u << 1)   /**< Opcode 0xD8 works */
#define DMSPIFLASH_CHIP_QUAD_PROGRAM    (1u << 2)   /**< Opcode 0x32 works */
#define DMSPIFLASH_CHIP_FLAG_STATUS     (1u << 3)   /**< Micron flag status register (0x70) reports program/erase errors */

/** JEDEC identification (opcode 0x9F, 3 bytes). */
typedef struct
{
    uint8_t manufacturer;   /**< 0x20 = Micron, 0xEF = Winbond, 0xC2 = Macronix */
    uint8_t memory_type;
    uint8_t capacity;
} dmspiflash_jedec_id_t;

/**
 * One row of the chip table (src/dmspiflash_chips.c). Plain data, no
 * pointers: a module cannot hold const data that needs relocation.
 */
typedef struct
{
    char                        name[16];           /**< "N25Q128A" */
    dmspiflash_jedec_id_t       id;
    uint32_t                    size_bytes;         /**< 16 MiB */
    uint32_t                    page_size;          /**< Program unit, 256 */
    uint32_t                    sector_size;        /**< Smallest erase unit, 4096 */
    uint32_t                    block_size;         /**< Large erase unit, 65536 (0 = none) */
    dmspiflash_quad_enable_t    quad_enable;
    uint8_t                     read_dummy_cycles;  /**< For fast read quad I/O (0xEB) at the configured clock */
    uint32_t                    flags;              /**< DMSPIFLASH_CHIP_* */
    uint32_t                    page_program_ms;    /**< Worst-case timeouts from the datasheet */
    uint32_t                    sector_erase_ms;
    uint32_t                    chip_erase_ms;
} dmspiflash_chip_t;

/** Configuration read from the .ini by the dmdrvi _create (docs/configuration.md). */
typedef struct
{
    dmspiflash_bus_kind_t   bus;
    char                    chip_name[16];  /**< Empty = identify by JEDEC id; else force this row, skip the id check */
    bool                    use_quad;       /**< Quad read/program when bus and chip allow (qspi only) */
    bool                    memory_mapped;  /**< Enter memory-mapped mode after open (qspi only) */
} dmspiflash_config_t;

/** Driver-specific ioctl commands (numbered from DMDRVI_IOCTL_CUSTOM_BASE). Block ioctls are the standard DMDRVI_IOCTL_BLOCK_*. */
#define DMSPIFLASH_IOCTL_GET_JEDEC_ID       (DMDRVI_IOCTL_CUSTOM_BASE + 0)  /**< arg: dmspiflash_jedec_id_t* */
#define DMSPIFLASH_IOCTL_GET_CHIP           (DMDRVI_IOCTL_CUSTOM_BASE + 1)  /**< arg: dmspiflash_chip_t* (a copy) */
#define DMSPIFLASH_IOCTL_ERASE_CHIP         (DMDRVI_IOCTL_CUSTOM_BASE + 2)  /**< arg: none (takes seconds) */
#define DMSPIFLASH_IOCTL_SET_MEMORY_MAPPED  (DMDRVI_IOCTL_CUSTOM_BASE + 3)  /**< arg: const bool* (qspi only) */
#define DMSPIFLASH_IOCTL_GET_MAPPED_BASE    (DMDRVI_IOCTL_CUSTOM_BASE + 4)  /**< arg: uintptr_t*, -ENOTSUP when not mapped */

#endif // DMSPIFLASH_TYPES_H
