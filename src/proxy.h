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

#define CACHE_PATH "cache"

/**
 * @brief Initialize the proxy server
 *
 * @param port Port to listen on
 * @param cache_ttl Cache TTL in seconds
 * @param prefetch_depth Prefetch depth
 * @param verbose Verbose mode
 */
void proxy_init(char *cache_path, char *blocklist_path, int port, int cache_ttl,
                int prefetch_depth, int verbose);

/**
 * @brief Start the proxy server
 *
 */
void proxy_start();

/**
 * @brief Stop the proxy server
 *
 */
void proxy_stop();

/**
 * @brief Check if the proxy server is running
 *
 * @return int 1 if running, 0 if not
 */
int proxy_is_running();

#endif
