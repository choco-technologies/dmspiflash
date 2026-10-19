#ifndef DMSPIFLASH_CMDS_H
#define DMSPIFLASH_CMDS_H

/*
 * Opcodes of the JEDEC-style NOR command set (Micron N25Q128A, Winbond
 * W25Q, Macronix MX25L...). Verify against the datasheet of the actual chip.
 */
#define DMSPIFLASH_CMD_READ_ID              0x9F    /**< 3 bytes: manufacturer, type, capacity */
#define DMSPIFLASH_CMD_READ_SFDP            0x5A
#define DMSPIFLASH_CMD_WRITE_ENABLE         0x06    /**< Before EVERY program / erase / write-status */
#define DMSPIFLASH_CMD_WRITE_DISABLE        0x04
#define DMSPIFLASH_CMD_READ_STATUS          0x05    /**< Bit 0 WIP, bit 1 WEL */
#define DMSPIFLASH_CMD_WRITE_STATUS         0x01
#define DMSPIFLASH_CMD_READ_FLAG_STATUS     0x70    /**< Micron: bit 7 ready, bit 5 erase err, bit 4 program err */
#define DMSPIFLASH_CMD_CLEAR_FLAG_STATUS    0x50
#define DMSPIFLASH_CMD_READ_ENH_VOLATILE    0x65    /**< Micron */
#define DMSPIFLASH_CMD_WRITE_ENH_VOLATILE   0x61
#define DMSPIFLASH_CMD_READ_VOLATILE_CFG    0x85
#define DMSPIFLASH_CMD_WRITE_VOLATILE_CFG   0x81
#define DMSPIFLASH_CMD_READ                 0x03    /**< Slow read, 1 line, no dummy */
#define DMSPIFLASH_CMD_FAST_READ            0x0B    /**< 1 line, 8 dummy cycles */
#define DMSPIFLASH_CMD_FAST_READ_QUAD_OUT   0x6B
#define DMSPIFLASH_CMD_FAST_READ_QUAD_IO    0xEB    /**< Address on 4 lines, alternate byte, dummy cycles */
#define DMSPIFLASH_CMD_PAGE_PROGRAM         0x02
#define DMSPIFLASH_CMD_QUAD_PAGE_PROGRAM    0x32
#define DMSPIFLASH_CMD_ERASE_4K             0x20
#define DMSPIFLASH_CMD_ERASE_64K            0xD8
#define DMSPIFLASH_CMD_CHIP_ERASE           0xC7
#define DMSPIFLASH_CMD_RESET_ENABLE         0x66
#define DMSPIFLASH_CMD_RESET_MEMORY         0x99

#define DMSPIFLASH_STATUS_WIP               (1u << 0)
#define DMSPIFLASH_STATUS_WEL               (1u << 1)

#endif // DMSPIFLASH_CMDS_H
