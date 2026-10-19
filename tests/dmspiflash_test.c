#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmspiflash.h"

/* Host tests: pure logic only (config validation, chip table, page arithmetic).
 * Hardware is verified on the board - see docs/tasks.md. */

DMOD_TEST_STEP(validate_qspi_ok)
{
    dmspiflash_config_t c = { .bus = dmspiflash_bus_qspi, .use_quad = true, .memory_mapped = true };
    DMOD_TEST_EXPECT_TRUE(dmspiflash_validate_config(&c));
}

DMOD_TEST_STEP(validate_spi_rejects_quad)
{
    dmspiflash_config_t c = { .bus = dmspiflash_bus_spi, .use_quad = true };
    DMOD_TEST_EXPECT_FALSE(dmspiflash_validate_config(&c));
}

DMOD_TEST_STEP(find_chip_by_id)
{
    dmspiflash_jedec_id_t id = { .manufacturer = 0x20, .memory_type = 0xBA, .capacity = 0x18 };
    const dmspiflash_chip_t *chip = dmspiflash_find_chip_by_id(&id);
    DMOD_TEST_EXPECT_NOT_NULL(chip);
    DMOD_TEST_EXPECT_TRUE(chip->size_bytes == 16u * 1024u * 1024u);
}

DMOD_TEST_STEP(find_chip_unknown_id)
{
    dmspiflash_jedec_id_t id = { .manufacturer = 0xFF, .memory_type = 0xFF, .capacity = 0xFF };
    DMOD_TEST_EXPECT_NULL(dmspiflash_find_chip_by_id(&id));
}

DMOD_TEST_STEP(find_chip_by_name_case_insensitive)
{
    DMOD_TEST_EXPECT_NOT_NULL(dmspiflash_find_chip_by_name("n25q128a"));
}

DMOD_TEST_STEP(page_chunk_stops_at_page_boundary)
{
    DMOD_TEST_EXPECT_TRUE(dmspiflash_page_chunk(256, 250, 100) == 6);
    DMOD_TEST_EXPECT_TRUE(dmspiflash_page_chunk(256, 256, 1000) == 256);
    DMOD_TEST_EXPECT_TRUE(dmspiflash_page_chunk(256, 10, 5) == 5);
}
