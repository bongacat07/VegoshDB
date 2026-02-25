/**
 * @file vegosh.h
 * @brief Fixed-size Robin Hood open-addressing hash map with 16-byte keys
 *        and 32-byte values. All slots are cache-line aligned (64 bytes).
 *
 * Usage:
 *   initializevegosh() → insert() / get()
 */

#ifndef VEGOSH_H
#define VEGOSH_H

#include "xxhash.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/** Number of hash table slots. Must be a power of two. */
#define TABLE_SIZE (1 << 21)  /* 2,097,152 slots */

/** Bitmask used in place of modulo for power-of-two table sizes. */
#define MASK (TABLE_SIZE - 1)

/** Hard cap on the number of distinct keys that may be stored. */
#define MAX_KEYS 1000000

/** Slot status: no entry present. */
#define EMPTY  0x00

/** Slot status: entry is present. */
#define OCCUPIED 0x01

/**
 * @struct Slot
 * @brief One entry in the hash table.
 *
 * The struct is padded to exactly 64 bytes so that each slot maps to a
 * single cache line, eliminating false sharing and improving prefetch
 * efficiency on sequential probes.
 *
 * Layout:
 *   key      [0..15]   – raw 16-byte key
 *   value    [16..47]  – raw 32-byte value
 *   hash     [48..51]  – cached lower 32 bits of the XXH3 hash
 *   value_len[52]      – length of the value in bytes
 *   status   [53]      – EMPTY or OCCUPIED
 *   CRC32    [54..57]  – CRC32 checksum of the value
 *   reserved [58..63]  – padding to reach 64 bytes
 *
 *  */
 struct Slot {
     uint8_t  key[16];
     uint8_t  value[32];
     uint32_t hash;
     uint32_t crc32;
     uint8_t  status;
     uint8_t  value_len;
     uint8_t  reserved[6];
 };

/**
 * @brief Allocates and zero-initialises the global hash table.
 * @return 0 on success, -1 if allocation fails.
 */
int initializevegosh(void);

/**
 * @brief Inserts or updates a key-value pair.
 *
 * If the key already exists its value is overwritten without consuming an
 * additional slot. New insertions are rejected once MAX_KEYS is reached.
 *
 * @param key   Pointer to exactly 16 bytes of key data.
 * @param value Pointer to exactly 32 bytes of value data.
 * @return 0 on success, -1 if the table is full or the key cap is reached.
 */
int insert(const uint8_t *key, const uint8_t *value,const uint8_t *value_len);

/**
 * @brief Looks up a key and copies its value into @p out_value.
 *
 * @param key       Pointer to exactly 16 bytes of key data.
 * @param out_value Destination buffer of at least 32 bytes.
 * @return 0 if found (value written to @p out_value), -1 if not found.
 */
int get(const uint8_t *key, uint8_t *out_value, uint8_t *value_len);

#endif /* VEGOSH_H */
