// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../include/tree_path.h"
#include "../include/bucket.h"
#include "../include/util.h"

#define BUCKET_STORE_NUM_LEVELS(b)  ((b)[0])
#define BUCKET_STORE_SIZE_BYTES(b)  ((b)[1])
#define BUCKET_STORE_DATA(b)        ((b)[2])
/*
struct bucket_store
{
    size_t num_levels;
    size_t size_bytes;
    u8 *data;
};
*/

// Create a path ORAM bucket store with capacity for a tree with `num_levels` levels,
// i.e. 2^num_levels - 1 tree nodes and 2^(num_levels - 1) leaf nodes/pathORAM positions.
bucket_store *bucket_store_create(size_t num_levels)
{
    size_t num_buckets = tree_path_num_nodes(num_levels);
    size_t size_bytes = num_buckets * ENCRYPTED_BUCKET_SIZE;

    u8 *data;
    CHECK(data = malloc(size_bytes));
    bucket_store *bucket_store;
    CHECK(bucket_store = calloc(1, sizeof(*bucket_store)));

    memset(data, 255, size_bytes);
    BUCKET_STORE_DATA(*bucket_store) = data;
    BUCKET_STORE_SIZE_BYTES(*bucket_store) = size_bytes;
    BUCKET_STORE_NUM_LEVELS(*bucket_store) = num_levels;
    return bucket_store;
}
void bucket_store_destroy(bucket_store *bucket_store)
{
    // Acceptable if: not executed in oram_access
    if (bucket_store)
    {
        free(BUCKET_STORE_DATA(*bucket_store));
        free(bucket_store);
    }
}

void bucket_store_clear(bucket_store *bucket_store)
{
    memset(BUCKET_STORE_DATA(*bucket_store), 255, BUCKET_STORE_SIZE_BYTES(*bucket_store));
}

u64 bucket_store_root(const bucket_store *bucket_store)
{
    return (1ULL << (BUCKET_STORE_NUM_LEVELS(*bucket_store) - 1)) - 1;
}

size_t bucket_store_num_levels(const bucket_store *bucket_store)
{
    return BUCKET_STORE_NUM_LEVELS(*bucket_store);
}

size_t bucket_store_capacity_bytes(const bucket_store *bucket_store)
{
    return BLOCK_DATA_SIZE_BYTES * (1ULL << (BUCKET_STORE_NUM_LEVELS(*bucket_store) - 1));
}

size_t bucket_store_num_leaves(const bucket_store *bucket_store)
{
    return 1ULL << (BUCKET_STORE_NUM_LEVELS(*bucket_store) - 1);
}

void bucket_store_read_bucket_blocks(bucket_store *bucket_store, u64 bucket_id, block bucket_data[BLOCKS_PER_BUCKET])
{
    CHECK(bucket_id < tree_path_num_nodes(BUCKET_STORE_NUM_LEVELS(*bucket_store)));
    size_t offset = bucket_id * ENCRYPTED_BUCKET_SIZE;
    u8 *encrypted_bucket = BUCKET_STORE_DATA(*bucket_store) + offset;
    memcpy(bucket_data, encrypted_bucket, BLOCKS_PER_BUCKET * sizeof(block));
}

void bucket_store_write_bucket_blocks(bucket_store *bucket_store, u64 bucket_id, const block bucket_data[BLOCKS_PER_BUCKET]) {
    size_t offset = bucket_id * ENCRYPTED_BUCKET_SIZE;
    u8 *encrypted_bucket_start = BUCKET_STORE_DATA(*bucket_store) + offset;
    memcpy(encrypted_bucket_start, bucket_data,  BLOCKS_PER_BUCKET * sizeof(block));
}


size_t bucket_store_block_data_size(bucket_store *bucket_store)
{
    return BLOCK_DATA_SIZE_QWORDS;
}

bool block_is_empty(block block)
{
    return BLOCK_ID(block) == EMPTY_BLOCK_ID;
}

#ifdef IS_TEST
#include <stdio.h>
#include <time.h>
#include "../include/tests.h"

// Planning to add tests here when using pruned trees and variable branching
void private_bucket_store_tests()
{
    printf("TEST private bucket store functions");
}
#endif
