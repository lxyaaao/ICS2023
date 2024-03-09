/*
 * cache.h
 *
 * 刘昕垚 2200012866 
 * 
 * cache header
 *
 */

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Cache size */
#define CACHE_SIZE   10

/*
 * cache_init - Initialize the cache
 */
void cache_init(int index);

/*
 * cache_find - Find the cache if it exists
 *
 *              return the index if found
 *              return -1 if failed
 */
int cache_find(int fd, char *url);

/*
 * cache_remove - Find the block to remove
 *                return the index it find
 */
int cache_remove();

/*  
 * cache_update - Update every cacheblock and update LRU
 */
void cache_update(char *url, char *buf);

/*  
 * cache_LRU - Update every cacheblock's LRU 
 */
void cache_LRU(int index);