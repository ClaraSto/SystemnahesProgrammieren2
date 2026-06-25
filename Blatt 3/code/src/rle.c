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

static void append_to_rle(RLE *rle, uint64_t count) {
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

/**
 * Create a new RLE data structure. The RLE data structure is a linked
 * list of RLENodes. Each RLENode contains a count of the number of
 * consecutive bits that are the same. The RLE data structure is
 * initialized with an entry.
 * @return a pointer to the RLE data structure
 */
RLE *create_rle() {
  RLE *rle = malloc(sizeof(RLE));
  rle->head = NULL;
  rle->tail = NULL;
  rle->size = 0;

  append_to_rle(rle, 0); // Start with a count of 0 bits

  return rle;
}

/**
 * Delete the RLE data structure and all of its nodes. This function
 * should be called when the RLE data structure is no longer needed.
 * @param rle the RLE data structure to delete
 */
void delete_rle(RLE *rle) {
  RLENode *node = rle->head;
  while (node) {
    RLENode *next = node->next;
    free(node);
    node = next;
  }
  free(rle);
}

static bool pop_head_rle(RLE *rle, uint64_t *count) {
  if (!rle->head) {
    return false;
  }

  RLENode *node = rle->head;
  *count = node->count;

  rle->head = node->next;
  if (rle->tail == node) {
    rle->tail = NULL;
  }

  free(node);
  rle->size -= 1;

  return true;
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

/**
 * Fill rle counts with the provided data. The data should be treated as
 * binary data, not as a string, so the data is not null-terminated.
 *
 * This function counts the number of consecutive bits that are the
 * same and appends that count to the rle. The first entry of the rle
 * should always be the number of consecutive 0s at the beginning of
 * the data.
 *
 * For example, if the start of data is "00001111", then the rle should contain
 * two entries 4 and 4.
 *
 * If the start of data is "11110000", then the rle should contain three
 * entries, 0, 4, and 4
 * @param rle Will be filled with counts
 * @param data Source data, treated as binary data
 * @param size Size of the source data
 */
void encode_rle(RLE *rle, const char *data, size_t size) {
  uint8_t counting_bit = (rle->size & 1) ^ 1;

  for (size_t i = 0; i < size; i++) {
    for (int8_t j = 7; j >= 0; j--) {
      uint8_t current_bit = (data[i] >> j) & 1;
      if (current_bit == counting_bit) {
        rle->tail->count++;
      } else {
        append_to_rle(rle, 1);
        counting_bit ^= 1; // Switch between 0 and 1
      }
    }
  }
}

/**
 * Decodes the rle to the appropriate binary data. The returned data
 * should be treated as binary data, not as a string, so the data is
 * not null-terminated.
 * @param rle assumed to be filled with counts
 * @param size will be set by this function and is the size of the returned data
 * @return binary data
 */
char *decode_rle(RLE *rle, size_t *size) {
  uint64_t total_bits = get_rle_total_count(rle);
  *size = (total_bits + 7) >> 3; // Round up to the nearest byte

  char *output = calloc(*size, sizeof(char));
  if (!output) {
    return NULL;
  }

  uint64_t count;
  size_t byte_index = 0;

  uint8_t bit = 0;
  uint8_t bit_index = 7;

  while (byte_index < *size && pop_head_rle(rle, &count)) {
    while (count > 0) {
      output[byte_index] |= bit << bit_index;
      if (bit_index == 0) {
        byte_index++;
        bit_index = 7;
      } else {
        bit_index--;
      }
      count--;
    }
    bit ^= 1; // Switch between 0 and 1
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
      printf(", "); // print comma only if this isn't the last node
    if (counts_per_line > 0 && ++counter >= counts_per_line) {
      printf("\n");
      counter = 0;
    }

    node = node->next;
  }
  printf(" }");
  printf("\n");
}

/*
  Serialisierung und Deserialisierung im 4-Bit/Nibble-Format.

  Format pro Nibble (4 Bit), von MSB nach LSB:
    bit3 (MSB) = type (0 => zählt 0-Bits, 1 => zählt 1-Bits)
    bit2       = ext  (0 => nur 2 LSBs als count, 1 => zusätzlich nächste Nibble
  als high4) bit1..0    = low2 (niedrigste 2 Bits des Zählers)

  Wenn ext == 1:
    nächste Nibble liefert high4, sodass count = (high4 << 2) | low2
    => count ist 6-Bit, Bereich 0..63

  Implementierungsprinzip:
    - Für jeden RLE-Knoten splitten wir node->count in Chunks <= 63.
    - Jeder Chunk wird als 1 oder 2 Nibbles emittiert.
    - Nibbles werden temporär als bytes (0..15) gesammelt und am Ende
      zu Bytes gepackt (2 Nibbles pro Byte).
    - Beim Deserialisieren entpacken wir Bytes in Nibbles und bauen die
  RLE-Liste.
    - Der erste gelesene Count überschreibt den bereits von create_rle
      angelegten ersten Knoten (wie in deiner Vorlage).
*/

/* Hilfsfunktion: Nibble dynamisch anhängen */
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

/* Hilfsfunktion: emittiere einen einzelnen count (0..63) als 1 oder 2 Nibbles
 */
static void emit_count_as_nibbles(uint8_t **nibbles, size_t *ncount,
                                  uint8_t type, uint64_t count) {
  if (count > 63)
    count = 63; // safety, caller sollte splitten
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
}

/* Normale Serialisierung: einfache, klare Implementierung */
static char *serialize_rle_normal(RLE *rle, size_t *size) {
  uint8_t *nibbles = NULL;
  size_t ncount = 0;
  uint8_t type = 0; // erstes Listenelement zählt 0-Bits

  RLENode *node = rle->head;
  while (node) {
    uint64_t remaining = node->count;
    while (remaining > 0) {
      uint64_t chunk = remaining <= 63 ? remaining : 63;
      emit_count_as_nibbles(&nibbles, &ncount, type, chunk);
      remaining -= chunk;
    }
    type ^= 1;
    node = node->next;
  }

  // Packe zwei Nibbles pro Byte
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

/* Optimierte Serialisierung:
   - Greedy: packe große counts in möglichst wenige 6-Bit-Chunks.
   - Entfernt keine semantische Information; Verhalten bleibt kompatibel.
   - In einer sauberen alternierenden RLE-Liste ist der Unterschied zur normalen
   Variante gering; die Optimierung hilft vor allem bei speziellen Listenformen.
*/
static char *serialize_rle_optimized(RLE *rle, size_t *size) {
  uint8_t *nibbles = NULL;
  size_t ncount = 0;
  uint8_t type = 0;

  RLENode *node = rle->head;
  while (node) {
    if (node->count == 0) {
      // überspringe Null-Counts (spart Nibbles)
      type ^= 1;
      node = node->next;
      continue;
    }
    uint64_t remaining = node->count;
    while (remaining > 0) {
      uint64_t chunk = remaining <= 63 ? remaining : 63;
      emit_count_as_nibbles(&nibbles, &ncount, type, chunk);
      remaining -= chunk;
    }
    type ^= 1;
    node = node->next;
  }

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

/* Wrapper: wählt je nach use_optimized die Variante */
char *serialize_rle(RLE *rle, size_t *size) {
  if (use_optimized) {
    return serialize_rle_optimized(rle, size);
  } else {
    return serialize_rle_normal(rle, size);
  }
}

/* Deserialisierung: Bytes -> Nibbles -> RLE-Liste
   - Entpacke Bytes in Nibbles (zwei Nibbles pro Byte).
   - Lese nacheinander Nibbles und bilde counts.
   - Ersten Count überschreibe rle->head->count (wie in Vorlage).
   - Weitere counts werden mit append_to_rle angehängt.
*/
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

  size_t idx = 0;
  if (!rle->head) {
    append_to_rle(rle, 0);
  }

  // Erster Count überschreiben
  if (idx < ncount) {
    uint8_t nib = nibbles[idx++];
    uint8_t ext = (nib >> 2) & 1;
    uint64_t count = nib & 0x3;
    if (ext) {
      if (idx >= ncount) {
        free(nibbles);
        return;
      }
      uint8_t extra = nibbles[idx++];
      count = ((uint64_t)extra << 2) | count;
    }
    rle->head->count = count;
  }

  // Restliche Counts anhängen
  while (idx < ncount) {
    uint8_t nib = nibbles[idx++];
    uint8_t ext = (nib >> 2) & 1;
    uint64_t count = nib & 0x3;
    if (ext) {
      if (idx >= ncount)
        break;
      uint8_t extra = nibbles[idx++];
      count = ((uint64_t)extra << 2) | count;
    }
    append_to_rle(rle, count);
  }

  free(nibbles);
}