#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <sys/random.h>
#include <time.h>
#include "../include/stash.h"

#define err_SUCCESS 0
#define err_FAILED 1
#define TEST_ASSERT(cond) \
  do { \
    if (!(cond)) { \
      fprintf(stderr, "Test failed: %s\n", #cond); \
      return err_FAILED; \
    } \
  } while (0)

#define GET_TIME(start, end) (((end) - (start)) / CLOCKS_PER_SEC)
#define GET_CYCLES(start, end) ((end) - (start))

#define ARR_SIZE 1024

typedef uint64_t u64;

static inline uint64_t get_cycles() {
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t)high << 32) | low;
}

void selection_sort_jazz(block* blocks, u64* block_level_assignments, size_t lb, size_t ub, bool direction);

int test_sort() {
  size_t num_blocks = ARR_SIZE;
  block original_blocks[ARR_SIZE];
  u64 original_bucket_assignments[ARR_SIZE];

  ssize_t result = getrandom(original_bucket_assignments, sizeof(original_bucket_assignments), 0);
  if (result == -1) {
    perror("getrandom failed");
    return err_FAILED;
  }
  for (size_t i = 0; i < ARR_SIZE; ++i)
    original_bucket_assignments[i] = original_bucket_assignments[i] % ARR_SIZE;
  for (size_t i = 0; i < ARR_SIZE; ++i) {
    original_blocks[i].id = i;
    original_blocks[i].position = i;
  }

  block blocks[ARR_SIZE];
  u64 bucket_assignments[ARR_SIZE];
  memcpy(blocks, original_blocks, sizeof(original_blocks));
  memcpy(bucket_assignments, original_bucket_assignments, sizeof(original_bucket_assignments));

  block blocks_jazz[ARR_SIZE];
  u64 bucket_assignments_jazz[ARR_SIZE];
  memcpy(blocks_jazz, original_blocks, sizeof(original_blocks));
  memcpy(bucket_assignments_jazz, original_bucket_assignments, sizeof(original_bucket_assignments));

  clock_t start_time, end_time;
  uint64_t start_cycles, end_cycles;

  // Measure time and cycles for C version
  start_time = clock();
  start_cycles = get_cycles();
  bitonic_sort(blocks, bucket_assignments, 0, ARR_SIZE, true);
  end_cycles = get_cycles();
  end_time = clock();

  printf("C sort:\n");
  printf("Time: %lf seconds\n", GET_TIME(start_time, end_time));
  printf("Cycles: %llu\n", GET_CYCLES(start_cycles, end_cycles));
  
  for(size_t i = 1; i < num_blocks; ++i) {
    // check that it is sorted
    TEST_ASSERT(bucket_assignments[i-1] <= bucket_assignments[i]);
    // and that the blocks and bucket assignments moved together
    bool found = false;
    for(size_t j = 0; j < num_blocks; ++j) {
      if(blocks[i].id == original_blocks[j].id) {
        found = true;
        TEST_ASSERT(bucket_assignments[i] == original_bucket_assignments[j]);
        break;
      }
    }
    TEST_ASSERT(found);
  }
  printf("\n");

  // Measure time and cycles for Jasmin version
  start_time = clock();
  start_cycles = get_cycles();
  selection_sort_jazz(blocks_jazz, bucket_assignments_jazz, 0, ARR_SIZE, true);
  end_cycles = get_cycles();
  end_time = clock();

  printf("Jasmin sort:\n");
  printf("Time: %lf seconds\n", GET_TIME(start_time, end_time));
  printf("Cycles: %llu\n", GET_CYCLES(start_cycles, end_cycles));

  for(size_t i = 0; i < num_blocks; ++i) {
    TEST_ASSERT(bucket_assignments_jazz[i] == bucket_assignments[i]);
    TEST_ASSERT(blocks_jazz[i].id == blocks[i].id);
  }
  printf("\n");

  return err_SUCCESS;
}

int main() {
  int result = test_sort();
  if (result == err_SUCCESS) {
    printf("Test passed!\n");
  }
  return result;
}
