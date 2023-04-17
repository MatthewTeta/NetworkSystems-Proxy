/**
 * @file cache.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "cache.h"

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "debug.h"
#include "md5.h"

typedef enum cache_entry_status {
    CACHE_ENTRY_STATUS_NOT_FOUND = 0,
    CACHE_ENTRY_STATUS_OK,
    CACHE_ENTRY_STATUS_IN_PROGRESS,
    CACHE_ENTRY_STATUS_INVALID,
} cache_entry_status_t;

typedef struct cache_entry_storage {
    cache_entry_status_t status;
    int                  num_users;
    time_t               timestamp;
} cache_entry_storage_t;

// Private function prototypes
void cache_entry_touch();

char *cache_path;
int   cache_ttl;
int   cache_verbose;
cache_entry_storage_t
    cache_entry_hash_usage[16][16][16]
                          [16]; // Indicate if a cache entry is in use
                                // [hash][num_users, cache_entry_status]
sem_t cache_mutex;

/**
 * @brief Initialize the cache
 *
 * @param path Path to cache directory
 * @param ttl Cache TTL in seconds
 * @param verbose Verbose mode
 */
void cache_init(char *path, int ttl, int verbose) {
    if (path == NULL) {
        fprintf(stderr, "cache_init: path is NULL\r\n");
        exit(1);
    }
    if (ttl <= 0) {
        fprintf(stderr, "cache_init: ttl is negative\r\n");
        exit(1);
    }
    cache_path    = path;
    cache_ttl     = ttl;
    cache_verbose = verbose;
    memset(cache_entry_hash_usage, 0, sizeof(cache_entry_hash_usage));
    // Initialize the cache mutex
    sem_init(&cache_mutex, 0, 1);
}

/**
 * @brief Destroy the cache
 *
 */
void cache_destroy() {
    // Destroy the cache mutex
    sem_destroy(&cache_mutex);
}

/**
 * @brief Get a cache entry
 *
 * @param key Key to get
 * @param cache_miss_resolver Function to call if the cache entry is not found
 * @param arg Argument to pass to the cache miss resolver
 * @return void* Pointer to the cache entry
 */
cache_entry_t *cache_get(char *key,
                         void(cache_miss_resolver)(char *filepath, void *arg),
                         void *arg) {
    if (!key) {
        fprintf(stderr, "cache_get: key is NULL\r\n");
        exit(1);
    }

    uint8_t              hash[16];
    char                 hash_str[33];
    char                 cache_entry_path[1024];
    cache_entry_status_t status;
    int                  wait                   = 1;
    int                  synchronize_local_copy = 0;

    // Compute the MD5 hash of the request key
    md5String(key, hash);

    // Only use part of the hash for locks (16*16*16*16 = 65532 locks)
    cache_entry_storage_t *cache_entry_hash_usage_ptr =
        &cache_entry_hash_usage[hash[0]][hash[1]][hash[2]][hash[3]];

    // Convert the hash to a string for the cache entry path
    for (int i = 0; i < 16; i++) {
        sprintf(hash_str + (i * 2), "%02x", hash[i]);
    }
    sprintf(cache_entry_path, "%s/%s", cache_path, hash_str);
    DEBUG_PRINT("cache_get: key=%s, hash=%s\r\n", key, hash_str);

    while (wait) {
        // Protect the cache with a mutex lock
        sem_wait(&cache_mutex);
        status = cache_entry_hash_usage_ptr->status;
        switch (status) {
        case CACHE_ENTRY_STATUS_OK:
            // Cache entry is valid, use it
            // Check if the cache entry is expired
            if (time(NULL) - cache_entry_hash_usage_ptr->timestamp >
                cache_ttl) {
                // Cache entry is expired, invalidate it
                cache_entry_hash_usage_ptr->status = CACHE_ENTRY_STATUS_INVALID;
                break;
            }
            cache_entry_hash_usage_ptr->num_users++;
            wait = 0;
            break;
        case CACHE_ENTRY_STATUS_NOT_FOUND:
            // Cache entry is not found, create it
            cache_entry_touch(cache_entry_path);
            cache_entry_hash_usage_ptr->status = CACHE_ENTRY_STATUS_IN_PROGRESS;
            wait                               = 0;
            synchronize_local_copy             = 1;
            break;
        case CACHE_ENTRY_STATUS_IN_PROGRESS:
            // Cache entry is in progress, wait for it to finish
            break;
        case CACHE_ENTRY_STATUS_INVALID:
            // Cache entry is invalid
            if (cache_entry_hash_usage_ptr->num_users == 0) {
                // No one is using the cache entry, delete it
                cache_entry_hash_usage_ptr->status =
                    CACHE_ENTRY_STATUS_IN_PROGRESS;
                wait                   = 0;
                synchronize_local_copy = 1;
            } else {
                // Someone is using the cache entry, wait for them to finish
            }
            break;
        default:
            fprintf(stderr, "cache_get: invalid cache entry status\r\n");
            exit(1);
        }
        // Release the mutex lock
        sem_post(&cache_mutex);
        // Wait for the cache entry to be ready
        sleep(1);
    }

    if (synchronize_local_copy) {
        // Synchronize the local copy of the cache entry
        // Request the cache entry from the web server, and save it to the local
        // file
        cache_miss_resolver(cache_entry_path, arg);
        // Update the cache entry status
        sem_wait(&cache_mutex);
        cache_entry_hash_usage_ptr->status = CACHE_ENTRY_STATUS_OK;
        cache_entry_hash_usage_ptr->num_users++;
        cache_entry_hash_usage_ptr->timestamp = time(NULL);
        sem_post(&cache_mutex);
    }

    // Assume we can read the resource
    cache_entry_t *cache_entry = malloc(sizeof(cache_entry_t));
    memset(cache_entry, 0, sizeof(cache_entry_t));

    FILE *file = fopen(cache_entry_path, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }

    // Determine the file size
    fseek(file, 0, SEEK_END);
    cache_entry->size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate a buffer for the file contents
    char *buffer = malloc(cache_entry->size + 1);
    if (!buffer) {
        perror("Failed to allocate memory");
        fclose(file);
        exit(1);
    }

    // Read the file contents into the buffer
    fread(buffer, 1, cache_entry->size, file);
    buffer[cache_entry->size] = '\0';

    // Create a memory stream and write the buffer to it
    cache_entry->fp = fmemopen(buffer, cache_entry->size, "r");
    if (!cache_entry->fp) {
        perror("Failed to create memory stream");
        free(buffer);
        fclose(file);
        exit(1);
    }

    // Close the file
    fclose(file);

    cache_entry->key  = strdup(key);
    cache_entry->path = strdup(cache_entry_path);

    return cache_entry;
}

/**
 * @brief Cache entry free
 * @details This function will free a cache entry
 * @param entry Cache entry to free
 */
void cache_entry_free(cache_entry_t *entry) {
    if (!entry) {
        return;
    }
    free(entry->key);
    free(entry->path);
    free(entry->buffer);
    fclose(entry->fp);
    free(entry);
}

// Private functions

/**
 * @brief Touch a cache entry to indicate that it is in progress (Create if not
 * exists)
 */
void cache_entry_touch(char *path) {
    // Create the cache directory if it does not exist
    mkdir(cache_path, 0777);

    // Create the meta file
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "cache_entry_touch: failed to create file\r\n");
        exit(1);
    }
    fclose(fp);
}
