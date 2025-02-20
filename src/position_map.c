// Copyright 2022 Signal Messenger, LLC
// SPDX-License-Identifier: AGPL-3.0-only

#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../include/position_map.h"
#include "../include/path_oram.h"
#include "../include/bucket.h"

#include <stdio.h>

// The most basic position map uses a linear scan to provide oblivious RAM security.
// We will want to use this for 2^14 or fewer entries, then use an ORAM
// backed position_map for more.
/*
typedef struct
{
    size_t size;
    u64 *data;
} scan_position_map;


typedef struct
{
    size_t size;
    oram *oram;
    u64 base_block_id;
    u64 *access_buf;
} oram_position_map;
*/

typedef enum
{
    scan_map,
    oram_map
} position_map_type;

/*
struct position_map
{
    position_map_type type;
    size_t num_positions;
    union
    {
        scan_position_map scan_position_map;
        oram_position_map oram_position_map;
    } impl;
};
*/
typedef u64 oram_position_map[4];
typedef u64 scan_position_map[2];

// position_map
#define POSITION_MAP_TYPE(o)            ((o)[0])
#define POSITION_MAP_NUM_POSITIONS(o)   ((o)[1])
#define POSITION_MAP_SIZE(o)            ((o)[2])
#define POSITION_MAP_DATA(o)            ((o)[3])
#define POSITION_MAP_BASE_BLOCK_ID(o)   ((o)[4])
#define POSITION_MAP_ACCESS_BUF(o)      ((o)[5])

// oram_position_map
#define ORAM_POSITION_MAP_SIZE(o)            ((o)[0])
#define ORAM_POSITION_MAP_ORAM(o)            ((o)[1])
#define ORAM_POSITION_MAP_BASE_BLOCK_ID(o)   ((o)[2])
#define ORAM_POSITION_MAP_ACCESS_BUF(o)      ((o)[3])

// scan_position_map
#define SCAN_POSITION_MAP_SIZE(o)            ((o)[0])
#define SCAN_POSITION_MAP_DATA(o)            ((o)[1])

// oram implementation
static oram_position_map *oram_position_map_create(size_t num_blocks, size_t num_positions, size_t overflow_stash_size, entropy_func getentropy)
{
    CHECK(num_positions <= num_blocks);
    // oram capacity is measured in u64s
    oram *oram = oram_create(num_blocks, overflow_stash_size, getentropy);
    size_t block_size = oram_block_size(oram);
    size_t blocks_needed = num_blocks / block_size + ((num_blocks % block_size == 0) ? 0 : 1);
    u64 base_block_id = oram_allocate_contiguous(oram, blocks_needed);
    //TEST_LOG("oram_position_map size: %zu blocks: %zu", num_blocks, blocks_needed);
    // initialize position map with random data
    u64 *buf;
    CHECK(buf = calloc(block_size, sizeof(*buf)));
    for (size_t i = 0; i < blocks_needed; ++i)
    {
        for (size_t j = 0; j < block_size; ++j)
        {
            getentropy(buf + j, sizeof(*buf));
            buf[j] = buf[j] % num_positions;
        }
        oram_put(oram, base_block_id + i, buf);
    }

    oram_position_map *result = calloc(6, sizeof(u64));
    ORAM_POSITION_MAP_SIZE(*result) = num_blocks;
    ORAM_POSITION_MAP_ORAM(*result) = oram;
    ORAM_POSITION_MAP_BASE_BLOCK_ID(*result) = base_block_id;
    ORAM_POSITION_MAP_ACCESS_BUF(*result) = buf;

    return result;
}

static void oram_position_map_destroy(oram_position_map *oram_position_map)
{
    oram_destroy(ORAM_POSITION_MAP_ORAM(*oram_position_map));
    free(ORAM_POSITION_MAP_ACCESS_BUF(*oram_position_map));
}

static u64 block_id_for_index(const oram_position_map *oram_position_map, u64 index)
{
    size_t entries_per_block = oram_block_size(ORAM_POSITION_MAP_ORAM(*oram_position_map));
    return ORAM_POSITION_MAP_BASE_BLOCK_ID(*oram_position_map) + (index / entries_per_block);
}

static error_t oram_position_map_get(const oram_position_map *oram_position_map, u64 block_id, u64* position)
{
    size_t block_size = oram_block_size(ORAM_POSITION_MAP_ORAM(*oram_position_map));
    u64 *buf = ORAM_POSITION_MAP_ACCESS_BUF(*oram_position_map);
    RETURN_IF_ERROR(oram_get(ORAM_POSITION_MAP_ORAM(*oram_position_map), block_id_for_index(oram_position_map, block_id), buf));
    
    *position = buf[block_id % block_size];
    return err_SUCCESS;
}

static error_t oram_position_map_set(oram_position_map *oram_position_map, u64 block_id, u64 position, u64 *prev_position)
{
    CHECK(prev_position!= NULL);
    size_t block_size = oram_block_size(ORAM_POSITION_MAP_ORAM(*oram_position_map));
    u64 *buf = ORAM_POSITION_MAP_ACCESS_BUF(*oram_position_map);

    size_t idx_in_block = block_id % block_size;
    size_t len_to_put = 1;
    RETURN_IF_ERROR(oram_put_partial(ORAM_POSITION_MAP_ORAM(*oram_position_map), block_id_for_index(oram_position_map, block_id), idx_in_block, len_to_put, &position, buf));
    
    *prev_position = buf[idx_in_block];

    return err_SUCCESS;
}

static size_t oram_position_map_capacity(oram_position_map *oram_position_map)
{
    return ORAM_POSITION_MAP_SIZE(*oram_position_map);
}

// scan implementation
static scan_position_map *scan_position_map_create(size_t size, size_t num_positions, entropy_func getentropy)
{
    u64 *data;
    CHECK(data = calloc(size, sizeof(*data)));
    //TEST_LOG("scan_position_map size: %zu", size);
    scan_position_map *scan_position_map = calloc(2, sizeof(u64));
    SCAN_POSITION_MAP_SIZE(*scan_position_map) = size;
    SCAN_POSITION_MAP_DATA(*scan_position_map) = data;

    for (size_t i = 0; i < size; ++i)
    {
        getentropy(data + i, sizeof(*data));
        data[i] = data[i] % num_positions;
    }
    return scan_position_map;
}

static void scan_position_map_destroy(scan_position_map *scan_position_map)
{
    free(SCAN_POSITION_MAP_DATA(*scan_position_map));
}

static error_t scan_position_map_get(const scan_position_map *scan_position_map, u64 block_id, u64* position)
{  
    CHECK(block_id < SCAN_POSITION_MAP_SIZE(*scan_position_map));
    // linear scan of array so that every access looks the same.
    for (size_t i = 0; i < SCAN_POSITION_MAP_SIZE(*scan_position_map); ++i)
    {
        bool cond = (i == block_id);
        cond_obv_cpy_u64(cond, position, (u64*)SCAN_POSITION_MAP_DATA(*scan_position_map) + i);
    }
    return err_SUCCESS;
}

static error_t scan_position_map_set(scan_position_map *scan_position_map, u64 block_id, u64 position, u64 *prev_position)
{
    CHECK(block_id < SCAN_POSITION_MAP_SIZE(*scan_position_map));
    u64 tmp;
    prev_position = prev_position ? prev_position : &tmp;
    *prev_position = position;
    // linear scan of array so that every access looks the same.
    for (size_t i = 0; i < SCAN_POSITION_MAP_SIZE(*scan_position_map); ++i)
    {
        bool cond = (i == block_id);
        cond_obv_swap_u64(cond, prev_position, (u64*)SCAN_POSITION_MAP_DATA(*scan_position_map) + i);
    }
    return err_SUCCESS;
}

static size_t scan_position_map_capacity(scan_position_map *scan_position_map)
{
    return SCAN_POSITION_MAP_SIZE(*scan_position_map);
}

// position_map public interface
position_map *position_map_create(size_t size, size_t num_positions, size_t overflow_stash_size, entropy_func getentropy)
{
    position_map *result;
    CHECK(result = calloc(1, sizeof(*result)));
    POSITION_MAP_NUM_POSITIONS(*result) = num_positions;
    // Acceptable if: this is not executed in an oram_access
    if (size > SCAN_THRESHOLD)
    {
        POSITION_MAP_TYPE(*result) = oram_map;
        oram_position_map *oram = oram_position_map_create(size, num_positions, overflow_stash_size, getentropy);
        POSITION_MAP_SIZE(*result) = ORAM_POSITION_MAP_SIZE(*oram);
        POSITION_MAP_DATA(*result) = ORAM_POSITION_MAP_ORAM(*oram);
        POSITION_MAP_BASE_BLOCK_ID(*result) = ORAM_POSITION_MAP_BASE_BLOCK_ID(*oram);
        POSITION_MAP_ACCESS_BUF(*result) = ORAM_POSITION_MAP_ACCESS_BUF(*oram);
        free(oram);
    }
    else
    {
        POSITION_MAP_TYPE(*result) = scan_map;
        scan_position_map *scan = scan_position_map_create(size, num_positions, getentropy);
        POSITION_MAP_SIZE(*result) = SCAN_POSITION_MAP_SIZE(*scan);
        POSITION_MAP_DATA(*result) = SCAN_POSITION_MAP_DATA(*scan);
        free(scan);
    }
    return result;
}

void position_map_destroy(position_map *position_map)
{
    // Acceptable if: this is not executed in an oram_access
    if (position_map)
    {
        switch (POSITION_MAP_TYPE(*position_map))
        {
        case scan_map:
            scan_position_map_destroy(&POSITION_MAP_SIZE(*position_map));
            break;
        case oram_map:
            oram_position_map_destroy(&POSITION_MAP_SIZE(*position_map));
            break;
        default:
            CHECK(false);
            break;
        }
        free(position_map);
    }
}

error_t position_map_get(const position_map *position_map, u64 block_id, u64* position)
{
    // Acceptable switch: executed identically in each oram_access
    switch (POSITION_MAP_TYPE(*position_map))
    {
    case scan_map:
        RETURN_IF_ERROR(scan_position_map_get(&POSITION_MAP_SIZE(*position_map), block_id, position));
        break;
    case oram_map:
        RETURN_IF_ERROR(oram_position_map_get(&POSITION_MAP_SIZE(*position_map), block_id, position));
        break;
    default:
        CHECK(false);
        break;
    }
    return err_SUCCESS;
}

error_t position_map_read_then_set(position_map *position_map, u64 block_id, u64 position, u64 *prev_position)
{
    // Acceptable switch: executed identically in each oram_access
    switch (POSITION_MAP_TYPE(*position_map))
    {
    case scan_map:
        return scan_position_map_set(&POSITION_MAP_SIZE(*position_map), block_id, position, prev_position);
    case oram_map:
        return oram_position_map_set(&POSITION_MAP_SIZE(*position_map), block_id, position, prev_position);
    default:
        CHECK(false);
        break;
    }
}

size_t position_map_capacity(const position_map *position_map)
{
    u64 result = 0;
    // Acceptable switch: not executed in an oram_access
    switch (POSITION_MAP_TYPE(*position_map))
    {
    case scan_map:
        result = scan_position_map_capacity(&POSITION_MAP_SIZE(*position_map));
        break;
    case oram_map:
        result = oram_position_map_capacity(&POSITION_MAP_SIZE(*position_map));
        break;
    default:
        CHECK(false);
        break;
    }
    return result;
}

size_t position_map_recursion_depth(const position_map* position_map) {

    const oram_statistics* oram_stats = NULL;
    // Acceptable switch: not executed in an oram_access
    switch (POSITION_MAP_TYPE(*position_map))
    {
    case scan_map:
        return 1;
    case oram_map:
        oram_stats = oram_report_statistics(ORAM_POSITION_MAP_ORAM(&POSITION_MAP_SIZE(*position_map)));
        return 1 + oram_stats->recursion_depth;
    default:
        CHECK(false);
    }
    return 0;
}

const oram_statistics* position_map_oram_statistics(position_map* position_map) {
    // Acceptable switch: not executed in oram_access
    switch (POSITION_MAP_TYPE(*position_map))
    {
    case scan_map:
        return NULL;
    case oram_map:
        return oram_report_statistics(ORAM_POSITION_MAP_ORAM(&POSITION_MAP_SIZE(*position_map)));
    default:
        CHECK(false);
    }
    return NULL;
}


size_t position_map_size_bytes(size_t num_blocks, size_t stash_overflow_size) {
    // Acceptable if: this is not executed in an oram_access
    if(num_blocks > SCAN_THRESHOLD) {
        size_t blocks_needed = num_blocks / BLOCK_DATA_SIZE_QWORDS + 1;
        size_t num_levels = floor_log2(blocks_needed);
        return oram_size_bytes(num_levels, blocks_needed, stash_overflow_size) + sizeof(position_map);
    }
    
    return num_blocks * sizeof(u64) + sizeof(position_map);
    

}
