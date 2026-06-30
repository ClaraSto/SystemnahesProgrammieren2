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
  if (!node)
    return;
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
  if (!rle)
    return NULL;
  rle->head = NULL;
  rle->tail = NULL;
  rle->size = 0;

  append_to_rle(rle, 0);
  return rle;
}

void delete_rle(RLE *rle) {
  if (!rle)
    return;
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
  if (!rle || !data)
    return;
  uint8_t counting_bit = 0;

  for (size_t i = 0; i < size; i++) {
    for (int8_t j = 7; j >= 0; j--) {
      uint8_t current_bit = ((uint8_t)data[i] >> j) & 1;
      if (current_bit == counting_bit) {
        rle->tail->count++;
      } else {
        append_to_rle(rle, 1);
        counting_bit ^= 1;
      }
    }
  }
}

char *decode_rle(RLE *rle, size_t *size) {
  if (!rle || !size)
    return NULL;

  uint64_t total_bits = get_rle_total_count(rle);
  *size = (total_bits + 7) >> 3;

  if (*size == 0) {
    return calloc(1, 1);
  }

  char *output = calloc(*size, sizeof(char));
  if (!output) {
    return NULL;
  }

  uint64_t bit_pos = 0;
  uint8_t bit_value = 0;
  RLENode *node = rle->head;

  while (node) {
    uint64_t cnt = node->count;
    for (uint64_t i = 0; i < cnt; ++i) {
      if (bit_pos >= total_bits)
        break;

      if (bit_value) {
        uint64_t byte_index = bit_pos >> 3;
        uint8_t bit_index = 7 - (bit_pos & 0x7);
        output[byte_index] |= (char)(1u << bit_index);
      }
      bit_pos++;
    }
    bit_value ^= 1;
    node = node->next;
  }

  return output;
}

static void write_nibble(char **buf, size_t *nibble_cnt, uint8_t nibble_val) {
  size_t byte_idx = *nibble_cnt / 2;
  if (*nibble_cnt % 2 == 0) {
    (*buf)[byte_idx] = (char)((nibble_val & 0x0F) << 4);
  } else {
    (*buf)[byte_idx] |= (char)(nibble_val & 0x0F);
  }
  (*nibble_cnt)++;
}

static uint8_t read_nibble(const char *buf, size_t nibble_idx) {
  size_t byte_idx = nibble_idx / 2;
  if (nibble_idx % 2 == 0) {
    return (uint8_t)((buf[byte_idx] >> 4) & 0x0F);
  } else {
    return (uint8_t)(buf[byte_idx] & 0x0F);
  }
}

char *serialize_rle(RLE *rle, size_t *size) {
  if (!rle || !size)
    return NULL;

  size_t max_bytes = (rle->size * 2) + 1;
  char *buf = calloc(max_bytes, sizeof(char));
  if (!buf)
    return NULL;

  size_t nibble_cnt = 0;
  RLENode *node = rle->head;
  uint8_t bit_type = 0;

  while (node) {
    uint64_t count = node->count;

    if (count == 0) {
      bit_type ^= 1;
      node = node->next;
      continue;
    }

    while (count > 0) {
      uint64_t chunk = 0;
      uint8_t ext = 0;
      uint8_t val_high = 0;
      uint8_t val_low = 0;

      if (!use_optimized) {
        if (count <= 3) {
          chunk = count;
          ext = 0;
          val_high = (uint8_t)chunk;
        } else {
          chunk = (count > 63) ? 63 : count;
          ext = 1;
          val_high = (uint8_t)(chunk >> 4);
          val_low = (uint8_t)(chunk & 0x0F);
        }
      } else {
        if (bit_type == 0) {
          if (count <= 3) {
            chunk = count;
            ext = 0;
            val_high = (uint8_t)chunk;
          } else {
            chunk = (count > 67) ? 67 : count;
            ext = 1;
            uint8_t actual_val = (uint8_t)(chunk - 4);
            val_high = (uint8_t)(actual_val >> 4);
            val_low = (uint8_t)(actual_val & 0x0F);
          }
        } else {
          if (count <= 4) {
            chunk = count;
            ext = 0;
            val_high = (uint8_t)(chunk - 1);
          } else {
            chunk = (count > 68) ? 68 : count;
            ext = 1;
            uint8_t actual_val = (uint8_t)(chunk - 5);
            val_high = (uint8_t)(actual_val >> 4);
            val_low = (uint8_t)(actual_val & 0x0F);
          }
        }
      }

      uint8_t first_nibble =
          (uint8_t)((bit_type << 3) | (ext << 2) | (val_high & 0x03));
      write_nibble(&buf, &nibble_cnt, first_nibble);

      if (ext) {
        write_nibble(&buf, &nibble_cnt, val_low);
      }

      count -= chunk;
    }

    bit_type ^= 1;
    node = node->next;
  }

  // Korrigiertes Padding: Setzt ungenutzte Bits am Byte-Ende auf 1 (0x0F)
  if (nibble_cnt > 0 && (nibble_cnt % 2 != 0)) {
    size_t last_byte_idx = (nibble_cnt - 1) / 2;
    buf[last_byte_idx] |= 0x0F;
  }

  *size = (nibble_cnt + 1) / 2;
  return buf;
}

void deserialize_rle(RLE *rle, const char *data, size_t size) {
  if (!rle || !data || size == 0)
    return;

  size_t total_nibbles = size * 2;
  size_t nibble_idx = 0;
  uint8_t current_expected_type = 0;

  while (nibble_idx < total_nibbles) {
    // Erkennt Padding am Pufferende (0x00 oder 0x0F im ungenutzten hinteren
    // Nibble)
    if (nibble_idx == total_nibbles - 1) {
      uint8_t last_nibble = read_nibble(data, nibble_idx);
      if (last_nibble == 0 || last_nibble == 0x0F) {
        break;
      }
    }

    uint8_t first_nibble = read_nibble(data, nibble_idx++);
    uint8_t bit_type = (first_nibble >> 3) & 1;
    uint8_t ext = (first_nibble >> 2) & 1;
    uint8_t val_high = first_nibble & 0x03;

    uint64_t count = 0;

    if (ext) {
      if (nibble_idx >= total_nibbles)
        break;
      uint8_t second_nibble = read_nibble(data, nibble_idx++);
      uint8_t combined_val = (uint8_t)((val_high << 4) | second_nibble);

      if (!use_optimized) {
        count = combined_val;
      } else {
        count = combined_val + (bit_type == 0 ? 4 : 5);
      }
    } else {
      if (!use_optimized) {
        count = val_high;
      } else {
        count = val_high + (bit_type == 0 ? 0 : 1);
      }
    }

    if (count == 0)
      continue;

    while (current_expected_type != bit_type) {
      append_to_rle(rle, 0);
      current_expected_type ^= 1;
    }

    rle->tail->count += count;
  }
}

void print_rle(RLE *rle, uint8_t counts_per_line) {
  if (!rle)
    return;
  RLENode *node = rle->head;
  int index = 0;
  while (node) {
    if (counts_per_line) {
      printf("  Node %d (%s-Bits): %lu\n", index, (index % 2 == 0) ? "0" : "1",
             (unsigned long)node->count);
    }
    node = node->next;
    index++;
  }
}
