# Supported chips

| Chip | Board | JEDEC (mfr, type, cap) | Size | Page | Erase | Quad enable |
| --- | --- | --- | --- | --- | --- | --- |
| Micron N25Q128A (3 V) | STM32F746G-DISCO | 20 BA 18 | 16 MiB | 256 B | 4 KB / 64 KB | volatile config regs (0x81 / 0x61) |

Adding a chip: one row in `src/dmspiflash_chips.c` (sizes, flags, dummy cycles, the worst-case
timeouts from the datasheet) and a line here. A new quad-enable procedure also needs a value in
`dmspiflash_quad_enable_t` and a branch in `dmspiflash_core_open`. A unit test for the new id goes
in `tests/`. Optional later: SFDP fallback (0x5A) for chips not in the table.
