// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include "../include/util.h"
#include "../include/tests.h"

#include <inttypes.h>
#include <sys/random.h>

u64 muluh64_jazz(u64 a, u64 b);
u64 ct_div_jazz(u64 n, u64 d, u64 m_prime, size_t shift1, size_t shift2);

int test_muluh() {
    u64 a = 1000*(1ul << 32) + 1001;
    u64 b = 1729*(1ul << 32) + 999;

    u64 uh = muluh64(a,b);
    TEST_ASSERT(uh == 1729000);
    u64 uh_jazz = muluh64_jazz(a,b);
    TEST_ASSERT(uh_jazz == 1729000);

    for(size_t i = 0; i < 100; ++i) {
        getentropy(&a, sizeof(a));
        getentropy(&b, sizeof(b));
        uh = muluh64(a,b);
        uh_jazz = muluh64_jazz(a,b);

        u64 ah = (a >> 32);
        u64 bh = (b >> 32);
        u64 al = a %(1ul << 32);
        u64 bl = b % (1ul << 32);
        u64 lot_overflow = (((ah*bl) % (1ul << 32)) + ((al*bh) % ((1ul << 32))) + ((al*bl)>>32)) >> 32;
        u64 uh_direct = ah*bh + ((ah*bl >> 32) + (al*bh >> 32)) + lot_overflow;
        TEST_ASSERT(a = ah*(1ul << 32) + al);
        TEST_ASSERT(b = bh*(1ul << 32) + bl);
        TEST_ASSERT(al < (1ul << 32));
        TEST_ASSERT(bl < (1ul << 32));
        TEST_ASSERT(uh == uh_direct);
        TEST_ASSERT(uh_jazz == uh_direct);
    }
    return err_SUCCESS;
}

int test_ct_div() {
    size_t num_divisors = 100;
    u64 d = 1729;
    u64 m_prime;

    size_t shift1;
    size_t shift2;

    u64 r;

    for(size_t divisor = 0; divisor < num_divisors; ++divisor){
    prep_ct_div(d, &m_prime, &shift1, &shift2);

    for(size_t i = 0; i < 10000; ++i) {
        getentropy(&r, sizeof(r));
        u64 q = ct_div(r, d, m_prime, shift1, shift2);
        u64 q_jazz = ct_div(r, d, m_prime, shift1, shift2);
        TEST_ASSERT(q == r/d);
        TEST_ASSERT(q_jazz == r/d);

    }

    // set d to be a new random u64
    getentropy(&d, sizeof(d));
    }
    return err_SUCCESS;
}

int main(int argc, char** argv) {
    RUN_TEST(test_muluh());
    RUN_TEST(test_ct_div());
    return 0;
}
