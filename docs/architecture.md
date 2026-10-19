# dmspiflash architecture

```
 littlefs / FAT / automount / shell
              |  block device /dev/dmspiflash0
   +----------+-----------+
   | dmspiflash           |
   |  src/dmspiflash.c       module entry, pure helpers, dmdrvi driver (block ioctls)
   |  src/dmspiflash_core.c  chip logic: identify, read, page program, erase, quad enable
   |  src/dmspiflash_chips.c table of known chips (JEDEC id -> geometry, quirks)
   |  src/dmspiflash_bus.c   dispatcher: switch on config.bus
   |  src/dmspiflash_bus_qspi.c | dmspiflash_bus_spi.c   one flash command over that bus
   +----------+-----------+
              |  Dmod_Ioctl / Dmod_Read / Dmod_Write on the bus node
   /dev/dmqspi1 (dmqspi)   or   /dev/dmspi1 + CS gpio (dmspi)
```

Rules: the core never touches a bus (only `dmspiflash_bus_*`); the bus layer never
knows an opcode's meaning (it moves a `dmspiflash_cmd_t`); no table of function
pointers (const data with relocations is not allowed in a module) - dispatch with a `switch`.

## Finding the bus
The flash `.ini` section shares a `friends_group` with the bus section
(`configs/board/stm32f746g-disco/n25q128.ini`).

## Dependencies and configs live here
One `.ini` per chip/board holds *everything*: the dmgpio pin sections, the `[qspi]`/`[spi]` bus section
and the `[flash]` section. Therefore dmspiflash depends on dmqspi, dmspi and dmgpio
(`dmod_link_modules` in CMakeLists.txt -> dmspiflash.dmd), and `dmspiflash.dmr` installs `configs/`. A board
asks only for `dmspiflash`; dmf-get imports the bus and pin drivers by itself. Bus modules stay
chip-agnostic and ship no board configs. In `_create` ask dmdevfs for the friends
(`DMDRVI_IOCTL_DEVFS_GET_FRIEND`, see dmdrvi docs "friends"); on plain SPI also take the
`friend_role=chip_select` GPIO. If the bus is not ready yet, finish in `_friend_changed`.
`driver_order` of the flash must be higher than the bus.

## Block-device contract
Offsets are bytes. NOR can only clear bits, so **the area must be erased before it is
written** - the driver does not erase implicitly (the file system decides; that is why
`DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED` is set).

| dmdrvi | Behaviour |
| --- | --- |
| `_read` | any offset/size; quad fast read (0xEB) when `quad=1`, else 0x0B |
| `_write` | page program, split by `dmspiflash_page_chunk`; `-EIO` on a chip-reported program error |
| `BLOCK_GET_INFO` | `logical_block_size=512`, `erase_block_size=chip->sector_size`, `block_count=size/512`, flag ERASE_SUPPORTED |
| `BLOCK_ERASE` | range aligned to the sector size, else `-EINVAL`; 64 KB opcode for aligned 64 KB runs |
| `DMSPIFLASH_IOCTL_*` | JEDEC id, chip copy, chip erase, memory-mapped on/off, mapped base |
| `_flush` | no-op - every write finishes (WIP clear) before returning |

One `dmosi` mutex per device: a whole read/write/erase is one critical section (WREN + program + poll
must not interleave with another thread's command).

## Flash operations (core)
* **open:** reset (0x66, 0x99, ~1 ms) -> READ ID 0x9F on 1 line -> chip by `chip=` name or JEDEC id
  (unknown -> `-ENODEV`; log the id so the user can add a row) -> if `quad=1`: enable per `chip->quad_enable`
  (Micron: write volatile config 0x81 with dummy cycles, enhanced volatile 0x61 to enable quad I/O) ->
  if `memory_mapped=1`: map.
* **read:** one command, `data size = size` (the controller streams; no page limit).
* **write:** per chunk: WREN (0x06) -> 0x32/0x02 + address + data -> poll status WIP=0 (timeout
  `chip->page_program_ms`); Micron: read flag status 0x70, program-error bit -> clear 0x50, `-EIO`.
* **erase:** per 4 KB sector WREN + 0x20 + poll (`sector_erase_ms`); chip erase 0xC7 (`chip_erase_ms`, seconds).
* **memory-mapped (qspi):** while on, indirect operations first leave it and re-enter afterwards;
  after write/erase the bus invalidates the D-cache of the window.
