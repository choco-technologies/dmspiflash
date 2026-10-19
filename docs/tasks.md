# Implementation plan (in this order, one PR per step)

**Status:** steps 1-4 and 6 are DONE and verified on the STM32F746G-DISCO with `tools/flashtest` (block geometry, erase, page-crossing write, read-back, out-of-range refusal, memory-mapped window; `flashtest /dev/dmspiflashx/flash --xip`). Open: the plain-SPI bus layer (7), the file system (8). The node is `/dev/dmspiflashx/flash` (dmdevfs names it <driver>x/<section>).

Needs the bus: **dmqspi** (repo dmqspi) for the QSPI path. Board: STM32F746G-DISCO. Workflow:
`dmod-board-debugging` skill of dmod-boot. Read `docs/architecture.md` first. Host tests (`ctest`)
cover the pure helpers; the rest is verified on the board.

1. **qspi bus layer** (`dmspiflash_bus_qspi.c` + dispatcher `dmspiflash_bus.c`): open the node,
   command, poll. (dmqspi, dmspi and dmgpio are already in `dmod_link_modules` - keep them there.) Pass: READ ID through it returns `20 BA 18`.
2. **core open + read:** `dmspiflash_core_open` (reset, id, chip, quad enable), `dmspiflash_core_read`.
   Pass: 256 B at offset 0 equals a known pattern (write it once with the flash tool of OpenOCD
   or step 3).
3. **core write + erase:** `_write` (page split), `_erase`, `_erase_chip`. Pass: erase a sector ->
   all 0xFF; program a pattern across a page boundary -> read back identical.
4. **dmdrvi driver** in `dmspiflash.c` (per the comment there + `docs/configuration.md`): `_create` with
   friends, block ioctls. Pass: `/dev/dmspiflash0` exists; read/write/erase from the shell; `BLOCK_GET_INFO` sane.
5. **Board integration (dmod-boot):** modules `dmqspi dmqspi_port dmspiflash` + both `.ini` in `flash.dmd`.
6. **memory-mapped** switch (needs dmqspi step 4). Pass: read via 0x90000000 equals read via the node.
7. **plain SPI bus layer** (`dmspiflash_bus_spi.c`) when a board with SPI flash exists.
8. Next, other repos: `dmlfs` (littlefs on the node), automount rule + blkid signature.

Conventions: `Dmod_Malloc`/`Dmod_Free`, no fixed buffers for strings, functions under ~50 lines,
`_create/_destroy` with a magic guard, negative errno, `DMOD_LOG_*`. A module must print something
in `dmod_init` (the packer rejects an "empty" module); `DMOD_ENABLE_REGISTRATION` in exactly one .c file;
no pointers in const tables.
