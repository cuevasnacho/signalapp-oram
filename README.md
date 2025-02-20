## Tree path
Tree path implementation is completed.

## Bucket
Bucket implementations is almost completed. Missing implementations handling internal memory. Current implementation is wrong

## Stash
Current Stash implementation does not handle the case when a block cannot be inserted (reallocation needed). Missing implementations handling internal memory. There is a batcher odd even sort written in C to replace bitonic sort.

## Path ORAM
Since the capacities are fixed, some of them have been modified thus every test has the same capacity. There is an inconvenient when $num\_blocks > 1 \ll14 $ because two ORAMs are created and one of them will use wrong parameters.

### Wish list:
- add to every static function name a `_` in the front
- fix every avoidable jump
- change cmov for bitmask ternary op

### Tasks:
1) change structs for arrays to ensure fixed size -> remove defines when done
2) make it pass security checks (not sorter yet)
3) double check if stash length changes:
  - update: when stash is extended, it increases by `STASH_GROWTH_INCREMENT` in overflow_capacity. This means that the sorter must support dynamic lengths.
4) check [batcher odd even sort](https://en.wikipedia.org/wiki/Batcher_odd%E2%80%93even_mergesort#cite_note-4)

### Update:
There are four active branches now:
- `main`: In this branch there is the implementation using original C code. This implementation uses many params therefore it is fixed. Besides that, it only supports position maps with less than $1\ll14$ blocks, this is due to the oram position map implementation.

- `fix-struct-size`: This branch contains the same Jasmin implementation but using "fixed size structs" (using arrays).

- `oram-as-positionmap`: In this branch, some parameters were removed in order to have more flexibility. With this approach I tried to implement position map using oram but I concluded it is not possible if we don't know the number of blocks beforehand.
  1. There are circular function calls
  2. We could rewrite the functions but if the number of blocks for the position map is big enough it may need another oram, i.e, another circular function call, and so on.

- `change-oblv-sort`: The purpose of this branch was to add the new sorting algorithm. Since I confirmed that the stash actually grows, we need a flexible sorting algorithm because it might change during execution. It is already working

I would need to know:
- Regarding the position map implementation, shall we wait for Rolfe to respond? So we can have more details about it.

- Is the approach for fixed struct size right?

- To start with the security checks, which variables should be #secret?

- Since we allocate C memory, which memory reads should I #declassify?

> In addition to this, I'd like to do some tests regarding the realloc syscalls