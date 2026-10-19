# dmspiflash configuration (.ini)

The `.ini` of a board (`configs/board/<board>/<chip>.ini`) holds pins, the bus section and this one.

Section with `driver_name=dmspiflash`; `friends_group` = the group of the bus section.

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `bus` | `qspi` \| `spi` | required | Which bus layer to use |
| `chip` | name from the table | empty | Force a chip, skip the JEDEC check (empty = identify) |
| `quad` | 0 \| 1 | 0 | Use 4-line read/program (qspi only; `spi` + `quad=1` is a config error) |
| `memory_mapped` | 0 \| 1 | 0 | Enter memory-mapped mode after open (qspi only) |

Unknown keys: warn, do not fail. Parsing is in the dmdrvi `_create`; the result must pass
`dmspiflash_validate_config()` (implemented, unit tested). Node name: `/dev/dmspiflash<N>`.
