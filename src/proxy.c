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
#include <sys/types.h>
#include <unistd.h>

#include "cache.h"
#include "prefetch.h"
#include "proxy.h"
#include "request.h"
#include "response.h"
#include "server.h"

// Global variables
char *cache_path;
char *blocklist_path;
int   port;
int   cache_ttl;
int   prefetch_depth;
int   verbose;
int   serverfd;
int   clientfd;
int   proxyfd;

// Function prototypes
void  handle_client(void *arg);
void *handle_response(void *arg);
void *handle_prefetch(void *arg);

/**
 * @brief Initialize the proxy server
 *
 * @param port Port to listen on
 * @param cache_ttl Cache TTL in seconds
 * @param prefetch_depth Prefetch depth
 * @param verbose Verbose mode
 */
void proxy_init(char *cache_path, char *blocklist_path, int port, int cache_ttl,
                int prefetch_depth, int verbose) {
    if (verbose) {
        printf("Initializing the proxy server\n");
    }
    // Set the global variables
    cache_path     = cache_path;
    blocklist_path = blocklist_path;
    port           = port;
    cache_ttl      = cache_ttl;
    prefetch_depth = prefetch_depth;
    verbose        = verbose;

    // Initialize the cache
    cache_init(cache_path, cache_ttl, verbose);

    // Initialize the Server
    server_init(port, verbose, handle_client);
}

/**
 * @brief Start the proxy server
 *
 */
void proxy_start() {
    if (verbose) {
        printf("Starting the proxy server\n");
    }
    // Start the server
    server_start();
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

    // Destroy the cache
    cache_destroy();
}

/**
 * @brief Exit the child process
 */
void exit_child() {
    if (verbose) {
        printf("Exiting the child process\n");
    }
    // Close the client socket
    close(clientfd);

    // Exit the child process
    exit(0);
}

/**
 * @brief Handle a request
 *
 * @param arg Request
 * @return void*
 */
void handle_client(connection_t *connection) {
    // Child process
    int connection_keep_alive = 0;
    // Recv the request from the client
    request_t *request = request_get(connection);
    if (request == NULL) {
        exit_child();
    }

    connection_keep_alive = request_is_connection_keep_alive(request);

    // Check if the request is in the blocklist

    // Get the response from the cache
    response_t *response = cache_get(request);

    // If the response is NULL, then the response is not in the cache
    if (response == NULL) {
        // Get the response from the server
        response = response_get(request);

        // If the response is NULL, then the server is down
        if (response == NULL) {
            // Send a 504 Gateway Timeout response
            response_send(NULL, clientfd);
        } else {
            // Send the response to the client
            response_send(response, clientfd);

            // Set the response in the cache
            cache_set(request, response);
        }
    } else {
        // Send the response to the client
        response_send(response, clientfd);
    }

    // Free the request
    request_free(request);

    // Free the response
    response_free(response);

    // Close the client socket
    close(clientfd);

    // Exit the thread
    pthread_exit(NULL);
}

/**
 * @brief Handle not found in cache
 *
 * @param arg Request
 */
void *handle_response(void *arg) {
    // TODO
}
