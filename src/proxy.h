/**
 * @file proxy.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Implementation of the concurrent proxy server
 * @version 0.1
 * @date 2023-04-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef PROXY_H
#define PROXY_H

#include "cache.h"
#include "prefetch.h"
#include "request.h"
#include "response.h"
#include "server.h"

/**
 * @brief Initialize the proxy server
 * 
 * @param port Port to listen on
 * @param cache_ttl Cache TTL in seconds
 * @param prefetch_depth Prefetch depth
 * @param verbose Verbose mode
 */
void proxy_init(int port, int cache_ttl, int prefetch_depth, int verbose);

/**
 * @brief Start the proxy server
 * 
 */
void proxy_start();

#endif
