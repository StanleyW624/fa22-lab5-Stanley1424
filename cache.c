#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
    return -1;
}

int cache_destroy(void) {
    return -1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  return 0;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  return 1;
}

bool cache_enabled(void) {
	return cache != NULL && cache_size > 0;
}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
	fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
