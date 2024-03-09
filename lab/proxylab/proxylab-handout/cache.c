/*
 * cache.c
 *
 * 刘昕垚 2200012866 
 * 
 * cache main code
 * In this part, I check the cache and update it.
 * The eviction policy is LRU.
 *
 */

#include "csapp.h"
#include "cache.h"

typedef struct {
    char object[MAX_OBJECT_SIZE];
    char url[MAXLINE];
    int LRU;
    int empty;

    /* mutex, so that needn't use cnt */
    pthread_mutex_t read_mutex; 
    pthread_mutex_t mutex;
} Cache;

Cache cache[CACHE_SIZE];

/*
 * cache_init - Initialize the cache
 */
void cache_init(int index) {
    cache[index].LRU = 0;
    cache[index].empty = 1;

    pthread_mutex_init(&cache[index].read_mutex, NULL);
    pthread_mutex_init(&cache[index].mutex, NULL);
}

/*
 * cache_find - Find the cache if it exists
 *
 *              return the index if found
 *              return -1 if failed
 */
int cache_find(int fd, char *url) {
    int flag = -1;
    for(int i = 0; i < CACHE_SIZE; ++ i) {
        pthread_mutex_lock(&cache[i].read_mutex);
        if((!cache[i].empty) && strcmp(cache[i].url, url) == 0) {
            Rio_writen(fd, cache[i].object, MAX_OBJECT_SIZE);
            pthread_mutex_unlock(&cache[i].read_mutex);
            flag = i;
            break;
        }
        pthread_mutex_unlock(&cache[i].read_mutex);
    }
    return flag;
}

/*
 * cache_remove - Find the block to remove
 *                return the index it find
 */
int cache_remove() {
    int max_LRU = -1;
    int flag = 0;
    for(int i = 0; i < CACHE_SIZE; ++ i) {
        pthread_mutex_lock(&cache[i].read_mutex);

        if(cache[i].empty) {
            flag = i;
            pthread_mutex_unlock(&cache[i].read_mutex);
            break;
        }
        if(cache[i].LRU > max_LRU) {
            flag = i;
            max_LRU = cache[i].LRU;
        }
        
        pthread_mutex_unlock(&cache[i].read_mutex);
    }
    return flag;
}

/*  
 * cache_update - Update every cacheblock and update LRU
 */
void cache_update(char *url, char *buf) {
    int index = cache_remove();

    pthread_mutex_lock(&cache[index].mutex);

    strcpy(cache[index].url, url);
    memcpy(cache[index].object, buf, MAX_OBJECT_SIZE);

    cache[index].empty = 0;
    cache[index].LRU = 0;

    pthread_mutex_unlock(&cache[index].mutex);

    cache_LRU(index);
}

/*  
 * cache_LRU - Update every cacheblock's LRU 
 */
void cache_LRU(int index) {
    for(int i = 0; i < CACHE_SIZE; ++ i) {

        pthread_mutex_lock(&cache[i].mutex);

        if(i == index)
            cache[i].LRU = 0;
        else if(!(cache[i].empty))
            ++ cache[i].LRU;

        pthread_mutex_unlock(&cache[i].mutex);
    }
}