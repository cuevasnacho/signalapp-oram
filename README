## Tree path
Tree path implementation seems to be completed. All tests pass

Some considerations:
- As in Jasmin is not possible to allocate memory dynamically, the size of the `tree_path` has to be fixed in the parameters.

Tested functions:
- `tree_path_update`
- `level`
- `node_val`
- `tree_path_num_nodes`
- `tree_path_lower_bound`
- `tree_path_upper_bound`
- `tree_path_level`

## Bucket
Current implementation use the same structure as in C (take into account both parameters when reading from memory).

Tested functions:
- `bucket_store_clear`
- `bucket_store_read_bucket_blocks`
- `bucket_store_write_bucket_blocks`

## Stash
Code almost completed. The main problem here are:
- `bitonic_sort`: which is a recursive function (not possible in Jasmin)
- `stash_extend_overflow`: does reallocation. Jasmin does not support dynamic memory allocation but may be there is a way to reallocate using syscalls

Tested functions:
- `stash_add_path_bucket`
- `stash_scan_overflow_for_target`
- `stash_overflow_ub`
- `first_block_in_bucket_for_level`
- `cond_swap_blocks`
- `cond_copy_block`
- `cond_obv_copy_u64`
- `cond_obv_swap_u64`

## Path ORAM
Since the capacities are fixed, some of them have been modified thus every test has the same capacity. There is an inconvenient when $num\_blocks > 1\ll14$ because two ORAMs are created and one of them will use wrong parameters.

Tested functions:
- `oram_read_path_for_block_jazz`
