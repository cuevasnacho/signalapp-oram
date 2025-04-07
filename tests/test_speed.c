#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <sys/random.h>

#include "../include/tests.h"
#include "../include/bucket.h"
#include "../include/path_oram.h"

#define CAPACITY 1<<24
#define NRUNS 1330
#define TEST_STASH_SIZE 100
#define ARR_SIZE 1024
#define err_FAILED -1

void oram_access_read_jazz(oram *oram, u64 block_id, u64 *out_data);
void oram_access_write_jazz(oram *oram, u64 block_id, u64 *in_data);
void odd_even_msort_jazz(block* blocks, u64* block_level_assignments, size_t lb, size_t ub);

static inline uint64_t get_cycles() {
  uint32_t low, high;
  __asm__ volatile("rdtsc" : "=a" (low), "=d" (high));
  return ((uint64_t)high << 32) | low;
}

static inline uint64_t cpucycles(void) {
  uint64_t result;

  asm volatile("rdtsc; shlq $32,%%rdx; orq %%rdx,%%rax"
    : "=a" (result) : : "%rdx");

  return result;
}

static int cmp_uint64(const void *a, const void *b) {
  if(*(uint64_t *)a < *(uint64_t *)b) return -1;
  if(*(uint64_t *)a > *(uint64_t *)b) return 1;
  return 0;
}

static uint64_t median(uint64_t *l, size_t llen) {
  qsort(l,llen,sizeof(uint64_t),cmp_uint64);

  if(llen%2) return l[llen/2];
  else return (l[llen/2-1]+l[llen/2])/2;
}

static uint64_t average(uint64_t *t, size_t tlen) {
  size_t i;
  uint64_t acc=0;

  for(i=0;i<tlen;i++)
    acc += t[i];

  return acc/tlen;
}

uint64_t cpucycles_overhead(void) {
  uint64_t t0, t1, overhead = -1LL;
  unsigned int i;

  for(i=0;i<100000;i++) {
    t0 = cpucycles();
    __asm__ volatile ("");
    t1 = cpucycles();
    if(t1 - t0 < overhead)
      overhead = t1 - t0;
  }

  return overhead;
}

void print_results(const char *s, uint64_t *t, size_t tlen) {
  size_t i;
  static uint64_t overhead = -1;

  if(overhead == (uint64_t)-1)
    overhead = cpucycles_overhead();

  tlen--;
  for(i=0;i<tlen;++i)
    t[i] = t[i+1] - t[i] - overhead;

  printf("%s\n", s);
  printf("median: %llu cycles/ticks\n", (unsigned long long)median(t, tlen));
  printf("average: %llu cycles/ticks\n", (unsigned long long)average(t, tlen));
  printf("\n");
}

void print_results_jasmin(const char *s, uint64_t *t, size_t tlen) {
  printf("%s\n", s);
  printf("median: %llu cycles/ticks\n", (unsigned long long)median(t, tlen));
  printf("average: %llu cycles/ticks\n", (unsigned long long)average(t, tlen));
  printf("\n");
}

int test_sort() {
  // benchmark variables
  uint64_t t0, t1, i;
  size_t ri;

  // test variables
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
    BLOCK_ID(original_blocks[i]) = i;
    BLOCK_POSITION(original_blocks[i]) = i;
  }

  block blocks[ARR_SIZE];
  u64 bucket_assignments[ARR_SIZE];
  memcpy(blocks, original_blocks, sizeof(original_blocks));
  memcpy(bucket_assignments, original_bucket_assignments, sizeof(original_bucket_assignments));

  block blocks_jazz[ARR_SIZE];
  u64 bucket_assignments_jazz[ARR_SIZE];
  memcpy(blocks_jazz, original_blocks, sizeof(original_blocks));
  memcpy(bucket_assignments_jazz, original_bucket_assignments, sizeof(original_bucket_assignments));

  // Measure time and cycles for C version
  t0 = get_cycles();
  bitonic_sort(blocks, bucket_assignments, 0, ARR_SIZE, true);
  t1 = get_cycles();
  printf("bitonic sort: %lu cycles\n", t1 - t0);
  
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
  }
  printf("\n");
  
  // Measure time and cycles for C version
  t0 = get_cycles();
  odd_even_msort_jazz(blocks_jazz, bucket_assignments_jazz, 0, ARR_SIZE);
  t1 = get_cycles();
  printf("odd even msort: %lu cycles\n", t1 - t0);
  
  for(size_t i = 1; i < num_blocks; ++i) {
    // check that it is sorted
    TEST_ASSERT(bucket_assignments_jazz[i-1] <= bucket_assignments_jazz[i]);
    // and that the blocks and bucket assignments moved together
    bool found = false;
    for(size_t j = 0; j < num_blocks; ++j) {
      if(BLOCK_ID(blocks_jazz[i]) == BLOCK_ID(original_blocks[j])) {
        found = true;
        TEST_ASSERT(bucket_assignments_jazz[i] == original_bucket_assignments[j]);
        break;
      }
    }
    TEST_ASSERT(found);
  }
  printf("\n");

  return err_SUCCESS;
}

int test_oram() {
  // benchmark variables
  uint64_t t[NRUNS], i;
  size_t ri;

  // initialize oram
  oram *oram0 = oram_create(CAPACITY, TEST_STASH_SIZE, getentropy);
  oram_allocate_contiguous(oram0, NRUNS);
  oram_allocate_block(oram0);

  oram *oram1 = oram_create(CAPACITY, TEST_STASH_SIZE, getentropy);
  oram_allocate_contiguous(oram1, NRUNS);
  oram_allocate_block(oram1);

  u64 buf[BLOCK_DATA_SIZE_QWORDS];
  for (size_t i = 0; i < BLOCK_DATA_SIZE_QWORDS; ++i)
  {
    buf[i] = BLOCK_DATA_SIZE_QWORDS + i;
  }

  // test put
  for(i=0;i<=NRUNS;i++)
  {
    t[i] = cpucycles();
    oram_put(oram0, i, buf);
  }
  print_results("oram_put: ", t, NRUNS);

  // test get
  for(i=0;i<=NRUNS;i++)
  {
    t[i] = cpucycles();
    oram_get(oram0, i, buf);
  }
  print_results("oram_get: ", t, NRUNS);

  // test put
  for(i=0;i<=NRUNS;i++)
  {
    t[i] = cpucycles();
    oram_access_write_jazz(oram1, i, buf);
  }
  print_results("oram_put_jazz: ", t, NRUNS);
  
  // test get jazz
  for(i=0;i<=NRUNS;i++)
  {
    t[i] = cpucycles();
    oram_access_read_jazz(oram1, i, buf);
  }
  print_results("oram_get_jazz: ", t, NRUNS);
  
  oram_destroy(oram0);
  oram_destroy(oram1);

  return err_SUCCESS;
}

int main(void)
{
  test_sort();
  test_oram();
  return 0;
}