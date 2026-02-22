#ifndef KV_H
#define KV_H

#include "xxhash.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE (1 << 21)  // 2,097,152 for example
#define MASK (TABLE_SIZE - 1)
#define MAX_KEYS 1000000
#define EMPTY 0
#define OCCUPIED 1



struct Slot {
  uint8_t key[16];
  uint8_t value[32];
  uint32_t hash;
  uint8_t status;
  uint8_t value_type;
  uint8_t reserved[10];

};

int initializeKV();
int insert(const uint8_t *key, const uint8_t *value, uint8_t value_type);
int get(const uint8_t *key,uint8_t *out_value,uint8_t *out_value_type);
#endif /* KV_H */
