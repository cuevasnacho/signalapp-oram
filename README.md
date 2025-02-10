## Tree path
Tree path implementation is completed. Waiting for oram implementation to test `tree_path_create`.

## Bucket
Bucket implementations is almost completed. Missing implementations handling internal memory. Current implementation is wrong

## Stash
Current Stash implementation does not handle the case when a block cannot be inserted (reallocation needed). Missing implementations handling internal memory.
Besides that, `bitonic_sort` only supports arrays with fixed power of 2s length.

## Path ORAM
Since the capacities are fixed, some of them have been modified thus every test has the same capacity. There is an inconvenient when $num\_blocks > 1 \ll14 $ because two ORAMs are created and one of them will use wrong parameters.

> ### Tasks:
> - finish write_accessor for partial accesses
> - implement `#randombytes` syscall
> - write `oram_access`