// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#include "../include/position_map.h"
#include "../include/bucket.h"
#include "../include/util.h"
#include "../include/tests.h"

void position_map_get_jazz(const position_map *position_map, u64 block_id, u64* position);
void position_map_read_then_set_jazz(position_map *position_map, u64 block_id, u64 position, u64 *prev_position);

int test_position_map_lifecycle()
{
    position_map *pm = position_map_create(1 << 18, 1<<17, TEST_STASH_SIZE, getentropy);
    TEST_ASSERT(pm != 0);
    position_map_destroy(pm);

    return err_SUCCESS;
}

int test_position_map_recursion_depth() {
    position_map* pm = position_map_create(SCAN_THRESHOLD, SCAN_THRESHOLD, TEST_STASH_SIZE, getentropy);
    TEST_ASSERT(position_map_recursion_depth(pm) == 1);
    position_map_destroy(pm);

    pm = position_map_create(SCAN_THRESHOLD + 1, SCAN_THRESHOLD + 1, TEST_STASH_SIZE, getentropy);
    TEST_ASSERT(position_map_recursion_depth(pm) == 2);
    position_map_destroy(pm);

    pm = position_map_create(SCAN_THRESHOLD*BLOCK_DATA_SIZE_QWORDS + 1, SCAN_THRESHOLD*BLOCK_DATA_SIZE_QWORDS + 1, TEST_STASH_SIZE, getentropy);
    TEST_ASSERT(position_map_recursion_depth(pm) == 3);
    position_map_destroy(pm);

    return err_SUCCESS;
}

static int cmpu64(const void *pa, const void *pb)
{
    u64 a = *(u64 *)pa;
    u64 b = *(u64 *)pb;
    return (a > b) - (b > a);
}

int test_position_map_initial_data()
{
    size_t size = 1 << 14;
    position_map *pm = position_map_create(size, size, TEST_STASH_SIZE, getentropy);
    u64 *data = calloc(size, sizeof(*data));
    for (size_t i = 0; i < size; ++i)
    {
        RETURN_IF_ERROR(position_map_get(pm, i, data + i));
    }

    qsort(data, size, sizeof(*data), cmpu64);

    size_t num_repeat_histogram[20];
    memset(num_repeat_histogram, 0, 20 * sizeof(*num_repeat_histogram));

    u64 curr_item = data[0];
    size_t run_length = 1;
    for (size_t i = 0; i < size; ++i)
    {
        if (curr_item == data[i])
        {
            ++run_length;
        }
        else
        {
            if (run_length >= 20)
            {
                fprintf(stderr, "WARNING encountered a long run length: %" PRIu64 " occured %zu times\n", curr_item, run_length);
                run_length = 19;
            }
            num_repeat_histogram[run_length]++;
            run_length = 1;
            curr_item = data[i];
        }
    }
    free(data);
    position_map_destroy(pm);

    return err_SUCCESS;
}

int test_position_map_put_get()
{
    size_t capacity_u64 = 1 << 24;
    size_t num_blocks = (capacity_u64 / BLOCK_DATA_SIZE_QWORDS) + (capacity_u64 % BLOCK_DATA_SIZE_QWORDS == 0 ? 0 : 1);
    position_map *pm0 = position_map_create(num_blocks, num_blocks, TEST_STASH_SIZE, getentropy);
    position_map *pm1 = position_map_create(num_blocks, num_blocks, TEST_STASH_SIZE, getentropy);

    u64 prev;
    RETURN_IF_ERROR(position_map_read_then_set(pm0, 1234, 4321, &prev));
    position_map_read_then_set_jazz(pm1, 1234, 4321, &prev);

    u64 result0, result1;
    RETURN_IF_ERROR(position_map_get(pm0, 1234, &result0));
    position_map_get_jazz(pm1, 1234, &result1);

    TEST_ASSERT(result0 == 4321);
    TEST_ASSERT(result1 == 4321);

    position_map_destroy(pm0);
    position_map_destroy(pm1);

    return err_SUCCESS;
}

int test_position_map_put_get_repeat()
{
    size_t capacity_u64 = 1 << 24;
    size_t num_blocks = (capacity_u64 / BLOCK_DATA_SIZE_QWORDS) + (capacity_u64 % BLOCK_DATA_SIZE_QWORDS == 0 ? 0 : 1);
    position_map *pm0 = position_map_create(num_blocks, num_blocks, TEST_STASH_SIZE, getentropy);
    position_map *pm1 = position_map_create(num_blocks, num_blocks, TEST_STASH_SIZE, getentropy);

    for (size_t i = 0; i < num_blocks; ++i)
    {
        u64 prev;
        RETURN_IF_ERROR(position_map_read_then_set(pm0, i, i, &prev));
        position_map_read_then_set_jazz(pm1, i, i, &prev);
    }

    for (size_t i = 0; i < num_blocks; ++i)
    {
        u64 result0, result1;
        RETURN_IF_ERROR(position_map_get(pm0, i, &result0));
        RETURN_IF_ERROR(position_map_get(pm1, i, &result1));
        TEST_ASSERT(result0 == i);
        TEST_ASSERT(result1 == i);
    }
    for (size_t i = 0; i < num_blocks; ++i)
    {
        u64 old_pos0 = 0, old_pos1 = 0;
        RETURN_IF_ERROR(position_map_read_then_set(pm0, i, num_blocks - i, &old_pos0));
        position_map_read_then_set_jazz(pm1, i, num_blocks - i, &old_pos1);
        TEST_ASSERT(old_pos0 == i);
        TEST_ASSERT(old_pos1 == i);
    }

    for (size_t i = 0; i < num_blocks; ++i)
    {
        u64 result0, result1;
        RETURN_IF_ERROR(position_map_get(pm0, i, &result0));
        RETURN_IF_ERROR(position_map_get(pm1, i, &result1));
        TEST_ASSERT(result0 == num_blocks - i);
        TEST_ASSERT(result1 == num_blocks - i);
    }
    position_map_destroy(pm0);
    position_map_destroy(pm1);

    return err_SUCCESS;
}
void public_position_map_tests()
{
    RUN_TEST(test_position_map_lifecycle());
    RUN_TEST(test_position_map_recursion_depth());
    RUN_TEST(test_position_map_initial_data());
    RUN_TEST(test_position_map_put_get());
    RUN_TEST(test_position_map_put_get_repeat());
}

int main()
{
    public_position_map_tests();

    return 0;
}
