/**
 * @file proxy.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "blocklist.h"
#include "cache.h"
#include "debug.h"
#include "prefetch.h"
#include "proxy.h"
#include "request.h"
#include "response.h"
#include "server.h"

// Global variables
int          port;
int          verbose;
int          proxyfd;
blocklist_t *blocklist;
cache_t     *cache;
int          proxy_running = 0;

// Function prototypes
void handle_client(connection_t *connection);
// void *handle_response(void *arg);
// void *handle_prefetch(void *arg);
void proxy_cache_miss_resolver(cache_entry_t *entry, void *arg);

/**
 * @brief Initialize the proxy server
 * @details This function will initialize the proxy server, blocklist, cache,
 * and prefetcher
 *
 * @param port Port to listen on
 * @param cache_ttl Cache TTL in seconds
 * @param prefetch_depth Prefetch depth
 * @param verbose Verbose mode
 */
void proxy_init(char *_cache_path, char *_blocklist_path, int _port,
                int _cache_ttl, int _prefetch_depth, int _verbose) {
    if (verbose) {
        printf("Initializing the proxy server\n");
    }
    // Set the global variables
    port    = _port;
    verbose = _verbose;

    // Initialize the blocklist
    blocklist = blocklist_init(_blocklist_path);
    if (blocklist == NULL) {
        fprintf(stderr, "Error: Failed to initialize the blocklist\n");
    }

    // Initialize the cache
    cache = cache_init(_cache_path, _cache_ttl, _verbose);
}

/**
 * @brief Start the proxy server
 *
 */
void proxy_start() {
    if (verbose) {
        printf("Starting the proxy server\n");
    }
    proxy_running = 1;
    // Start the server
    server_config_t config;
    memset(&config, 0, sizeof(config));
    config.port          = port;
    config.verbose       = verbose;
    config.forking       = 0;
    config.handle_client = handle_client;
    server_start(config);

    // Wait for the server to stop
    while (server_is_running())
        sleep(1);

    DEBUG_PRINT(" !!  -- Server stopped -- SUCCESS \n");

    // Destroy the cache
    cache_free(cache);

    // Destroy the blocklist
    blocklist_free(blocklist);

    proxy_running = 0;
}

/**
 * @brief Stop the proxy server
 *
 */
void proxy_stop() {
    if (verbose) {
        printf("Stopping the proxy server\n");
    }
    // Stop the server
    server_stop();
}

int proxy_is_running() { return proxy_running; }

/**
 * @brief Handle a new client connection.
 *
 * @param arg Request
 * @return void*
 */
void handle_client(connection_t *connection) {
    if (verbose) {
        printf("Handling a new client connection\n");
    }
    // Child process
    int             connection_keep_alive = 1;
    http_message_t *message;
    request_t      *request;
    response_t     *response;

    // Get the time the connection was accepted
    struct timeval time_accept;
    gettimeofday(&time_accept, NULL);

    while (connection_keep_alive) {
        // Recv the request from the client
        message = http_message_recv(connection);
        request = request_parse(message);
        if (request == NULL) {
            DEBUG_PRINT("Error: Failed to parse the request\n");
            response_send_error(connection, 400, "Bad Request");
            return;
        }

        // Check if the request is in the blocklist
        if (blocklist_check(blocklist, request->host)) {
            // Send a 403 Forbidden response
            response_send_error(connection, 403, "Forbidden");
            DEBUG_PRINT("Error: Request is in the blocklist\n");
            // response_send(NULL, connection);
            // continue;
            // TODO: Change this
            request_free(request);
            return;
        }

        // connection_keep_alive = request_is_connection_keep_alive(request);
        // connection_keep_alive |= !http_message_header_compare(
        //     message, "Proxy-Connection", "Keep-Alive");
        // DEBUG_PRINT("Connection: Keep-Alive = %d\n", connection_keep_alive);
        http_message_header_set(message, "Connection", "close");

        // Add the proxy headers
        http_message_header_set(message, "Forwarded", connection->ip);
        http_message_header_set(message, "Via", "1.1 MatthewTetaProxy");
        // Remove proxy headers
        http_message_header_remove(message, "Proxy-Connection");
        http_message_header_remove(message, "Proxy-Authorization");
        http_message_header_remove(message, "Proxy-Authenticate");

        // Get a hash key from the request
        char key[1024];
        request_get_key(request, key, 1024);
        // if (strlen(key) == 0) {
        if (1) {
            // Request is not cacheable - patch through
            response = response_fetch(request);
            if (response == NULL) {
                // Send a 504 Gateway Timeout response
                // This may be another error
                // TODO: Check if this is the correct error
                response_send_error(connection, 404, "Not Found");
                // response_send(NULL, clientfd);
                DEBUG_PRINT(
                    "Error: Failed to get the response from the server\n");
                request_free(request);
                return;
            }
            // Send the response to the client
            if (0 != response_send(response, connection)) {
                fprintf(stderr, "Error: Failed to send the response to the "
                                "client\n");
                request_free(request);
                response_free(response);
                return;
            }
        } else {
            // Request is cacheable
            char  *data = NULL;
            size_t size = 0;
            // Get the response from the cache
            cache_get(cache, key, data, &size, proxy_cache_miss_resolver,
                      request);
            message  = http_message_create(data, size);
            response = response_parse(message);

            // If the response is NULL, then the response is not in the cache
            if (response == NULL) {
                // Send a 504 Gateway Timeout response
                response_send_error(connection, 504, "Gateway Timeout");
                // response_send(NULL, clientfd);
                DEBUG_PRINT(
                    "Error: Failed to get the response from the cache\n");
                request_free(request);
                return;
            }
            // Send the response to the client
            if (0 != response_send(response, connection)) {
                fprintf(stderr,
                        "Error: Failed to send the response to the client\n");
                request_free(request);
                response_free(response);
                return;
            }
        }

        // Free the request
        request_free(request);

        // Free the response
        response_free(response);

        break;
    }

    return;
}

/**
 * @brief Handle not found in cache
 *
 * @param arg Request to fetch the response
 */
void proxy_cache_miss_resolver(cache_entry_t *entry, void *arg) {
    DEBUG_PRINT("Resolving cache miss\n");
    request_t *request = (request_t *)arg;
    // Open a connection to the server
    connection_t *server = connect_to_hostname(request->host, request->port);
    if (server == NULL) {
        DEBUG_PRINT("Error: Failed to connect to the server\n");
        return;
    }
    // Send the request to the server
    if (0 != request_send(request, server)) {
        DEBUG_PRINT("Error: Failed to send the request to the server\n");
        return;
    }
    // Recv the response from the server
    http_message_t *message = http_message_recv(server);

    // Before parsing the response (further), cache it
    char  *buffer = NULL;
    size_t size;
    http_get_message_buffer(message, &buffer, &size);
    cache_set(entry, buffer, size);

    // Close the connection to the server
    close_connection(server);
}
