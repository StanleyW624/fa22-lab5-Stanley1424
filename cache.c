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
static int cache_amount = 0;

int cache_create(int num_entries) {
  if (cache != NULL) {                                    //check if cache exist
    return -1;
  }else if (num_entries < 2) {                            //check lower bound
    return -1;
  }else if (num_entries > 4096) {                         //check upper bound
    return -1;
  } else {
    cache = malloc(num_entries*sizeof(cache_entry_t));    //memory allocation
    for (int i = 0; i < cache_size; i++) {                //loop to set all default valid to 0
      cache[i].valid = 0;
    }
    cache_size = num_entries;                             //set cache size
    num_queries = 0;                                      //reset queries
    num_hits = 0;                                         //reset hits
    cache_amount = 0;                                     //reset tracker for items in cache
    return 1;
  }
}

int cache_destroy(void) {
  if (cache == NULL) {                                    //check if cache exist
    return -1;
  } else {
    cache = NULL;                                         //remove cache
    free(cache);                                          //free memory for cache
    cache_size = 0;                                       //reset cache size
    cache_amount = 0;                                     //reset tracker for items in cache
    return 1;
  }
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if (cache == NULL) {                                   //check if cache exist
    return -1;
  } else if (buf == NULL ) {                             //check if buf exist
    return -1;
  }

  if (cache_amount == 0) {                               //check if there is any item in cache
    return -1;
  }

  if (disk_num > JBOD_NUM_DISKS) {                       //set upper bound for disk
    return -1;
  } else if (disk_num < 0) {                             //set lower bound for disk
    return -1;
  }

  if (block_num > JBOD_DISK_SIZE) {                      //set upper bound for block
    return -1;
  } else if (block_num < 0) {                            //set lower bound for block
    return -1;
  }

  for (int i = 0; i < cache_size; i++) {                 //loop to check for if selected disk and block exists
    if (cache[i].disk_num == disk_num) {            
      if (cache[i].block_num == block_num) {
	memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);    //copy memory if exists
	num_hits++;                                      //increment hits if exists
	num_queries++;                                   //increment queries
	cache[i].num_accesses++;                         //increment times accessed
	return 1;
      }
    }
  }
  num_queries++;                                         //increment queries
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  for (int i = 0; i < cache_size; i++) {                 //locate selected disk and block
    if (cache[i].disk_num == disk_num) {
      if (cache[i].block_num == block_num) {
	memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);    //update the block with input buf
	cache[i].num_accesses++;                         //increment times accessed
      }
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  if (cache == NULL) {                                   //check if cache exist
    return -1;
  } else if (buf == NULL ) {                             //check if buf exist
    return -1;
  }

  if (disk_num > JBOD_NUM_DISKS) {                       //upper bound for disk
    return -1;
  } else if (disk_num < 0) {                             //lower bound for disk
    return -1;
  }
  
  if (block_num > JBOD_DISK_SIZE) {                      //upper bound for block
    return -1;
  } else if (block_num < 0) {                            //lower bound for block
    return -1;
  }

  for (int i = 0; i < cache_size; i++) {                 //loop to check if selected disk and block exists
    if (cache[i].disk_num == disk_num) {
      if (cache[i].block_num == block_num) {
	if(cache[i].valid == 1) {
	  return -1;                                     //return -1 if it exists
	}
      }
    }
  }

  if (cache_amount == cache_size) {                      //check if cache is full
    int least_amount_used_position = 0;                  //this section is to find the position of least used cache
    int current_least_value = cache[0].num_accesses;
    for(int i = 0; i < cache_size; i++) {
      if (cache[i].num_accesses < current_least_value) {
	least_amount_used_position = i;
	current_least_value = cache[i].num_accesses;
      }
    }
    cache[least_amount_used_position].valid = 1;               //this section is to replace the least used cache
    cache[least_amount_used_position].disk_num = disk_num;
    cache[least_amount_used_position].block_num = block_num;
    memcpy(cache[least_amount_used_position].block, buf, JBOD_BLOCK_SIZE);
    cache[least_amount_used_position].num_accesses = 1;
  } else {                                               //if cache is not full
    int avaliable_position = 0;                          //this section is to find an open spot
    for(int i = 0; i < cache_size; i++) {                
      if(cache[i].valid != 1) {
	avaliable_position = i;
	break;
      }
    }
    cache[avaliable_position].valid = 1;                       //this section is to insert data into the open spot
    cache[avaliable_position].disk_num = disk_num;
    cache[avaliable_position].block_num = block_num;
    memcpy(cache[avaliable_position].block, buf, JBOD_BLOCK_SIZE);
    cache[avaliable_position].num_accesses = 1;
    cache_amount++;                                            //increment tracking of item amount in cache
  }
  return 1;
}

bool cache_enabled(void) {
	return cache != NULL && cache_size > 0;
}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
	fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
