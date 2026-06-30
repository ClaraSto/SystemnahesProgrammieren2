#include "../include/rle.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool use_optimized;

typedef struct RLENode {
  uint64_t count;
  struct RLENode *next;
} RLENode;

struct RLE {
  RLENode *head;
  RLENode *tail;
  uint64_t size;
};

void append_to_rle(RLE *rle, uint64_t count) {
  RLENode *node = malloc(sizeof(RLENode));
  node->count = count;
  node->next = NULL;

  if (rle->tail) {
    rle->tail->next = node;
  } else {
    rle->head = node;
  }

  rle->tail = node;
  rle->size += 1;
}

RLE *create_rle() {
  RLE *rle = malloc(sizeof(RLE));
  rle->head = NULL;
  rle->tail = NULL;
  rle->size = 0;

  append_to_rle(rle, 0);
  return rle;
}

void delete_rle(RLE *rle) {
  RLENode *node = rle->head;
  while (node) {
    RLENode *next = node->next;
    free(node);
    node = next;
  }
  free(rle);
}

static uint64_t get_rle_total_count(RLE *rle) {
  uint64_t total = 0;
  RLENode *node = rle->head;
  while (node) {
    total += node->count;
    node = node->next;
  }
  return total;
}

void encode_rle(RLE *rle, const char *data, size_t size) {
  uint8_t counting_bit = (rle->size & 1) ^ 1;

  for (size_t i = 0; i < size; i++) {
    for (int8_t j = 7; j >= 0; j--) {
      uint8_t current_bit = (data[i] >> j) & 1;
      if (current_bit == counting_bit) {
        rle->tail->count++;
      } else {
        append_to_rle(rle, 1);
        counting_bit ^= 1;
      }
    }
  }
}

/*
  Robust decode: iterate bit positions explicitly.
  This avoids subtle index/bit_index off-by-one errors and is easy to reason
  about.
*/
char *decode_rle(RLE *rle, size_t *size) {
  uint64_t total_bits = get_rle_total_count(rle);
  *size = (total_bits + 7) >> 3;

  if (*size == 0) {
    return calloc(1, 1); // return at least a non-NULL pointer for zero-length
  }

  char *output = calloc(*size, sizeof(char));
  if (!output) {
    return NULL;
  }

  uint64_t bit_pos = 0;  // 0 .. total_bits-1
  uint8_t bit_value = 0; // start with zeros (matches create_rle behavior)
  RLENode *node = rle->head;

  while (node) {
    uint64_t cnt = node->count;
    for (uint64_t i = 0; i < cnt; ++i) {
      if (bit_pos >= total_bits)
        break;
      if (bit_value) {
        uint64_t byte_index = bit_pos >> 3;      // bit_pos / 8
        uint8_t bit_index = 7 - (bit_pos & 0x7); // MSB first
        output[byte_index] |= (1u << bit_index);
      }
      bit_pos++;
    }
    bit_value ^= 1;
    node = node->next;
  }

  return output;
}

void print_rle(RLE *rle, uint8_t counts_per_line) {
  RLENode *node = rle->head;
  printf("{\n");
  int counter = 0;
  while (node) {
    printf("  %lu", node->count);
    if (node->next)
      printf(", ");
    if (counts_per_line > 0 && ++counter >= counts_per_line) {
      printf("\n");
      counter = 0;
    }
    node = node->next;
  }
  printf(" }\n");
}

static void push_nibble(uint8_t **nibbles, size_t *ncount, uint8_t nib) {
  uint8_t *tmp = realloc(*nibbles, (*ncount + 1) * sizeof(uint8_t));
  if (!tmp) {
    free(*nibbles);
    *nibbles = NULL;
    *ncount = 0;
    return;
  }
  *nibbles = tmp;
  (*nibbles)[(*ncount)++] = nib & 0xF;
}

static void emit_count_as_nibbles(uint8_t **nibbles, size_t *ncount,
                                  uint8_t type, uint64_t count,
                                  bool optimized) {
  if (!optimized) {
    if (count <= 3) {
      uint8_t nib =
          (uint8_t)((type & 1) << 3) | (0 << 2) | (uint8_t)(count & 0x3);
      push_nibble(nibbles, ncount, nib);
    } else {
      uint8_t low2 = count & 0x3;
      uint8_t high4 = (count >> 2) & 0xF;
      uint8_t nib1 = (uint8_t)((type & 1) << 3) | (1 << 2) | low2;
      uint8_t nib2 = high4 & 0xF;
      push_nibble(nibbles, ncount, nib1);
      push_nibble(nibbles, ncount, nib2);
    }
  } else {
    // Optimized token emission according to the rules:
    // 00xx -> value = low2
    // 01xx + extra -> value = (extra<<2)|low2 + 4
    // 10xx -> value = low2 + 1
    // 11xx + extra -> value = (extra<<2)|low2 + 5
    if (count <= 3) {
      // emit 00xx or 10xx depending on type: for type==0 use 00xx, for type==1
      // use 10xx
      uint8_t nib =
          (uint8_t)((type & 1) << 3) | (0 << 2) | (uint8_t)(count & 0x3);
      push_nibble(nibbles, ncount, nib);
    } else {
      // emit ext=1 token (6-bit) and rely on decode to add +4/+5
      uint8_t low2 = count & 0x3;
      uint8_t high4 = (count >> 2) & 0xF;
      uint8_t nib1 = (uint8_t)((type & 1) << 3) | (1 << 2) | low2;
      uint8_t nib2 = high4 & 0xF;
      push_nibble(nibbles, ncount, nib1);
      push_nibble(nibbles, ncount, nib2);
    }
  }
}

static char *pack_nibbles(uint8_t *nibbles, size_t ncount, size_t *size) {
  size_t bytes = (ncount + 1) / 2;
  char *out = malloc(bytes);
  if (!out) {
    free(nibbles);
    *size = 0;
    return NULL;
  }
  memset(out, 0, bytes);
  for (size_t i = 0; i < ncount; i += 2) {
    uint8_t high = nibbles[i] & 0xF;
    uint8_t low = (i + 1 < ncount) ? (nibbles[i + 1] & 0xF) : 0;
    out[i / 2] = (char)((high << 4) | low);
  }
  *size = bytes;
  free(nibbles);
  return out;
}

// ===== NORMALE SERIALISIERUNG =====
static char *serialize_rle_normal(RLE *rle, size_t *size) {
  uint8_t *nibbles = NULL;
  size_t ncount = 0;
  uint8_t type = 0;

  RLENode *node = rle->head;
  while (node) {
    uint64_t remaining = node->count;
    while (remaining > 0) {
      uint64_t chunk = remaining <= 63 ? remaining : 63;
      emit_count_as_nibbles(&nibbles, &ncount, type, chunk, false);
      remaining -= chunk;
    }
    type ^= 1;
    node = node->next;
  }

  return pack_nibbles(nibbles, ncount, size);
}

// ===== OPTIMIERTE SERIALISIERUNG =====
static char *serialize_rle_optimized(RLE *rle, size_t *size) {
  uint8_t *nibbles = NULL;
  size_t ncount = 0;
  uint8_t type = 0;

  RLENode *node = rle->head;
  while (node) {
    uint64_t remaining = node->count;
    while (remaining > 0) {
      uint64_t chunk = remaining <= 63 ? remaining : 63;
      emit_count_as_nibbles(&nibbles, &ncount, type, chunk, true);
      remaining -= chunk;
    }
    type ^= 1;
    node = node->next;
  }

  return pack_nibbles(nibbles, ncount, size);
}

// ===== WRAPPER =====
char *serialize_rle(RLE *rle, size_t *size) {
  if (use_optimized) {
    return serialize_rle_optimized(rle, size);
  } else {
    return serialize_rle_normal(rle, size);
  }
}

// ===== DESERIALISIERUNG =====
void deserialize_rle(RLE *rle, const char *data, size_t size) {
  if (size == 0)
    return;

  size_t ncount = size * 2;
  uint8_t *nibbles = malloc(ncount);
  if (!nibbles)
    return;

  for (size_t i = 0; i < size; ++i) {
    uint8_t b = (uint8_t)data[i];
    nibbles[i * 2] = (b >> 4) & 0xF;
    nibbles[i * 2 + 1] = b & 0xF;
  }

  // Clear the existing RLE structure
  RLENode *node = rle->head;
  while (node) {
    RLENode *next = node->next;
    free(node);
    node = next;
  }
  rle->head = NULL;
  rle->tail = NULL;
  rle->size = 0;

  size_t idx = 0;
  uint8_t current_type = 0; // start with zeros

  while (idx < ncount) {
    // If last nibble is padding zero, break
    if (idx == ncount - 1 && nibbles[idx] == 0) {
      break;
    }

    uint8_t nib = nibbles[idx++];
    uint8_t ext = (nib >> 2) & 1;
    uint64_t count = nib & 0x3;

    if (ext) {
      if (idx >= ncount)
        break;
      uint8_t extra = nibbles[idx++];
      uint64_t full = ((uint64_t)extra << 2) | count;
      if (!use_optimized) {
        // normal: full is the 6-bit count
        count = full;
      } else {
        // optimized: token semantics:
        // token_type bit is the top bit of nibble
        uint8_t token_type = (nib >> 3) & 1;
        if (token_type == 0) {
          // 01xx extra -> +4
          count = full + 4;
        } else {
          // 11xx extra -> +5
          count = full + 5;
        }
      }
    } else {
      // ext == 0: 2-bit value
      if (!use_optimized) {
        // normal: count is low2 (already)
      } else {
        uint8_t token_type = (nib >> 3) & 1;
        if (token_type == 0) {
          // 00xx -> value = low2
          count = count;
        } else {
          // 10xx -> value = low2 + 1
          count = count + 1;
        }
      }
    }

    append_to_rle(rle, count);
    current_type ^= 1;
  }

  free(nibbles);
}
