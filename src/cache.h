/**
 * @file cache.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>

typedef struct cache_entry_storage cache_entry_storage_t;

typedef struct cache_entry {
    char *key;
    char *path;
} cache_entry_t;

typedef struct cache cache_t;

/**
 * @brief Initialize the cache
 *
 * @param path Path to cache directory
 * @param ttl Cache TTL in seconds
 * @param verbose Verbose mode
 */

cache_t *cache_init(char *path, int ttl, int verbose);

/**
 * @brief Free the cache
 *
 */
void cache_free(cache_t *cache);

/**
 * @brief Get a cache entry, call the resolver function to get the item from the
 * cache if not found or invalidated by ttl. This is thread safe.
 *
 * @param cache Cache to get from
 * @param key Key to get
 * @param data_out Buffer to store the data in
 * @param size_out Length of the data buffer
 * @param cache_miss_resolver Function to call if the cache entry is not found
 * (char *filename to save cache data, void *arg provided in call)
 * @param arg Argument to pass to the cache miss resolver
 * @return int 0 if successful, -1 if not
 */
int cache_get(cache_t *cache, char *key, char *data_out, size_t *size_out,
              void (*cache_miss_resolver)(cache_entry_t *cache_entry,
                                          void          *arg),
              void *arg);

/**
 * @brief Set a cache entry. Must be called in the miss handler function.
 *
 * @param entry Entry to set
 * @param data Data to set
 * @param size Size of the data
 *
 * @return int 0 if successful, -1 if not
 */
int cache_set(cache_entry_t *entry, char *data, size_t size);

/**
 * @brief Cache to free
 * @param cache Cache to free
 */
void cache_free(cache_t *cache);

#endif
