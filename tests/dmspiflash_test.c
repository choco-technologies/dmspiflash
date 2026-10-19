#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmspiflash.h"

static dmspiflash_t g_handle = NULL;

void dmod_test_setup(void)
{
    g_handle = dmspiflash_create();
}

void dmod_test_teardown(void)
{
    dmspiflash_destroy(g_handle);
    g_handle = NULL;
}

DMOD_TEST_STEP(dmspiflash_create)
{
    DMOD_TEST_EXPECT_NOT_NULL(g_handle);
}

DMOD_TEST_STEP(dmspiflash_is_valid)
{
    DMOD_TEST_EXPECT_TRUE(dmspiflash_is_valid(g_handle));
}

DMOD_TEST_STEP(dmspiflash_destroy_null)
{
    /* Destroying NULL must not crash. */
    dmspiflash_destroy(NULL);
}
