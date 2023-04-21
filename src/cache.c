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

#define CACHE_NUM_BUCKET_BYTES 8
#define CACHE_NUM_BUCKETS      (CACHE_NUM_BUCKET_BYTES << 8)

typedef enum cache_entry_status {
    // Another user is fetching from the origin server
    CACHE_ENTRY_STATUS_IN_PROGRESS,
    // Valid (Need to check timestamp)
    CACHE_ENTRY_STATUS_OK,
    // The entry is invalid but no thread has claimed responsibility for
    // updating the entry
    CACHE_ENTRY_STATUS_INVALID,
} cache_entry_status_t;

struct cache_entry_storage {
    cache_entry_t          entry;
    cache_entry_status_t   status;
    int                    num_users;
    time_t                 timestamp;
    cache_entry_storage_t *next;
};

// Hash bucket with linked list pointers embedded
typedef struct cache_bucket {
    cache_entry_storage_t *first;
} cache_bucket_t;

struct cache {
    char          *path;
    int            ttl;
    int            verbose;
    int            num_users;
    sem_t          mutex;
    cache_bucket_t hash_table[CACHE_NUM_BUCKETS];
};

// Private function prototypes
void cache_entry_storage_free(cache_entry_storage_t *entry);
void cache_file_touch(char *path);

/**
 * @brief Initialize the cache
 *
 * @param path Path to cache directory
 * @param ttl Cache TTL in seconds
 * @param verbose Verbose mode
 */
cache_t *cache_init(char *path, int ttl, int verbose) {
    if (path == NULL) {
        fprintf(stderr, "cache_init: path is NULL\r\n");
        exit(1);
    }
    if (ttl <= 0) {
        fprintf(stderr, "cache_init: ttl is negative\r\n");
        exit(1);
    }
    cache_t *cache = malloc(sizeof(cache_t));
    memset(cache, 0, sizeof(cache_t));
    cache->path    = path;
    cache->ttl     = ttl;
    cache->verbose = verbose;
    // Initialize the cache mutex
    sem_init(&cache->mutex, 0, 1);
    // Create the cache directory if it does not exist
    mkdir(path, 0777);

    return cache;
}

/**
 * @brief Cache to free
 * @param cache Cache to free
 */
void cache_free(cache_t *cache) {
    if (cache == NULL) {
        return;
    }
    // Wait for total number of users to be zero (all threads have exited)
    while (1) {
        sem_wait(&cache->mutex);
        if (cache->num_users == 0) {
            sem_post(&cache->mutex);
            break;
        }
        DEBUG_PRINT("-- SOMEHOW THERE ARE STILL USERS -- \n");
        sem_post(&cache->mutex);
        sleep(1);
    }
    cache_entry_storage_t *ent, *tmp;
    // Free all of the items in the hash table
    for (size_t i = 0; i < CACHE_NUM_BUCKETS; i++) {
        ent = cache->hash_table[i].first;
        while (ent != NULL) {
            tmp = ent->next;
            cache_entry_storage_free(ent);
            ent = tmp;
        }
    }
    // Destroy the cache mutex
    sem_destroy(&cache->mutex);
    free(cache);
}

/**
 * @brief Get a cache entry, call the resolver function to get the item from the
 * cache if not found or invalidated by ttl. This is thread safe.
 *
 * @param cache Cache to get from
 * @param key Key to get
 * @param data_out Buffer to store the data in
 * @param data_out_len Length of the data buffer
 * @param cache_miss_resolver Function to call if the cache entry is not found
 * (char *filename to save cache data, void *arg provided in call)
 * @param arg Argument to pass to the cache miss resolver
 * @return int 0 if successful, -1 if not
 */
int cache_get(cache_t *cache, char *key, char *data_out, size_t *size_out,
              void (*cache_miss_resolver)(cache_entry_t *cache_entry,
                                          void          *arg),
              void *arg) {
    if (!key) {
        fprintf(stderr, "cache_get: key is NULL\r\n");
        return -1;
    }

    uint8_t                hash[16];
    char                   hash_str[33];
    char                   cache_entry_path[1024];
    size_t                 bucket_idx             = 0;
    int                    synchronize_local_copy = 0;
    cache_entry_t         *cache_entry            = NULL;
    cache_bucket_t        *hash_bucket            = NULL;
    cache_entry_storage_t *cache_entry_ptr        = NULL;
    cache_entry_status_t   status                 = CACHE_ENTRY_STATUS_INVALID;

    // Compute the MD5 hash of the request key
    md5String(key, hash);

    // Convert the hash to a string for the cache entry path
    for (int i = 0; i < 16; i++) {
        sprintf(hash_str + (i * 2), "%02x", hash[i]);
    }
    sprintf(cache_entry_path, "%s/%s", cache->path, hash_str);
    DEBUG_PRINT("cache_get: key=%s, hash=%s\r\n", key, hash_str);

    // Compute the index in the hash table from the 16 bytes hash
    // size_t bucket_offset = CACHE_NUM_BUCKET_BYTES * 16;
    for (size_t i = 0; i < CACHE_NUM_BUCKET_BYTES; i++) {
        bucket_idx |= (hash[i] << (i * 8));
    }
    bucket_idx = bucket_idx % CACHE_NUM_BUCKETS;
    DEBUG_PRINT("Bucket Index: %lu / %d\n", bucket_idx, CACHE_NUM_BUCKETS);

    // Get the cache entry from the hash table if it exists
    sem_wait(&cache->mutex);
    hash_bucket = &cache->hash_table[bucket_idx];
    // Traverse linked list to find the cache entry in the bucket if it exists
    cache_entry_ptr = hash_bucket->first;
    while (cache_entry_ptr != NULL) {
        if (strcmp(cache_entry_ptr->entry.key, key) == 0) {
            break;
        }
        cache_entry_ptr = cache_entry_ptr->next;
    }
    if (cache_entry_ptr == NULL) {
        // Cache entry does not exist, create it
        // cache_file_touch(cache_entry_path);
        cache_entry_ptr = malloc(sizeof(cache_entry_storage_t));
        memset(cache_entry_ptr, 0, sizeof(cache_entry_storage_t));
        cache_entry_ptr->entry.key  = strdup(key);
        cache_entry_ptr->entry.path = strdup(cache_entry_path);
        cache_entry_ptr->status     = CACHE_ENTRY_STATUS_INVALID;
        cache_entry_ptr->next       = NULL;
        // Add the cache entry to the hash table
        if (hash_bucket->first == NULL) {
            hash_bucket->first = cache_entry_ptr;
        } else {
            cache_entry_ptr->next = hash_bucket->first;
            hash_bucket->first    = cache_entry_ptr;
        }
    }
    sem_post(&cache->mutex);

    // Now we have a cache entry, wait for it to be valid or make it valid

    while (1) {
        // Protect the cache with a mutex lock
        sem_wait(&cache->mutex);
        status = cache_entry_ptr->status;
        switch (status) {
        case CACHE_ENTRY_STATUS_OK:
            // Cache entry is valid, use it
            // Check if the cache entry is expired
            if (time(NULL) - cache_entry_ptr->timestamp > cache->ttl) {
                // Cache entry is expired, invalidate it
                cache_entry_ptr->status = CACHE_ENTRY_STATUS_INVALID;
                goto end_switch;
            }
            // Use the resource
            cache->num_users++;
            cache_entry_ptr->num_users++;
            sem_post(&cache->mutex);
            goto obtained_cache_entry;
        case CACHE_ENTRY_STATUS_IN_PROGRESS:
            // Cache entry is in progress, wait for it to finish
            goto end_switch;
        case CACHE_ENTRY_STATUS_INVALID:
            // Cache entry is invalid
            if (cache_entry_ptr->num_users == 0) {
                // No one is using the cache entry, take ownership of obtaining
                // the new copy
                cache->num_users++;
                cache_entry_ptr->num_users++;
                cache_entry_ptr->status = CACHE_ENTRY_STATUS_IN_PROGRESS;
                synchronize_local_copy  = 1;
                sem_post(&cache->mutex);
                goto obtained_cache_entry;
            } else {
                // Someone is using the cache entry, wait for them to finish
            }
            goto end_switch;
        default:
            fprintf(stderr, "cache_get: invalid cache entry status\r\n");
            exit(1);
        }
    end_switch:
        // Release the mutex lock
        sem_post(&cache->mutex);
        // Wait for the cache entry to be ready
        sleep(1);
    }
obtained_cache_entry:

    // Get the cache entry for the caller
    cache_entry = &cache_entry_ptr->entry;

    if (synchronize_local_copy) {
        // Synchronize the local copy of the cache entry
        // Request the cache entry from the web server, and save it to the local
        // file
        cache_miss_resolver(cache_entry, arg);
        // Update the cache entry status
        sem_wait(&cache->mutex);
        cache_entry_ptr->status    = CACHE_ENTRY_STATUS_OK;
        cache_entry_ptr->timestamp = time(NULL);
        sem_post(&cache->mutex);
    }

    FILE *file = fopen(cache_entry_path, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }

    // Determine the file size
    fseek(file, 0, SEEK_END);
    *size_out = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate or reallocate a buffer for the file contents
    data_out = malloc(*size_out);
    if (!data_out) {
        perror("Failed to allocate memory");
        fclose(file);
        exit(1);
    }

    // Read the file contents into the buffer
    size_t ntot = 0;
    while (ntot < *size_out) {
        size_t n = fread(data_out + ntot, 1, *size_out - ntot, file);
        if (n == 0) {
            perror("Failed to read file");
            fclose(file);
            exit(1);
        }
        ntot += n;
    }

    // Close the file
    fclose(file);

    // Release the resource
    sem_wait(&cache->mutex);
    // Decrement the number of users of the cache entry
    cache_entry_ptr->num_users--;
    // Decrement the number of users of the cache
    cache->num_users--;
    // Release the mutex lock
    sem_post(&cache->mutex);

    return 0;
}

/**
 * @brief Set a cache entry. Must be called in the miss handler function.
 *
 * @param entry Entry to set
 * @param data Data to set
 * @param size Size of the data
 *
 * @return int 0 if successful, -1 if not
 */
int cache_set(cache_entry_t *entry, char *data, size_t size) {
    // Check if the entry is valid
    if (!entry) {
        return -1;
    }
    // Check if the data is valid
    if (!data) {
        return -1;
    }
    // Check if the size is valid
    if (size == 0) {
        return -1;
    }
    char *path = entry->path;
    // Open the file for writing
    FILE *file = fopen(path, "wb");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }
    // Write the data to the file
    size_t ntot = 0;
    while (ntot < size) {
        size_t n = fwrite(data + ntot, 1, size - ntot, file);
        if (n == 0) {
            perror("Failed to write file");
            fclose(file);
            exit(1);
        }
        ntot += n;
    }
    // Close the file
    fclose(file);
    return 0;
}

/**
 * @brief Cache entry storage free
 * @details This function will free a cache entry
 * @param entry Cache entry to free
 */
void cache_entry_storage_free(cache_entry_storage_t *entry) {
    if (!entry) {
        return;
    }
    free(entry->entry.key);
    free(entry->entry.path);
    free(entry);
}

// Private functions

/**
 * @brief Touch a cache file (Create if not
 * exists)
 */
// void cache_file_touch(char *path) {
//     // Create the meta file
//     DEBUG_PRINT("cache_entry_touch: creating file %s\r\n", path);
//     FILE *fp = fopen(path, "w");
//     if (!fp) {
//         fprintf(stderr, "cache_entry_touch: failed to create file\r\n");
//         exit(1);
//     }
//     fclose(fp);
// }
