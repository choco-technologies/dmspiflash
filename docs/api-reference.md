# API reference

* `include/dmspiflash.h` - pure helpers exported to other modules / tests: `_validate_config`,
  `_find_chip_by_id`, `_find_chip_by_name`, `_page_chunk`. Clients of the flash use the block node
  `/dev/dmspiflash<N>` (docs/architecture.md), not this header.
* `include/dmspiflash_types.h` - bus kinds, chip row, config, `DMSPIFLASH_IOCTL_*`.
* `include/dmspiflash_cmds.h` - NOR opcodes and status bits.
* `include/dmspiflash_internal.h` - NOT an API: the contract between the core and the bus layers.

Return convention: `0` / byte count on success, negative errno on failure.
