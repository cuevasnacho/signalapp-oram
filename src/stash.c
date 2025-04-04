// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#define _GNU_SOURCE
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "../include/stash.h"
#include "../include/bucket.h"
#include "../include/tree_path.h"

// We grow additively instead of doubling because the stash size process has a
// significant negative drift that increases as the size of the stash increases.
// One effect is that if the stash size is, e.g., 50, then it is very likely
// to go all the way down to zero before it goes to 60. Growing by 20 we have a
// vanishingly small probability of growing again before we clear the entire stash
// and essentially start over.
#define STASH_GROWTH_INCREMENT 20

#define STASH_BLOCKS(s)             ((s)[0])
#define STASH_PATH_BLOCKS(s)        ((s)[1])
#define STASH_OVERFLOW_BLOCKS(s)    ((s)[2])
#define STASH_NUM_BLOCKS(s)         ((s)[3])
#define STASH_PATH_LENGTH(s)        ((s)[4])
#define STASH_OVERFLOW_CAPACITY(s)  ((s)[5])
#define STASH_BUCKET_OCCUPANCY(s)   ((s)[6])
#define STASH_BUCKET_ASSIGNMENTS(s) ((s)[7])
// struct stash
// {
//     /**
//      * @brief Array of all blocks stored in the stash
//      */
//     block* blocks;
//     /**
//      * @brief Convenience pointer to the part of `blocks` that contains blocks for the current path
//      */
//     block* path_blocks;
//     /**
//      * @brief Convenience pointer to the part of `blocks` that contains blocks for the overflow that 
//      * either does not fit or has not been placed in the current stash.
//      */
//     block* overflow_blocks;
//     /**
//      * @brief Total number of blocks that can be held in the `blocks` array.
//      */
//     size_t num_blocks;
//     /**
//      * @brief Length of the paths for the ORAM using this stash.
//      */
//     size_t path_length;
//     /**
//      * @brief Number of blocks in the stash that can be used to store overflow blocks that can't be allocated to the 
//      * `path_blocks`. This is the upper bound for the `overflow` array.
//      * 
//      */
//     size_t overflow_capacity;

//     // scratch space for block placement computations
//     u64* bucket_occupancy;
//     u64* bucket_assignments;
// };


typedef enum {
    block_type_overflow,
    block_type_path
} block_type;

size_t stash_size_bytes(size_t path_length, size_t overflow_size) {
    size_t num_path_blocks = BLOCKS_PER_BUCKET * path_length;
    size_t num_blocks = overflow_size + num_path_blocks;
    
    // stash struct + blocks + bucket_occupancy + bucket_assignments
    return sizeof(stash) + num_blocks*sizeof(block) + path_length*sizeof(u64) + num_blocks*sizeof(u64);
}

stash *stash_create(size_t path_length, size_t overflow_size)
{
    size_t num_path_blocks = BLOCKS_PER_BUCKET * path_length;
    size_t num_blocks = overflow_size + num_path_blocks;
    stash *result;
    CHECK(result = calloc(1, sizeof(*result)));
    block *blocks;
    CHECK(blocks = mmap(NULL, num_blocks * sizeof(block), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    STASH_BLOCKS(*result) = blocks;
    STASH_PATH_BLOCKS(*result) = (block*)STASH_BLOCKS(*result);
    STASH_OVERFLOW_BLOCKS(*result) = (block*)STASH_BLOCKS(*result) + num_path_blocks;
    STASH_NUM_BLOCKS(*result) = num_blocks;
    STASH_OVERFLOW_CAPACITY(*result) = num_blocks - num_path_blocks; 
    CHECK(overflow_size == STASH_OVERFLOW_CAPACITY(*result));
    STASH_PATH_LENGTH(*result) = path_length;

    CHECK(STASH_BUCKET_OCCUPANCY(*result) = calloc(path_length, sizeof(u64)));
    CHECK(STASH_BUCKET_ASSIGNMENTS(*result) = mmap(NULL, num_blocks * sizeof(u64), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    memset(STASH_BLOCKS(*result), 255,  sizeof(block) * num_blocks);
    return result;
}

void stash_destroy(stash *stash)
{
    if (stash)
    {
        munmap(STASH_BLOCKS(*stash), STASH_NUM_BLOCKS(*stash) * sizeof(block));
        free(STASH_BUCKET_OCCUPANCY(*stash));
        munmap(STASH_BUCKET_ASSIGNMENTS(*stash), STASH_NUM_BLOCKS(*stash) * sizeof(u64));
    }
    free(stash);
}

const block* stash_path_blocks(const stash* stash) {
    return (block*)STASH_PATH_BLOCKS(*stash);
}

static void stash_extend_overflow(stash* stash) {
    size_t old_num_blocks = STASH_NUM_BLOCKS(*stash);
    size_t new_num_blocks = old_num_blocks + STASH_GROWTH_INCREMENT;

    // (re)allocate new space, free the old
    CHECK(STASH_BLOCKS(*stash) = mremap(STASH_BLOCKS(*stash),
                                        old_num_blocks * sizeof(block), new_num_blocks * sizeof(block), MREMAP_MAYMOVE));
    munmap(STASH_BUCKET_ASSIGNMENTS(*stash), old_num_blocks * sizeof(u64));
    CHECK(STASH_BUCKET_ASSIGNMENTS(*stash) = mmap(NULL, new_num_blocks * sizeof(u64),
                                                  PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    // update our alias pointers
    STASH_PATH_BLOCKS(*stash) = (block*)STASH_BLOCKS(*stash);
    STASH_OVERFLOW_BLOCKS(*stash) = (block*)STASH_BLOCKS(*stash) + STASH_PATH_LENGTH(*stash) * BLOCKS_PER_BUCKET;

    // initialize new memory
    memset((block*)STASH_BLOCKS(*stash) + old_num_blocks, 255,  sizeof(block) * STASH_GROWTH_INCREMENT);

    // update counts
    STASH_NUM_BLOCKS(*stash) = new_num_blocks;
    STASH_OVERFLOW_CAPACITY(*stash) += STASH_GROWTH_INCREMENT;
}

/** returns the index of the last nonempty blocks in overflow */
static size_t stash_overflow_ub(const stash* stash) {
    // setting this value to be true will allow stash maintenance operations to
    // stop at a known upper bound for used stash entries. In practice this provides ~20%
    // increase in throughput.
    //
    // When this value is `true`, the length of the computation and the pattern of memory accesses will depend 
    // on the current number of items in the overflow stash and hence may allow an attacker to infer some 
    // information about the request flow such as likelihood that there were multiple repeated E164s requested
    // in a short window. If repeated accesses are well spaced, this will contain negligible information.
    //
    // We will want to know the current and maximum overflow size for health monitoring, and if we report that, 
    // there is no value in obfuscating it in the computation.
    bool allow_overflow_size_leak = true;

    size_t i = STASH_OVERFLOW_CAPACITY(*stash);
    if(allow_overflow_size_leak) {
        while( i > 0) {
            if(BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash))[i-1]) != EMPTY_BLOCK_ID) break;
            --i;
        }
    }
    return i;
}

size_t stash_num_overflow_blocks(const stash* stash) {
    size_t result = 0;
    for(size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash); ++i) {
        result += U64_TERNARY(BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash))[i]) != EMPTY_BLOCK_ID, 1, 0);
    }
    return result;
}
static inline block* first_block_in_bucket_for_level(const stash* stash, size_t level) {
    return (block*)STASH_PATH_BLOCKS(*stash) + level * BLOCKS_PER_BUCKET;
}

static void cond_copy_block(bool cond, block* dst, const block* src) {
    u64* tail_dst = (u64*)dst;
    u64* tail_src = (u64*)src;
    for(size_t i=0;i<sizeof(*dst)/sizeof(u64);++i) {
        cond_obv_cpy_u64(cond, tail_dst + i, tail_src + i);
    }
}

static void cond_swap_blocks(bool cond, block* a, block* b) { 
    u64* tail_dst = (u64*)a;
    u64* tail_src = (u64*)b;
    for(size_t i=0;i<sizeof(*a)/sizeof(u64);++i) {
        cond_obv_swap_u64(cond, tail_dst + i, tail_src + i);
    }
}

// Precondition: `target` is an empty block OR no block in the bucket has ID equal to `target_block_id`
// Postcondition: No block in the bucket has ID equal to `target_block_id`, `target` is either empty or `target->id == target_block_id`.
void stash_add_path_bucket(stash* stash, bucket_store* bucket_store, u64 bucket_id, u64 target_block_id, block *target) {
    size_t level = tree_path_level(bucket_id);
    block* bucket_blocks = first_block_in_bucket_for_level(stash, level);
    bucket_store_read_bucket_blocks(bucket_store, bucket_id, bucket_blocks);
    for(size_t i = 0; i < BLOCKS_PER_BUCKET; ++i) {
        bool cond = (target_block_id == BLOCK_ID(bucket_blocks[i]));
        CHECK(!(cond  & (BLOCK_ID(*target) != EMPTY_BLOCK_ID)));
        cond_swap_blocks(cond, target, bucket_blocks + i);
    }
}


// Precondition: `target` is an empty block OR no block in the overflow has ID equal to `target_block_id`
// Postcondition: No block in the overflow has ID equal to `target_block_id`, `target` is either empty or `target->id == target_block_id`.
void stash_scan_overflow_for_target(stash* stash, u64 target_block_id, block *target) {
    size_t num_found = 0;
    size_t ub = stash_overflow_ub(stash);
    for(size_t i = 0; i < ub; ++i) {
        bool cond = (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash))[i]) == target_block_id);
        CHECK(!(cond  & (BLOCK_ID(*target) != EMPTY_BLOCK_ID)));
        cond_swap_blocks(cond, target, (block*)STASH_OVERFLOW_BLOCKS(*stash) + i);
        num_found += cond ? 1 : 0;
    }
    CHECK(num_found <= 1);
}

// Precondition: there is no block with ID `new_block->id` anywhere in the stash - neither the path_Stash nor the overflow.
error_t stash_add_block(stash* stash, block* new_block) {
    bool inserted = false;
    for(size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash); ++i) {
        bool cond = (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash))[i]) == EMPTY_BLOCK_ID) & !inserted;
        cond_copy_block(cond, (block*)STASH_OVERFLOW_BLOCKS(*stash) + i, new_block);
        inserted = inserted | cond;
    }

    // This branch may leak information about the size of the stash but (1) this branch is difficult
    // to hit even with a malicious attack pattern and (2) we currently leak the stash size in much
    // more direct ways (we even publish it in the statistics), and if we decide to stop leaking
    // stash size we will have to stop extending the stash and simply fail. 
    if(!inserted) {
        stash_extend_overflow(stash);
        return stash_add_block(stash, new_block);
    }
    return err_SUCCESS;
}

static void stash_assign_block_to_bucket(stash* stash, const tree_path* path, block_type type, size_t index) {
    bool is_overflow_block = (type == block_type_overflow);

    // the block cannot be assigned to this level or higher 
    size_t max_level = U64_TERNARY(is_overflow_block, STASH_PATH_LENGTH(*stash), (index / BLOCKS_PER_BUCKET) + 1);
    size_t assignment_index = U64_TERNARY(is_overflow_block,  BLOCKS_PER_BUCKET * STASH_PATH_LENGTH(*stash)  + index, index);
    block* assigned_block = ((block*)STASH_PATH_BLOCKS(*stash)) + assignment_index;

    bool is_assigned = false;
    for(u64 level = 0; level < max_level; ++level) {
        u64 bucket_occupancy = ((u64*)STASH_BUCKET_OCCUPANCY(*stash))[level];
        u64 bucket_id = TREE_PATH_VALUES(*path)[level];
        bool is_valid = tree_path_lower_bound(bucket_id) <= BLOCK_POSITION(*assigned_block) & tree_path_upper_bound(bucket_id) >= BLOCK_POSITION(*assigned_block);
        bool bucket_has_room = bucket_occupancy < BLOCKS_PER_BUCKET;
        bool cond = is_valid & bucket_has_room & !is_assigned & BLOCK_ID(*assigned_block) != EMPTY_BLOCK_ID;

        // If `cond` is true, put it in the bucket: increment the bucket occupancy and set the bucket assignment
        // for this position.
        // increment this, it will only get saved if `cond` is true.
        ++bucket_occupancy;
        cond_obv_cpy_u64(cond, (u64*)STASH_BUCKET_OCCUPANCY(*stash) + level, &bucket_occupancy);
        cond_obv_cpy_u64(cond, (u64*)STASH_BUCKET_ASSIGNMENTS(*stash) + assignment_index, &level);
        is_assigned = cond | is_assigned;
    }
}

static void stash_place_empty_blocks(stash* stash) {
    u64 curr_bucket = 0;
    
    for(size_t i = 0; i < STASH_NUM_BLOCKS(*stash); ++i) {
        bool found_curr_bucket = false;
        for(size_t j = 0; j < STASH_PATH_LENGTH(*stash); ++j) {
            bool bucket_has_room = (((u64*)STASH_BUCKET_OCCUPANCY(*stash))[j] != BLOCKS_PER_BUCKET);
            bool set_curr_bucket = bucket_has_room & !found_curr_bucket;
            cond_obv_cpy_u64(set_curr_bucket, &curr_bucket, &j);
            found_curr_bucket = set_curr_bucket | found_curr_bucket;
        }
        u64 bucket_occupancy = ((u64*)STASH_BUCKET_OCCUPANCY(*stash))[curr_bucket];
        bool cond_place_in_bucket = bucket_occupancy < BLOCKS_PER_BUCKET & BLOCK_ID(((block*)STASH_BLOCKS(*stash))[i]) == EMPTY_BLOCK_ID;
        bucket_occupancy++;

        cond_obv_cpy_u64(cond_place_in_bucket, (u64*)STASH_BUCKET_OCCUPANCY(*stash) + curr_bucket, &bucket_occupancy);
        cond_obv_cpy_u64(cond_place_in_bucket, (u64*)STASH_BUCKET_ASSIGNMENTS(*stash) + i, &curr_bucket);

    }

    // at the end, every bucket should be full
}

static error_t stash_assign_buckets(stash* stash, const tree_path* path) {
    // assign all blocks to "overflow" - level UINT64_MAX and set all occupancy to 0
    memset(STASH_BUCKET_ASSIGNMENTS(*stash), 255, STASH_NUM_BLOCKS(*stash) * sizeof(STASH_BUCKET_ASSIGNMENTS(*stash)));
    memset(STASH_BUCKET_OCCUPANCY(*stash), 0, STASH_PATH_LENGTH(*stash) * sizeof(u64));


    // assign blocks in path to buckets first
    for(size_t level = 0; level < STASH_PATH_LENGTH(*stash); ++level) {
        for(size_t b = 0; b < BLOCKS_PER_BUCKET; ++b) {
            stash_assign_block_to_bucket(stash, path, block_type_path, level * BLOCKS_PER_BUCKET + b);
        }
    }

    // assign blocks in overflow to buckets
    size_t ub = stash_overflow_ub(stash);
    for(size_t i = 0; i < ub; ++i) {
        stash_assign_block_to_bucket(stash, path, block_type_overflow, i);
    }

    // now assign empty blocks to fill the buckets
    stash_place_empty_blocks(stash);
    return err_SUCCESS;
}


static inline bool comp_blocks(block* blocks, u64* block_level_assignments, size_t idx1, size_t idx2) {
    return (block_level_assignments[idx1] > block_level_assignments[idx2])
                | ((block_level_assignments[idx1] == block_level_assignments[idx2]) & (BLOCK_POSITION(blocks[idx1]) > BLOCK_POSITION(blocks[idx2])));
}

/**
 * @brief Core subroutine for `bitonic_sort`. Takes a bitonic array as input and sorts it with a deterministic sequence
 * of conditional swaps.
 * 
 * @param blocks blocks to sort
 * @param block_level_assignments level assignments for blocks. Blocks will be sorted in order of increasing level
 * @param lb lower bound for array to sort
 * @param ub upper bound for array to sort (non-inclusive)
 * @param direction result is ascending sort if true, descending if false.
 */
static void bitonic_merge(block* blocks, u64* block_level_assignments, size_t lb, size_t ub, bool direction) {
    size_t n = ub - lb;
    if(n > 1) {
        size_t pow2 = first_pow2_leq(n);
        if(pow2 == n) pow2 >>= 1;
        for(size_t i = lb; i < ub - pow2; ++i) {
            bool cond = direction == comp_blocks(blocks, block_level_assignments, i, i+pow2);
            cond_swap_blocks(cond, blocks + i, blocks + i + pow2);
            cond_obv_swap_u64(cond, block_level_assignments + i, block_level_assignments + i + pow2);
        }

        // Note that at this point, since the input was bitonic, everything in the arraw with
        // index >= lb + pow2 has a "larger" value (relative to `direction`) than the entries
        // with index < lb + pow2. Also, both the upper and lower part of the array are bitonic
        // subarrays.
        bitonic_merge(blocks, block_level_assignments, lb, lb + pow2, direction);
        bitonic_merge(blocks, block_level_assignments, lb + pow2, ub, direction);
    }
}

/**
 * @brief Bitonic (as opposed to monotonic) sort is an oblivious sort algorithm due to Batcher (http://www.cs.kent.edu/~batcher/sort.pdf).
 * While initially designed for building sorting circuits in hardward, it serves our purposes here because it will sort a list
 * using a deterministic sequence of comparisons and swaps - independent of the input. It is called "bitonic" because its core
 * subroutine `bitonic_merge` takes a bitonic list (only changes direction once, e.g., increasing and then decreasing) and converts it to a monotonic
 * (i.e. sorted) list.
 * 
 * @param blocks blocks to sort
 * @param block_level_assignments level assignments for blocks. Blocks will be sorted in order of increasing level
 * @param lb lower bound for array to sort
 * @param ub upper bound for array to sort (non-inclusive)
 * @param direction Ascending sort if true, descending if false.
 */
static void bitonic_sort(block* blocks, u64* block_level_assignments, size_t lb, size_t ub, bool direction) {
    size_t n = ub - lb;
    if(n > 1) {
        size_t half_n = n>>1;
        bitonic_sort(blocks, block_level_assignments, lb, lb + half_n, !direction);
        bitonic_sort(blocks, block_level_assignments, lb + half_n, ub, direction);
        bitonic_merge(blocks, block_level_assignments, lb, ub, direction);
    }
}

static inline size_t min(size_t a, size_t b) {
    return U64_TERNARY(a < b, a, b);
}

static void odd_even_msort(block* blocks, u64 *block_level_assignments, size_t lb, size_t ub) {
    size_t n = ub - lb;
    for (size_t p = 1; p < n; p <<= 1) {
        for (size_t k = p; k >= 1; k >>= 1) {
            size_t mod_kp = k % p;
            for (size_t j = mod_kp; j < n-k; j += 2*k) {
                for (size_t i = 0; i < min(k, n-j-k); ++i) {
                    if (((i+j) / (p*2)) == ((i+j+k) / (p*2))) {
                        size_t idx = i + j + lb;
                        bool cond = comp_blocks(blocks, block_level_assignments, idx, idx+k);
                        cond_swap_blocks(cond, blocks + idx, blocks + idx + k);
                        cond_obv_swap_u64(cond, block_level_assignments + idx, block_level_assignments + idx + k);
                    }
                }
            }
        }
    }
}

void print_bucket_assignments(const stash* stash) {
    for(size_t i = 0; i < STASH_NUM_BLOCKS(*stash); ++i) {
        fprintf(stderr, "%zu: block: %" PRIu64 " pos: %" PRIu64 " assignment: %" PRIu64 "\n",
            i, BLOCK_ID(((block*)STASH_BLOCKS(*stash))[i]), BLOCK_POSITION(((block*)STASH_BLOCKS(*stash))[i]), ((u64*)STASH_BUCKET_ASSIGNMENTS(*stash))[i]);
    }
}

void stash_build_path(stash* stash, const tree_path* path) {
    size_t overflow_size = stash_overflow_ub(stash);
    stash_assign_buckets(stash, path);
    odd_even_msort((block*)STASH_BLOCKS(*stash), STASH_BUCKET_ASSIGNMENTS(*stash), 0, BLOCKS_PER_BUCKET * STASH_PATH_LENGTH(*stash) + overflow_size);
    // print_bucket_assignments(stash);
}


error_t stash_clear(stash* stash) {
    memset((block*)STASH_BLOCKS(*stash), 255,  sizeof(block) * STASH_NUM_BLOCKS(*stash));
    return err_SUCCESS;
}

#ifdef IS_TEST
#include <stdio.h>
#include <sys/random.h>
#include "../include/tests.h"

// jasmin functions
void cond_copy_block_jazz(bool cond, block* dst, const block* src);
void cond_swap_blocks_jazz(bool cond, block* a, block* b);
void stash_add_block_jazz(stash* stash, block* new_block);
void odd_even_msort_jazz(block* blocks, u64* block_level_assignments, size_t lb, size_t ub, bool direction);

void stash_print(const stash *stash)
{
    size_t num_blocks = 0;
    for (size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash); ++i)
    {
        if (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash))[i]) != EMPTY_BLOCK_ID)
        {
            num_blocks++;
        }
    }
    printf("Stash holds %zu blocks.\n", num_blocks);
    for (size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash); ++i)
    {
        if (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash))[i]) != EMPTY_BLOCK_ID)
        {
            printf("block_id: %" PRIu64 "\n", BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash))[i]));
        }
    }
}


int test_cond_cpy_block() {
    block b1 = {1, 1023, 9,8,7,6,5,4,3,2,1};
    block b1_jazz = {1, 1023, 9,8,7,6,5,4,3,2,1};
    block b1_orig = {1, 1023, 9,8,7,6,5,4,3,2,1};
    block b2 = {2, 2047, 19,18,17,16,15,14,13,12,11};
    block b2_jazz = {2, 2047, 19,18,17,16,15,14,13,12,11};
    block b2_orig = {2, 2047, 19,18,17,16,15,14,13,12,11};

    block target = {EMPTY_BLOCK_ID, UINT64_MAX};
    block mt_block = {EMPTY_BLOCK_ID, UINT64_MAX};
    block target_jazz = {EMPTY_BLOCK_ID, UINT64_MAX};
    block mt_block_jazz = {EMPTY_BLOCK_ID, UINT64_MAX};

    cond_copy_block(false, &target, &b1);
    cond_copy_block_jazz(false, &target_jazz, &b1_jazz);
    TEST_ASSERT(memcmp(&mt_block, &target, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&mt_block_jazz, &target_jazz, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&b1_orig, &b1, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&b1_jazz, &b1, sizeof(block)) == 0);

    cond_copy_block(true, &target, &b1);
    cond_copy_block_jazz(true, &target_jazz, &b1_jazz);
    TEST_ASSERT(memcmp(&target, &b1, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&target_jazz, &b1_jazz, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&b1_orig, &b1, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&b1_jazz, &b1, sizeof(block)) == 0);

    cond_swap_blocks(false, &b1, &b2);
    cond_swap_blocks(false, &b1_jazz, &b2_jazz);
    TEST_ASSERT(memcmp(&b1_orig, &b1, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&b1_orig, &b1_jazz, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&b2_orig, &b2, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&b2_orig, &b2_jazz, sizeof(block)) == 0);

    cond_swap_blocks_jazz(true, &b1, &b2);
    cond_swap_blocks_jazz(true, &b1_jazz, &b2_jazz);
    TEST_ASSERT(memcmp(&b1_orig, &b2, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&b1_orig, &b2_jazz, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&b2_orig, &b1, sizeof(block)) == 0);
    TEST_ASSERT(memcmp(&b2_orig, &b1_jazz, sizeof(block)) == 0);

    return 0;
}

int test_oblv_sort() {
    size_t num_blocks = 30;
    block blocks[30] = {0};
    for (size_t i = 0; i < 20; i++) BLOCK_ID(blocks[i]) = i;

    u64 bucket_assignments[30] = {
        0,7,UINT64_MAX,2,9,UINT64_MAX,4,11,UINT64_MAX,6,1,UINT64_MAX,8,3,UINT64_MAX,10,5,0,5,0
    };

    block original_blocks[30], jazz_blocks[30];
    u64 original_bucket_assignments[30], jazz_bucket_assignments[30];
    memcpy(original_blocks, blocks, sizeof(blocks));
    memcpy(jazz_blocks, blocks, sizeof(blocks));
    memcpy(original_bucket_assignments, bucket_assignments, sizeof(bucket_assignments));
    memcpy(jazz_bucket_assignments, bucket_assignments, sizeof(bucket_assignments));

    odd_even_msort(blocks, bucket_assignments, 0, num_blocks);
    odd_even_msort_jazz(jazz_blocks, jazz_bucket_assignments, 0, num_blocks, true);

    for(size_t i = 1; i < num_blocks; ++i) {
        // check that it is sorted
        TEST_ASSERT(bucket_assignments[i-1] <= bucket_assignments[i]);
        // and that the blocks and bucket assignments moved together
        bool found = false;
        for(size_t j = 0; j < num_blocks; ++j) {
            if(BLOCK_ID(blocks[i]) == BLOCK_ID(original_blocks[j])) {
                found = true;
                TEST_ASSERT(bucket_assignments[i] == original_bucket_assignments[j]);
                break;
            }
        }
        TEST_ASSERT(found);

        found = false;
        for(size_t j = 0; j < num_blocks; ++j) {
            if(BLOCK_ID(jazz_blocks[i]) == BLOCK_ID(original_blocks[j])) {
                found = true;
                TEST_ASSERT(jazz_bucket_assignments[i] == original_bucket_assignments[j]);
                break;
            }
        }
        TEST_ASSERT(found);
    }
    for(size_t i = 0; i < num_blocks; ++i) {
        TEST_ASSERT(bucket_assignments[i] == jazz_bucket_assignments[i]);
    }
    return err_SUCCESS;
}


static void fill_block_data(u64 data[static BLOCK_DATA_SIZE_QWORDS])
{
    for (size_t i = 0; i < BLOCK_DATA_SIZE_QWORDS; ++i)
    {
        data[i] = rand();
    }
}

static int check_stash_entry(block e, u64 expected_block_id, const u64 expected_data[BLOCK_DATA_SIZE_QWORDS], u64 expected_position)
{
    TEST_ASSERT(BLOCK_ID(e) == expected_block_id);
    TEST_ASSERT(BLOCK_POSITION(e) == expected_position);
    if (expected_block_id != EMPTY_BLOCK_ID)
    {
        for (size_t i = 0; i < BLOCK_DATA_SIZE_QWORDS; ++i)
        {
            TEST_ASSERT(BLOCK_DATA(e)[i] == expected_data[i]);
        }
    }
    return 0;
}


/*
Return the bucket ID for a leaf node that has a given bucket_id on its path, uniform random.
*/
u64 random_position_for_bucket(u64 bucket_id) {
    u64 lb = tree_path_lower_bound(bucket_id);
    u64 ub = tree_path_upper_bound(bucket_id);
    u64 rnd = 0;
    getentropy(&rnd, sizeof(rnd));
    return lb + (rnd % (ub+1 - lb)); // need the +1 because it is an inclusive upper bound
}
static void generate_blocks_for_bucket(u64 bucket_id, u64 block_id_start, size_t num_nonempty_blocks, block blocks[3]) {
    block empty = {EMPTY_BLOCK_ID, UINT64_MAX};
    u64 num_created = 0;
    u64 rnd[] = {0};
    getentropy(rnd, sizeof(rnd));

    // This code is dependent of BLOCKS_PER_BUCKET == 3 - To generalize could use a random shuffle
    // If we have either 1 or 2 non-empty blocks, one of our three blocks in our bucket will be
    // special: for 1 non-empty block the special one is the non-empty one. For 2 non-empty blocks,
    // the special one is the empty one. 
    //
    // Handling 0 or 3 non-empty blocks is easy.
    size_t special_block_idx = rnd[0] % BLOCKS_PER_BUCKET;
    for(size_t  i = 0; i < BLOCKS_PER_BUCKET; ++i) {
        bool fill_block = (num_nonempty_blocks == BLOCKS_PER_BUCKET)
                            || (num_nonempty_blocks == 2 && i != special_block_idx)
                            || (num_nonempty_blocks == 1 && i == special_block_idx);
        
        block block = {block_id_start + num_created, random_position_for_bucket(bucket_id)};
        if (fill_block) { memcpy(blocks[i], block, sizeof(block)); }
        else { memcpy(blocks[i], empty, sizeof(block)); }
        num_created += fill_block ? 1 : 0;
    }

}

static void load_bucket_store(
    bucket_store* bucket_store0, bucket_store* bucket_store1, size_t num_levels,
    const tree_path* path, bucket_density density, size_t* num_blocks_created
) {
    u8 bucket_data[DECRYPTED_BUCKET_SIZE];
    block* bucket_blocks = (block*) bucket_data;
    *num_blocks_created = 0;
    for(size_t level = 0; level < TREE_PATH_LENGTH(*path); ++level) {
        size_t num_blocks = 0;
        switch(density) {
        case bucket_density_empty:
            num_blocks = 0;
            break;
        case bucket_density_sparse:
            num_blocks = rand() % 2;
            break;
        case bucket_density_dense:
            num_blocks = 1 + rand() % BLOCKS_PER_BUCKET;
            break;
        case bucket_density_full:
            num_blocks = BLOCKS_PER_BUCKET;
        }
        *num_blocks_created += num_blocks;
        generate_blocks_for_bucket(TREE_PATH_VALUES(*path)[level], *num_blocks_created, num_blocks, bucket_blocks);
        bucket_store_write_bucket_blocks(bucket_store0, TREE_PATH_VALUES(*path)[level], bucket_blocks);
        bucket_store_write_bucket_blocks(bucket_store1, TREE_PATH_VALUES(*path)[level], bucket_blocks);
    }
    
}

int test_load_bucket_path_to_stash(bucket_density density) {
    size_t num_levels = 18;
    stash *stash0 = stash_create(num_levels, TEST_STASH_SIZE);
    stash *stash1 = stash_create(num_levels, TEST_STASH_SIZE);
    bucket_store* bucket_store0 = bucket_store_create(num_levels);
    bucket_store* bucket_store1 = bucket_store_create(num_levels);

    u64 root = (1ul << (num_levels - 1)) - 1;
    u64 leaf = 157142;
    tree_path* path0 = tree_path_create(leaf, root);
    tree_path* path1 = tree_path_create_jazz(leaf, root);

    size_t num_blocks_added = 0;
    load_bucket_store(bucket_store0, bucket_store1, num_levels, path0, density, &num_blocks_added);

    u64 target_block_id = num_blocks_added / 2;
    block target0 = {EMPTY_BLOCK_ID};
    block target1 = {EMPTY_BLOCK_ID};
    for(size_t i = 0; i < TREE_PATH_LENGTH(*path0); ++i) {
        stash_add_path_bucket(stash0, bucket_store0, TREE_PATH_VALUES(*path0)[i], target_block_id, &target0);
        stash_add_path_bucket_jazz(stash1, bucket_store1, TREE_PATH_VALUES(*path1)[i], target_block_id, &target1);
    }

    TEST_ASSERT(BLOCK_ID(target0) == target_block_id);
    TEST_ASSERT(BLOCK_ID(target1) == target_block_id);
    for(size_t i = 0; i < TREE_PATH_LENGTH(*path0); ++i) {
        block* bucket_blocks0 = (block*)STASH_PATH_BLOCKS(*stash0) + i * BLOCKS_PER_BUCKET;
        block* bucket_blocks1 = (block*)STASH_PATH_BLOCKS(*stash1) + i * BLOCKS_PER_BUCKET;
        for(size_t b = 0; b < BLOCKS_PER_BUCKET; ++b) {
            TEST_ASSERT(BLOCK_ID(bucket_blocks0[b]) != target_block_id);
            TEST_ASSERT(BLOCK_ID(bucket_blocks1[b]) != target_block_id);
        }
    }

    stash_destroy(stash0);
    stash_destroy(stash1);
    bucket_store_destroy(bucket_store0);
    bucket_store_destroy(bucket_store1);
    tree_path_destroy(path0);
    tree_path_destroy(path1);
    return 0;
}


int test_stash_insert_read()
{
    stash *stash0 = stash_create(18, TEST_STASH_SIZE);
    stash *stash1 = stash_create(18, TEST_STASH_SIZE);
    u64 data0[BLOCK_DATA_SIZE_QWORDS];
    u64 data1[BLOCK_DATA_SIZE_QWORDS];
    u64 block_id0 = 1234;
    u64 block_id1 = 45678;
    fill_block_data(data0);
    fill_block_data(data1);

    block b0 = {block_id0, 2};
    block b1 = {block_id1, 3};
    block target0 = {EMPTY_BLOCK_ID, UINT64_MAX};
    block target1 = {EMPTY_BLOCK_ID, UINT64_MAX};
    block target2 = {EMPTY_BLOCK_ID, UINT64_MAX};
    block target3 = {EMPTY_BLOCK_ID, UINT64_MAX};
    memcpy(BLOCK_DATA(b0), data0, sizeof(data0));
    memcpy(BLOCK_DATA(b1), data1, sizeof(data1));

    RETURN_IF_ERROR(stash_add_block(stash0, &b0));
    RETURN_IF_ERROR(stash_add_block(stash0, &b1));
    stash_add_block_jazz(stash1, &b0);
    stash_add_block_jazz(stash1, &b1);

    bool b0_in_stash = false, b1_in_stash = false;
    bool b2_in_stash = false, b3_in_stash = false;
    for(size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash0); ++i) {
        b0_in_stash = b0_in_stash || (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash0))[i]) == BLOCK_ID(b0));
        b1_in_stash = b1_in_stash || (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash0))[i]) == BLOCK_ID(b1));
    }
    for(size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash1); ++i) {
        b2_in_stash = b2_in_stash || (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash1))[i]) == BLOCK_ID(b0));
        b3_in_stash = b3_in_stash || (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash1))[i]) == BLOCK_ID(b1));
    }
    TEST_ASSERT(b0_in_stash);
    TEST_ASSERT(b1_in_stash);
    TEST_ASSERT(b2_in_stash);
    TEST_ASSERT(b3_in_stash);

    stash_scan_overflow_for_target(stash0, block_id0, &target0);
    stash_scan_overflow_for_target(stash0, block_id1, &target1);
    stash_scan_overflow_for_target_jazz(stash1, block_id0, &target2);
    stash_scan_overflow_for_target_jazz(stash1, block_id1, &target3);

    check_stash_entry(target0, block_id0, data0, 2);
    check_stash_entry(target1, block_id1, data1, 3);
    check_stash_entry(target2, block_id0, data0, 2);
    check_stash_entry(target3, block_id1, data1, 3);

    // check that it is no longer in stash
    b0_in_stash = false; b1_in_stash = false;
    b2_in_stash = false; b3_in_stash = false;
    for(size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash0); ++i) {
        b0_in_stash = b0_in_stash || (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash0))[i]) == BLOCK_ID(b0));
        b1_in_stash = b1_in_stash || (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash0))[i]) == BLOCK_ID(b1));
        b2_in_stash = b2_in_stash || (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash1))[i]) == BLOCK_ID(b0));
        b3_in_stash = b3_in_stash || (BLOCK_ID(((block*)STASH_OVERFLOW_BLOCKS(*stash1))[i]) == BLOCK_ID(b1));
    }
    TEST_ASSERT(!b0_in_stash);
    TEST_ASSERT(!b1_in_stash);
    TEST_ASSERT(!b2_in_stash);
    TEST_ASSERT(!b3_in_stash);

    stash_destroy(stash0);
    stash_destroy(stash1);
    return err_SUCCESS;
}

int test_fill_stash() {
    size_t small_stash_size = 20;
    stash *stash0 = stash_create(20, small_stash_size);
    stash *stash1 = stash_create(20, small_stash_size);
    for(size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash0); ++i) {
        block b = {0}; BLOCK_ID(b) = i; BLOCK_POSITION(b) = 2*(100+i);
        RETURN_IF_ERROR(stash_add_block(stash0, &b));
        // check that it is in the stash
    }
    for(size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash1); ++i) {
        block b = {0}; BLOCK_ID(b) = i; BLOCK_POSITION(b) = 2*(100+i);
        stash_add_block_jazz(stash1, &b);
        // check that it is in the stash
    }

    block b0 = {0};
    block b1 = {0};
    BLOCK_ID(b0) = STASH_OVERFLOW_CAPACITY(*stash0); BLOCK_POSITION(b0) = 2*(100+STASH_OVERFLOW_CAPACITY(*stash0));
    BLOCK_ID(b1) = STASH_OVERFLOW_CAPACITY(*stash1); BLOCK_POSITION(b1) = 2*(100+STASH_OVERFLOW_CAPACITY(*stash1));

    // This will trigger an extension of the stash
    RETURN_IF_ERROR(stash_add_block(stash0, &b0));
    stash_add_block_jazz(stash1, &b1);

    // now remove a block and then confirm that we have room
    block target0 = {0}; BLOCK_ID(target0) = EMPTY_BLOCK_ID; BLOCK_POSITION(target0) = UINT64_MAX;
    block target1 = {0}; BLOCK_ID(target1) = EMPTY_BLOCK_ID; BLOCK_POSITION(target1) = UINT64_MAX;

    u64 search_block_id = 11;
    stash_scan_overflow_for_target(stash0, search_block_id, &target0);
    stash_scan_overflow_for_target(stash1, search_block_id, &target1);
    TEST_ASSERT(BLOCK_ID(target0) == search_block_id);
    TEST_ASSERT(BLOCK_ID(target1) == search_block_id);

    for(size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash0); ++i) {
        TEST_ASSERT(BLOCK_ID(((block*)STASH_BLOCKS(*stash0))[i]) != search_block_id);
    }
    RETURN_IF_ERROR(stash_add_block(stash0, &b0));
    for(size_t i = 0; i < STASH_OVERFLOW_CAPACITY(*stash1); ++i) {
        TEST_ASSERT(BLOCK_ID(((block*)STASH_BLOCKS(*stash1))[i]) != search_block_id);
    }
    stash_add_block_jazz(stash1, &b1);

    stash_destroy(stash0);
    stash_destroy(stash1);
    return 0;
}

#endif // IS_TEST
