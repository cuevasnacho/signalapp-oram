
// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>

#include "../include/stash.h"
#include "../include/util.h"
#include "../include/tests.h"

#include "../include/tree_path.h"


static int test_stash_lifecycle()
{
    stash *stash = stash_create(20, 30);
    TEST_ASSERT(stash != 0);
    stash_destroy(stash);
    return err_SUCCESS;
}

static void public_stash_tests()
{
    RUN_TEST(test_cond_cpy_block());
    RUN_TEST(test_oblv_sort());
    RUN_TEST(test_stash_lifecycle());
    RUN_TEST(test_stash_insert_read());
    RUN_TEST(test_fill_stash());
    RUN_TEST(test_load_bucket_path_to_stash(bucket_density_full));
    RUN_TEST(test_load_bucket_path_to_stash(bucket_density_dense));
    RUN_TEST(test_load_bucket_path_to_stash(bucket_density_sparse));

    // Jasmin tests
    RUN_TEST(test_cond_cpy_block_jazz());
    RUN_TEST(test_oblv_sort_jazz());
    RUN_TEST(test_stash_insert_read_jazz());
    RUN_TEST(test_fill_stash_jazz());
    RUN_TEST(test_load_bucket_path_to_stash_jazz(bucket_density_full));
    RUN_TEST(test_load_bucket_path_to_stash_jazz(bucket_density_dense));
    RUN_TEST(test_load_bucket_path_to_stash_jazz(bucket_density_sparse));
}

int main()
{
    public_stash_tests();
    return 0;
}
