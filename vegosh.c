/**
 * vegosh.c
 * brief Robin Hood open-addressing hash map implementation.
 *
 * Robin Hood hashing minimises probe-length variance by evicting ("stealing
 * from the rich") any entry whose displacement from its home slot is
 * smaller than the displacement of the key currently being inserted. This
 * keeps the maximum probe length short and allows get() to exit early: if
 * the slot we are examining is closer to its home than we are to ours, our
 * key cannot be further along the chain.
 *
 * Hash function: XXH3_64bits (lower 32 bits used as the stored hash).
 * Collision resolution: linear probing with Robin Hood displacement.
 * Slot size: 64 bytes (one cache line) enforced by a compile-time assertion.
 */

#include "vegosh.h"
#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Global table state
 * ---------------------------------------------------------------------- */

/** Pointer to the cache-line-aligned array of slots. */
static struct Slot *vegosh = NULL;

/** Number of unique keys currently stored in the table. */
static size_t vegosh_count = 0;

/* -------------------------------------------------------------------------
 * Initialisation
 * ---------------------------------------------------------------------- */

/**
 * Brief: Allocates and zero-initialises the global hash table.
 *
 * Uses aligned_alloc() to guarantee that the base of the array – and
 * therefore every individual 64-byte slot – is cache-line aligned.
 *
 * @return 0 on success, -1 if allocation fails.
 */
int initializevegosh(void) {
    /* Verify the slot struct is exactly one cache line at compile time. */
    static_assert(sizeof(struct Slot) == 64,
                  "struct Slot must be exactly 64 bytes (one cache line)");
    static_assert(sizeof(struct Slot) % 64 == 0,
                  "struct Slot size must be a multiple of 64 bytes");

    size_t total_size = sizeof(struct Slot) * TABLE_SIZE;

    vegosh = aligned_alloc(64, total_size);
    if (!vegosh) {
        perror("aligned_alloc failed");
        return -1;
    }

    memset(vegosh, 0, total_size); /* mark every slot EMPTY (status = 0) */

    printf("Allocated %zu bytes at %p\n", total_size, (void *)vegosh);
    return 0;
}

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/**
 *  Returns the probe distance of slot @p index from its home bucket.
 *
 * Because the table wraps, subtraction is done modulo TABLE_SIZE using the
 * pre-computed MASK.
 *
 * @param index  Current slot index.
 * @param home Home slot index (hash & MASK) for the entry at @p index.
 * @return Number of steps the entry has been displaced from home.
 */
static inline size_t probe_distance(size_t index, size_t home) {
    return (index + TABLE_SIZE - home) & MASK;
}

/**
 * @brief Atomically swaps the contents of slot @p index with @p temp.
 *
 * After the swap, @p temp holds the old slot contents and vegosh[index]
 * holds what @p temp previously held. The status byte of the newly written
 * slot is explicitly set to OCCUPIED so callers need not worry about it.
 *
 * @param index  Index of the slot to swap with.
 * @param temp Temporary slot buffer used as the exchange partner.
 */
static inline void swap_entry_with_temp(size_t index, struct Slot *temp) {
    struct Slot old;
    memcpy(&old,          &vegosh[index], sizeof(struct Slot));
    memcpy(&vegosh[index],  temp,         sizeof(struct Slot));
    vegosh[index].status = OCCUPIED; /* guarantee the written slot is marked */
    memcpy(temp, &old,               sizeof(struct Slot));
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

/**
 * @brief Inserts or updates a key-value pair using Robin Hood hashing.
 *
 * Algorithm:
 *  1. Hash the key; compute the home slot.
 *  2. Walk forward linearly, tracking our own displacement (@p dist).
 *  3. On finding an EMPTY slot, write the entry (if MAX_KEYS not exceeded).
 *  4. On finding a matching key, overwrite the value in-place.
 *  5. On finding an incumbent whose displacement is less than ours, evict it
 *     (Robin Hood swap) and continue inserting the displaced entry.
 *
 * @param key   Pointer to exactly 16 bytes of key data.
 * @param value Pointer to exactly 32 bytes of value data.
 * @return 0 on success, -2 if the table or key cap is exhausted, 1 if the key already exists and is updated.
 */
 int insert(const uint8_t *key, const uint8_t *value, const uint8_t *value_len) {
     /* Cache the lower 32 bits of the 64-bit hash alongside each entry so we
      * can quickly skip non-matching slots and compute probe distances without
      * re-hashing. */
     uint32_t hash = (uint32_t)(XXH3_64bits(key, 16) & 0xFFFFFFFF);
     size_t   home = hash & MASK;
     size_t   index  = home;
     size_t   dist = 0; /* displacement of the entry we are trying to place */

     /* Build the entry to insert in a local buffer. */
     struct Slot temp = {0};
     memcpy(temp.key,   key,   16);
     memcpy(temp.value, value, 32);
     temp.value_len = *value_len;
     temp.crc32 = crc32(0L, (const Bytef *)temp.key, 16);
     temp.crc32 = crc32(temp.crc32, (const Bytef *)temp.value, 32);
     temp.crc32 = crc32(temp.crc32, (const Bytef *)&temp.value_len, 1);
     temp.hash   = hash;
     temp.crc32 = crc32(temp.crc32, (const Bytef *)&temp.hash, 4);
     temp.status = OCCUPIED;
     temp.crc32 = crc32(temp.crc32, (const Bytef *)&temp.status, 1);

     while (1) {
         struct Slot *slot = &vegosh[index];

         /* Case 1: empty slot – write the entry here. */
         if (slot->status == EMPTY) {
             if (vegosh_count >= MAX_KEYS) {
                 return -2; /* hard key cap reached */
             }
             memcpy(slot, &temp, sizeof(struct Slot));
             slot->status = OCCUPIED;
             vegosh_count++;
             return 0;
         }

         /* Case 2: same key – update value without consuming a new slot. */
         if (slot->hash == temp.hash &&
             memcmp(slot->key, temp.key, 16) == 0) {
             slot->value_len = temp.value_len;

             slot->crc32 = temp.crc32;


             return 1;
         }



         /* Case 3: Robin Hood eviction.
          * If the incumbent is closer to its home than we are to ours,
          * steal its slot and continue placing the displaced entry. */
         size_t occ_home = slot->hash & MASK;
         size_t occ_dist = probe_distance(index, occ_home);

         if (occ_dist < dist) {
             /* Swap our entry into this slot; continue with the evicted one. */
             swap_entry_with_temp(index, &temp);
             dist = occ_dist; /* reset dist to the evicted entry's displacement */
         }

         /* Advance to the next slot (linear probing). */
         index = (index + 1) & MASK;
         dist++;

         /* Safety guard: wrapped all the way around – table is completely full. */
         if (dist >= TABLE_SIZE) {
             return -2;
         }
     }
 }

/**
 * @brief Looks up a key and copies its associated value into @p out_value.
 *
 * Because Robin Hood hashing bounds the maximum displacement of any entry,
 * we can exit early: if the slot we land on has been displaced *less* than
 * we have, our key cannot appear later in the probe chain (it would have
 * evicted this entry during insertion).
 *
 * @param key       Pointer to exactly 16 bytes of key data.
 * @param out_value Destination buffer of at least 32 bytes; written on hit.
 * @return 0 if found (value written), -1 if the key is not present.
 */
int get(const uint8_t *key, uint8_t *out_value, uint8_t *value_len) {
    uint32_t hash = (uint32_t)(XXH3_64bits(key, 16) & 0xFFFFFFFF);
    size_t   home = hash & MASK;
    size_t   index  = home;
    size_t   dist = 0;

    while (1) {
        struct Slot *slot = &vegosh[index];

        /* An empty slot means the key was never inserted. */
        if (slot->status == EMPTY) {
            return -1;
        }

        /* Hash comparison is a cheap pre-filter before the full memcmp. */
        if (slot->hash == hash &&
            memcmp(slot->key, key, 16) == 0) {
            memcpy(out_value, slot->value, 32);
            *value_len = slot->value_len;
            return 0;
        }

        /* Robin Hood early-exit: if this incumbent is closer to its home
         * than we are to ours, our key cannot be here or beyond. */
        size_t occ_home = slot->hash & MASK;
        size_t occ_dist = probe_distance(index, occ_home);

        if (occ_dist < dist) {
            return -1;
        }

        index = (index + 1) & MASK;
        dist++;

        /* Safety guard against a completely full table with no match. */
        if (dist >= TABLE_SIZE) {
            return -1;
        }
    }
}
