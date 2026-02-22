#include "kv.h"
#include <stddef.h>
#include <stdint.h>

struct Slot *vegosh;  // global
static size_t vegosh_count = 0;


int initializeKV() {
    _Static_assert(sizeof(struct Slot) == 64, "Slot must be 64 bytes"); // Slot must be 64 bytes to ensure alignment
    _Static_assert(sizeof(struct Slot) % 64 == 0, "Slot size must be aligned"); // Slot size must be aligned to 64 bytes

    size_t total_size = sizeof(struct Slot) * TABLE_SIZE;
    vegosh = aligned_alloc(64, total_size);

    if (!vegosh) {
        perror("aligned_alloc failed");
        return -1;
    }

    memset(vegosh, 0, total_size);
    printf("Allocated %zu bytes at %p\n", total_size, (void *)vegosh);
    return 0;
}
static inline size_t probe_distance(size_t idx, size_t home) {
    return (idx + TABLE_SIZE - home) & MASK;
}

static inline void swap_entry_with_temp(size_t idx, struct Slot *temp) {
    struct Slot old;

    memcpy(&old, &vegosh[idx], sizeof(struct Slot));
    memcpy(&vegosh[idx], temp, sizeof(struct Slot));
    vegosh[idx].status = OCCUPIED;
    memcpy(temp, &old, sizeof(struct Slot));
}


int insert(const uint8_t *key, const uint8_t *value, uint8_t value_type)
{
    uint32_t hash = (uint32_t)(XXH3_64bits(key, 16) & 0xFFFFFFFF);
    size_t home = hash & MASK;
    size_t idx = home;
    size_t dist = 0;

    struct Slot temp = {0};

    memcpy(temp.key, key, 16);
    memcpy(temp.value, value, 32);
    temp.hash = hash;
    temp.status = OCCUPIED;
    temp.value_type = value_type;

    while (1) {
        struct Slot *slot = &vegosh[idx];

        // EMPTY → insert (respect hard cap)
        if (slot->status == EMPTY) {
            if (vegosh_count >= MAX_KEYS) {
                return -1; // hard limit reached
            }

            memcpy(slot, &temp, sizeof(struct Slot));
            slot->status = OCCUPIED;
            vegosh_count++;
            return 0;
        }

        // key match → update (does NOT count toward cap)
        if (slot->hash == temp.hash &&
            memcmp(slot->key, temp.key, 16) == 0) {
            memcpy(slot->value, temp.value, 32);
            slot->value_type = temp.value_type;
            return 0;
        }

        // Robin Hood logic
        size_t occ_home = slot->hash & MASK;
        size_t occ_dist = (idx + TABLE_SIZE - occ_home) & MASK;

        if (occ_dist < dist) {
            swap_entry_with_temp(idx, &temp);
            dist = occ_dist;
        }

        idx = (idx + 1) & MASK;
        dist++;

        if (dist >= TABLE_SIZE) {
            return -1; // table full
        }
    }
}

int get(const uint8_t *key,uint8_t *out_value,uint8_t *out_value_type)
{
    uint32_t hash = (uint32_t)(XXH3_64bits(key, 16) & 0xFFFFFFFF);
    size_t home = hash & MASK;
    size_t idx = home;
    size_t dist = 0;

    while (1) {
        struct Slot *slot = &vegosh[idx];

        // empty slot → definitely not present
        if (slot->status == EMPTY) {
            return -1;
        }

        // check match
        if (slot->hash == hash &&
            memcmp(slot->key, key, 16) == 0) {
            memcpy(out_value, slot->value, 32);
            if (out_value_type) {
                *out_value_type = slot->value_type;
            }
            return 0;
        }

        // 🔥 Robin Hood early-exit rule
        size_t occ_home = slot->hash & MASK;
        size_t occ_dist = (idx + TABLE_SIZE - occ_home) & MASK;

        if (occ_dist < dist) {
            // if we got farther than this guy ever probed,
            // our key cannot be in the table
            return -1;
        }

        idx = (idx + 1) & MASK;
        dist++;

        if (dist >= TABLE_SIZE) {
            return -1;
        }
    }
}
