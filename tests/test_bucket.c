// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include "../include/bucket.h"
#include "../include/tests.h"

int test_bucket_store_lifecycle()
{

    bucket_store *store = bucket_store_create(11);
    assert(store != 0);
    bucket_store_destroy(store);

    return 0;
}

int test_bucket_store_put_get()
{
    bucket_store *store0 = bucket_store_create(11);
    bucket_store *store1 = bucket_store_create(11);

    block block1 = {.id = 1331};
    block block2 = {.id = 1332};
    block empty_block = {.id = EMPTY_BLOCK_ID};
    block blocks[3] = {block1, block2, empty_block};
    assert(DECRYPTED_BLOCK_SIZE >= sizeof(block1));

    u64 bucket_id = 1234;
    bucket_store_write_bucket_blocks(store0, bucket_id, blocks);
    bucket_store_write_bucket_blocks_jazz(store1, bucket_id, blocks);

    u8 bucket_data0[DECRYPTED_BUCKET_SIZE];
    u8 bucket_data1[DECRYPTED_BUCKET_SIZE];
    block* new_blocks0 = (block*)bucket_data0;
    block* new_blocks1 = (block*)bucket_data1;
    bucket_store_read_bucket_blocks(store0, bucket_id, new_blocks0);
    bucket_store_read_bucket_blocks_jazz(store1, bucket_id, new_blocks1);
    assert(new_blocks0[0].id == block1.id);
    assert(new_blocks0[1].id == block2.id);
    assert(new_blocks1[0].id == block1.id);
    assert(new_blocks1[1].id == block2.id);

    for (size_t i = 2; i < BLOCKS_PER_BUCKET; ++i)
    {
        assert(block_is_empty(new_blocks0[i]));
        assert(block_is_empty(new_blocks1[i]));
    }

    // now check the data
    for (int i = 0; i < BLOCK_DATA_SIZE_QWORDS; ++i)
    {
        assert(new_blocks0[0].data[i] == block1.data[i]);
        assert(new_blocks0[1].data[i] == block2.data[i]);
        assert(new_blocks1[0].data[i] == block1.data[i]);
        assert(new_blocks1[1].data[i] == block2.data[i]);
    }

    bucket_store_destroy(store0);
    bucket_store_destroy(store1);

    return 0;
}

int test_bucket_store_clear()
{

    bucket_store *store0 = bucket_store_create(11);
    bucket_store *store1 = bucket_store_create(11);

    block block1 = {.id = 1331};
    block block2 = {.id = 1332};
    block empty_block = {.id = EMPTY_BLOCK_ID};
    block blocks[3] = {block1, block2, empty_block};
    assert(DECRYPTED_BLOCK_SIZE >= sizeof(block1));

    u64 bucket_id = 1234;

    bucket_store_write_bucket_blocks(store0, bucket_id, blocks);
    bucket_store_write_bucket_blocks_jazz(store1, bucket_id, blocks);


    u8 bucket_data0[DECRYPTED_BUCKET_SIZE];
    u8 bucket_data1[DECRYPTED_BUCKET_SIZE];
    block* new_blocks0 = (block*)bucket_data0;
    block* new_blocks1 = (block*)bucket_data1;
    bucket_store_read_bucket_blocks(store0, bucket_id, new_blocks0);
    bucket_store_read_bucket_blocks_jazz(store1, bucket_id, new_blocks1);

    assert(new_blocks0[0].id == block1.id);
    assert(new_blocks0[1].id == block2.id);
    assert(new_blocks1[0].id == block1.id);
    assert(new_blocks1[1].id == block2.id);
    for (size_t i = 2; i < BLOCKS_PER_BUCKET; ++i)
    {
        assert(block_is_empty(new_blocks0[i]));
        assert(block_is_empty(new_blocks1[i]));
    }

    // now check the data
    for (int i = 0; i < BLOCK_DATA_SIZE_QWORDS; ++i)
    {
        assert(new_blocks0[0].data[i] == block1.data[i]);
        assert(new_blocks0[1].data[i] == block2.data[i]);
        assert(new_blocks1[0].data[i] == block1.data[i]);
        assert(new_blocks1[1].data[i] == block2.data[i]);
    }
    for (size_t i = 2; i < BLOCKS_PER_BUCKET; ++i)
    {
        assert(block_is_empty(new_blocks0[i]));
        assert(block_is_empty(new_blocks1[i]));
    }

    // Now clear the bucket store and confirm data is gone
    bucket_store_clear(store0);
    bucket_store_clear_jazz(store1);
    bucket_store_read_bucket_blocks(store0, bucket_id, new_blocks0);
    bucket_store_read_bucket_blocks_jazz(store1, bucket_id, new_blocks1);
    for (size_t i = 0; i < BLOCKS_PER_BUCKET; ++i)
    {
        assert(block_is_empty(new_blocks0[i]));
        assert(block_is_empty(new_blocks1[i]));
    }

    for (int i = 0; i < BLOCK_DATA_SIZE_QWORDS; ++i)
    {
        assert(new_blocks0[0].data[i] == UINT64_MAX);
        assert(new_blocks0[1].data[i] == UINT64_MAX);
        assert(new_blocks1[0].data[i] == UINT64_MAX);
        assert(new_blocks1[1].data[i] == UINT64_MAX);
    }

    bucket_store_destroy(store0);
    bucket_store_destroy(store1);

    return 0;
}

void public_bucket_store_tests()
{
    printf("Public bucket store tests\n");
    RUN_TEST(test_bucket_store_lifecycle());
    RUN_TEST(test_bucket_store_put_get());
    RUN_TEST(test_bucket_store_clear());
}

int main()
{
    private_bucket_store_tests();
    public_bucket_store_tests();
}
