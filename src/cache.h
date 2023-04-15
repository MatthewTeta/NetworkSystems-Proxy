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

#include "request.h"
#include "response.h"

/**
 * @brief Initialize the cache
 *
 * @param path Path to cache directory
 * @param ttl Cache TTL in seconds
 * @param verbose Verbose mode
 */

void cache_init(char *path, int ttl, int verbose);

/**
 * @brief Destroy the cache
 *
 */
void cache_destroy();

/**
 * @brief Get a response from the cache
 *
 * @details This function should return a response from the cache if it exists,
 * otherwise it should fetch the response from the server and add it to the
 * cache. The cache will block if the request is currently being fetched from
 * the server. The cache will use the MD5 hash of the request key as the file
 * name. Metadata about the cache entry will be stored in a doubly linked list.
 *
 * @param request Request to get response for
 * @return response_t* Response from cache
 */
response_t *cache_get(request_t *request);

#endif
