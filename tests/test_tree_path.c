// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <inttypes.h>
#include <assert.h>
#include <stdio.h>

#include "../include/tree_path.h"
#include "../include/util.h"
#include "../include/tests.h"

int test_path(u64 leaf, u64 root, size_t expected_len, const u64 expected_path[expected_len])
{
    tree_path *path0 = tree_path_create(leaf, root);
    tree_path *path1 = tree_path_create_jazz(leaf, root);
    assert(path0->length == expected_len);
    assert(path1->length == expected_len);
    for (size_t i = 0; i < expected_len; ++i)
    {
        TEST_ASSERT(expected_path[i] == path0->values[i]);
        TEST_ASSERT(expected_path[i] == path1->values[i]);
    }
    tree_path_destroy(path0);
    tree_path_destroy(path1);
    return err_SUCCESS;
}

int test_paths()
{
    u64 e1[] = {2046, 2045, 2043, 2039, 2031, 2015, 1983, 1919, 1791, 1535, 1023};
    test_path(2046, 1023, 11, e1);

    u64 e2[] = {0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023};
    test_path(0, 1023, 11, e2);

    u64 e3[] = {878, 877, 875, 871, 879, 863, 831, 895, 767, 511, 1023};
    test_path(878, 1023, 11, e3);

    u64 e4[] = {146, 145, 147, 151, 143, 159, 191, 127, 255, 511, 1023};
    test_path(146, 1023, 11, e4);

    u64 e5[] = {132, 133, 131, 135, 143, 159, 191, 127, 255, 511, 1023};
    test_path(132, 1023, 11, e5);

    u64 e6[] = {1814, 1813, 1811, 1815, 1807, 1823, 1855, 1919, 1791, 1535, 1023};
    test_path(1814, 1023, 11, e6);

    u64 e7[] = {1720, 1721, 1723, 1719, 1711, 1695, 1727, 1663, 1791, 1535, 1023};
    test_path(1720, 1023, 11, e7);

    return err_SUCCESS;
}

void public_tree_path_tests()
{
    RUN_TEST(test_paths());
}

int main()
{
    private_tree_path_tests();
    public_tree_path_tests();
    return 0;
}
