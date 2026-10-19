#include "dmspiflash.h"

/*
 * Table of known flash chips. dmspiflash_core_open matches the JEDEC id read
 * from the chip against it (or the `chip=` name from the .ini). Adding a
 * chip = adding a row (+ a line in docs/chips.md); a chip whose quad-enable
 * procedure is new needs a new dmspiflash_quad_enable_t value and a branch
 * in the core. Values below: confirm against the datasheet.
 */
static const dmspiflash_chip_t g_chips[] =
{
    /* STM32F746G-DISCO: Micron N25Q128A, 3 V, 128 Mbit */
    {
        .name = "N25Q128A",
        .id = { .manufacturer = 0x20, .memory_type = 0xBA, .capacity = 0x18 },
        .size_bytes = 16u * 1024u * 1024u,
        .page_size = 256,
        .sector_size = 4096,
        .block_size = 65536,
        .quad_enable = dmspiflash_quad_micron_vcr,
        .read_dummy_cycles = 10,        /* TODO: verify for 0xEB at the chosen clock */
        .flags = DMSPIFLASH_CHIP_ERASE_4K | DMSPIFLASH_CHIP_ERASE_64K |
                 DMSPIFLASH_CHIP_QUAD_PROGRAM | DMSPIFLASH_CHIP_FLAG_STATUS,
        .page_program_ms = 5,           /* TODO: datasheet worst case */
        .sector_erase_ms = 800,
        .chip_erase_ms = 250000,
    },
};

#define CHIPS_COUNT (sizeof(g_chips) / sizeof(g_chips[0]))

static char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

dmod_dmspiflash_api_declaration(1.0, const dmspiflash_chip_t*, _find_chip_by_id, ( const dmspiflash_jedec_id_t *id ))
{
    if (id == NULL)
        return NULL;
    for (size_t i = 0; i < CHIPS_COUNT; i++)
    {
        if (g_chips[i].id.manufacturer == id->manufacturer &&
            g_chips[i].id.memory_type == id->memory_type &&
            g_chips[i].id.capacity == id->capacity)
            return &g_chips[i];
    }
    return NULL;
}

dmod_dmspiflash_api_declaration(1.0, const dmspiflash_chip_t*, _find_chip_by_name, ( const char *name ))
{
    if (name == NULL)
        return NULL;
    for (size_t i = 0; i < CHIPS_COUNT; i++)
    {
        const char *a = g_chips[i].name;
        const char *b = name;
        while (*a != '\0' && lower(*a) == lower(*b)) { a++; b++; }
        if (*a == '\0' && *b == '\0')
            return &g_chips[i];
    }
    return NULL;
}
