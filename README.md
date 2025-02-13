## Tree path
Tree path implementation is completed. Waiting for oram implementation to test `tree_path_create`.

## Bucket
Bucket implementations is almost completed. Missing implementations handling internal memory. Current implementation is wrong

## Stash
Current Stash implementation does not handle the case when a block cannot be inserted (reallocation needed). Missing implementations handling internal memory.
Besides that, `bitonic_sort` only supports arrays with fixed power of 2s length.

## Path ORAM
Since the capacities are fixed, some of them have been modified thus every test has the same capacity. There is an inconvenient when $num\_blocks > 1 \ll14 $ because two ORAMs are created and one of them will use wrong parameters.

### Wish list:
- finish write_accessor for partial accesses
- add to every static function name a `_` in the front
- fix every avoidable jump
- change cmov for bitmask ternary op

### Tasks:
1) change structs for arrays to ensure fixed size
2) make it pass security checks (not sorter yet)
3) double check if stash length changes
4) check [batcher odd even sort](https://en.wikipedia.org/wiki/Batcher_odd%E2%80%93even_mergesort#cite_note-4)
