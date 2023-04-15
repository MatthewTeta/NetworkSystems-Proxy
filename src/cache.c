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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "md5.h"
#include "request.h"
#include "response.h"

typedef enum cache_entry_status {
    CACHE_ENTRY_STATUS_NOT_FOUND = 0,
    CACHE_ENTRY_STATUS_OK,
    CACHE_ENTRY_STATUS_IN_PROGRESS,
    CACHE_ENTRY_STATUS_INVALID,
} cache_entry_status_t;

char   *cache_path;
int     cache_ttl;
int     cache_verbose;
uint8_t cache_entry_hash_usage[16][16][16][16]
                              [2]; // Indicate if a cache entry is in use
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
 * @brief Get a response from the cache (or from the web if not exists)
 *
 * @param request Request to get response for
 * @return response_t* Response from cache
 */
response_t *cache_get(request_t *request) {
    if (!request) {
        fprintf(stderr, "cache_get: request is NULL\r\n");
        exit(1);
    }

    char                *key;
    uint8_t              hash[16];
    char                 hash_str[33];
    char                 cache_entry_path[1024];
    cache_entry_status_t status;
    int                  wait                   = 1;
    int                  synchronize_local_copy = 0;

    // Compute the MD5 hash of the request key
    request_get_key(request, key);
    md5String(key, hash);

    // Only use part of the hash for locks (16*16*16*16 = 65532 locks)
    uint8_t *cache_entry_hash_usage_ptr =
        cache_entry_hash_usage[hash[0]][hash[1]][hash[2]][hash[3]];

    // Convert the hash to a string for the cache entry path
    for (int i = 0; i < 16; i++) {
        sprintf(hash_str + (i * 2), "%02x", hash[i]);
    }
    sprintf(cache_entry_path, "%s/%s", cache_path, hash_str);

    while (wait) {
        // Protect the cache with a mutex lock
        sem_wait(&cache_mutex);
        status = cache_entry_hash_usage_ptr[1];
        switch (status) {
        case CACHE_ENTRY_STATUS_OK:
            // Cache entry is valid, use it
            cache_entry_hash_usage_ptr[0]++;
            wait = 0;
            break;
        case CACHE_ENTRY_STATUS_NOT_FOUND:
            // Cache entry is not found, create it
            cache_entry_touch(cache_entry_path);
            cache_entry_hash_usage_ptr[1] = CACHE_ENTRY_STATUS_IN_PROGRESS;
            wait                          = 0;
            synchronize_local_copy        = 1;
            break;
        case CACHE_ENTRY_STATUS_IN_PROGRESS:
            // Cache entry is in progress, wait for it to finish
            break;
        case CACHE_ENTRY_STATUS_INVALID:
            // Cache entry is invalid
            if (cache_entry_hash_usage_ptr[0] == 0) {
                // No one is using the cache entry, delete it
                cache_entry_hash_usage_ptr[1] = CACHE_ENTRY_STATUS_IN_PROGRESS;
                wait                          = 0;
                synchronize_local_copy        = 1;
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
        // TODO: Request the cache entry from the web server
        // TODO: Save the cache entry to the local file system
        // Update the cache entry status
        sem_wait(&cache_mutex);
        cache_entry_hash_usage_ptr[1] = CACHE_ENTRY_STATUS_OK;
        cache_entry_hash_usage_ptr[0]++;
        sem_post(&cache_mutex);
    }

    // Assume we can read the resource
    // TODO: Read the resource from the local file system
    response_t *response = response_create();
    response_set_status(response, 200);
    response_set_header(response, "Content-Type", "text/html");
    response_set_body(response, "Hello World!", 11);

    // Release the cache entry
    sem_wait(&cache_mutex);
    cache_entry_hash_usage_ptr[0]--;
    sem_post(&cache_mutex);
}

/**
 * @brief Get the status of a cache entry. Must be called with the cache mutex
 *
 * @param hash_str Hash string of cache entry
 *
 * @return cache_entry_status_t Status of cache entry
 */
cache_entry_status_t cache_get_status(char *filename) {
    if (!filename) {
        fprintf(stderr, "cache_get_status: filename is NULL\r\n");
        exit(1);
    }

    time_t      created = 0;
    struct stat st;
    int         readable = 0;
    int         writable = 0;

    // Stat the file
    if (stat(filename, &st)) {
        // File does not exist
        return CACHE_ENTRY_STATUS_NOT_FOUND;
    }

    // Check mode bits to determine status of file
    // File exists
    if (st.st_mode & S_IRUSR) {
        readable = 1;
    }
    if (st.st_mode & S_IWUSR) {
        writable = 1;
    }

    if (readable && writable) {
        // File is readable and writable
        return CACHE_ENTRY_STATUS_OK;
    } else if (readable && !writable) {
        // File is readable but not writable
        return CACHE_ENTRY_STATUS_IN_USE;
    } else if (!readable && writable) {
        // File is writable but not readable
        return CACHE_ENTRY_STATUS_IN_PROGRESS;
    } else {
        // File is not readable or writable
        return CACHE_ENTRY_STATUS_NOT_FOUND;
    }

    // Check if the meta file exists
    filename = cache_entry_status.path;
    sprintf(filename, ".%s/%s", cache_path, hash_str);
    fp = fopen(filename, "r");
    if (!fp) {
        // No meta file means the cache entry is not finished yet (or does not
        // exist)
        cache_entry_status.valid = 0;
    } else {
        fclose(fp);
        // Get the creation time of the meta file
        stat(filename, &st);
        created                  = st.st_ctime;
        cache_entry_status.valid = 1;
        // Check if the cache entry is expired (valid)
        if (created + cache_ttl < time(NULL)) {
            remove(filename);
        }
    }

    // Check if the cache entry exists
    fp = fopen(filename, "r");
    if (!fp) {
        // No cache entry file means the cache entry does not exist
        cache_entry_status.exists = 0;
    } else {
        cache_entry_status.exists = 1;
        fclose(fp);
    }
}

/**
 * @brief Touch a cache entry to indicate that it is in progress (Create if not
 * exists)
 */
void cache_entry_touch() {
    // Create the cache directory if it does not exist
    mkdir(cache_path, 0777);

    // Create the meta file
    FILE *fp = fopen(cache_entry_status.path, "w");
    if (!fp) {
        fprintf(stderr, "cache_entry_touch: failed to create meta file\r\n");
        exit(1);
    }
    fclose(fp);
}
