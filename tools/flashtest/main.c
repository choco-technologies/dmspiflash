#include "dmod.h"
#include "dmspiflash_types.h"
#include <string.h>

/*
 * flashtest <node> [--xip] - exercises the dmspiflash block device, e.g. /dev/dmspiflashx/flash.
 * Works on the LAST 4 KiB sector of the chip: its content is saved first and restored at the end
 * (a failure in between can still leave it erased). Exit code = number of failed steps.
 */

#define SECTOR      4096u
#define TEST_OFFSET 0x00FFF000u

static int g_failed;

static void check(bool ok, const char *what)
{
    Dmod_Printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        g_failed++;
}

static int node_read(void *fp, uint32_t offset, void *buf, uint32_t size)
{
    if (Dmod_FileSeek(fp, (Dmod_FileOffset_t)offset, 0) != 0)
        return -1;
    return Dmod_FileRead(buf, 1, size, fp) == size ? 0 : -1;
}

static int node_write(void *fp, uint32_t offset, const void *buf, uint32_t size)
{
    if (Dmod_FileSeek(fp, (Dmod_FileOffset_t)offset, 0) != 0)
        return -1;
    return Dmod_FileWrite(buf, 1, size, fp) == size ? 0 : -1;
}

static int node_erase(void *fp, uint32_t offset, uint32_t size)
{
    dmdrvi_block_range_t r = { .offset = offset, .length = size };
    return Dmod_Ioctl(fp, DMDRVI_IOCTL_BLOCK_ERASE, &r);
}

static bool all_ff(const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (p[i] != 0xFF)
            return false;
    return true;
}

static void test_info(void *fp)
{
    dmdrvi_block_info_t info;
    memset(&info, 0, sizeof(info));
    int ret = Dmod_Ioctl(fp, DMDRVI_IOCTL_BLOCK_GET_INFO, &info);
    Dmod_Printf("block info: ret=%d logical=%u erase=%u blocks=%u flags=0x%x\n", ret,
                (unsigned)info.logical_block_size, (unsigned)info.erase_block_size,
                (unsigned)info.block_count, (unsigned)info.flags);
    check(ret == 0 && info.logical_block_size == 512 && info.erase_block_size == 4096 &&
          (uint64_t)info.block_count * 512u == 16u * 1024u * 1024u &&
          (info.flags & DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED) != 0, "block geometry of a 16 MiB flash with 4 KiB erase");
    dmspiflash_chip_t chip;
    check(Dmod_Ioctl(fp, DMSPIFLASH_IOCTL_GET_CHIP, &chip) == 0 && chip.id.manufacturer == 0x20, "chip description (Micron)");
}

static void test_erase_write(void *fp, uint8_t *buf, uint8_t *buf2)
{
    check(node_erase(fp, TEST_OFFSET, SECTOR) == 0, "erase the sector (block ioctl)");
    check(node_read(fp, TEST_OFFSET, buf2, SECTOR) == 0 && all_ff(buf2, SECTOR), "erased sector reads 0xFF");
    check(node_erase(fp, TEST_OFFSET + 1, SECTOR) != 0, "unaligned erase is refused");

    uint8_t pattern[300];
    for (unsigned i = 0; i < sizeof(pattern); i++)
        pattern[i] = (uint8_t)(i * 3u + 5u);
    check(node_write(fp, TEST_OFFSET + 100, pattern, sizeof(pattern)) == 0, "write 300 bytes across a page boundary");
    memset(buf2, 0, SECTOR);
    check(node_read(fp, TEST_OFFSET, buf2, SECTOR) == 0 && memcmp(buf2 + 100, pattern, sizeof(pattern)) == 0 &&
          all_ff(buf2, 100) && all_ff(buf2 + 400, SECTOR - 400), "read back: pattern in place, the rest still 0xFF");

    check(node_erase(fp, TEST_OFFSET, SECTOR) == 0, "erase again");
    for (uint32_t i = 0; i < SECTOR; i++)
        buf[i] = (uint8_t)(i * 7u + (i >> 8) + 1u);
    check(node_write(fp, TEST_OFFSET, buf, SECTOR) == 0, "write the whole sector");
    memset(buf2, 0, SECTOR);
    check(node_read(fp, TEST_OFFSET, buf2, SECTOR) == 0 && memcmp(buf, buf2, SECTOR) == 0, "read back the whole sector");
    uint8_t one = 0;
    check(node_read(fp, TEST_OFFSET + 1000, &one, 1) == 0 && one == buf[1000], "read one byte");
    check(node_read(fp, 16u * 1024u * 1024u - 8u, buf2, 16) != 0, "a read past the end of the chip is refused");
}

static void test_xip(void *fp, uint8_t *buf, uint8_t *buf2)
{
    bool on = true;
    uintptr_t base = 0;
    check(Dmod_Ioctl(fp, DMSPIFLASH_IOCTL_SET_MEMORY_MAPPED, &on) == 0, "memory-mapped mode on");
    check(Dmod_Ioctl(fp, DMSPIFLASH_IOCTL_GET_MAPPED_BASE, &base) == 0 && base == 0x90000000u, "mapped base 0x90000000");
    if (base != 0)
    {
        const uint8_t *window = (const uint8_t *)base;
        check(memcmp(window + TEST_OFFSET, buf, SECTOR) == 0, "the mapped window shows the sector just written");
        (void)node_read(fp, 0, buf2, 256);
        check(memcmp(window, buf2, 256) == 0, "the mapped window equals the node read at 0 (read kept the mode)");
    }
    on = false;
    check(Dmod_Ioctl(fp, DMSPIFLASH_IOCTL_SET_MEMORY_MAPPED, &on) == 0, "memory-mapped mode off");
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        Dmod_Printf("usage: flashtest <node> [--xip]\n");
        return 1;
    }
    void *fp = Dmod_FileOpen(argv[1], "r+");
    uint8_t *raw = Dmod_Malloc(3 * SECTOR + 64);
    if (fp == NULL || raw == NULL)
    {
        Dmod_Printf("flashtest: cannot open '%s' or out of memory\n", argv[1]);
        return 1;
    }
    uint8_t *buf = (uint8_t *)(((uintptr_t)raw + 31u) & ~(uintptr_t)31u);
    uint8_t *buf2 = buf + SECTOR;
    uint8_t *saved = buf2 + SECTOR;

    test_info(fp);
    bool have_backup = node_read(fp, TEST_OFFSET, saved, SECTOR) == 0;
    check(have_backup, "save the last sector");
    if (have_backup)
    {
        test_erase_write(fp, buf, buf2);
        if (argc >= 3 && strcmp(argv[2], "--xip") == 0)
            test_xip(fp, buf, buf2);
        bool restored = node_erase(fp, TEST_OFFSET, SECTOR) == 0 && node_write(fp, TEST_OFFSET, saved, SECTOR) == 0 &&
                        node_read(fp, TEST_OFFSET, buf2, SECTOR) == 0 && memcmp(saved, buf2, SECTOR) == 0;
        check(restored, "the saved sector content restored");
    }
    Dmod_Printf("flashtest: %d failed step(s)\n", g_failed);
    Dmod_Free(raw);
    Dmod_FileClose(fp);
    return g_failed;
}
