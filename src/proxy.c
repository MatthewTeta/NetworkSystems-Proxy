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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "request.h"
#include "response.h"
#include "cache.h"
#include "server.h"
#include "prefetch.h"
#include "proxy.h"

// Global variables
int port;
int cache_ttl;
int prefetch_depth;
int verbose;
int serverfd;
int clientfd;
int proxyfd;

// Function prototypes
void *handle_request(void *arg);
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
void proxy_init(int port, int cache_ttl, int prefetch_depth, int verbose)
{
    // Initialize the cache
    cache_init(cache_ttl, verbose);

    // Initialize the server
    server_init(port, verbose, handle_request);
}

/**
 * @brief Start the proxy server
 * 
 */
void proxy_start()
{
    // Start the server
    server_start();
}

/**
 * @brief Handle a request
 * 
 * @param arg Request
 * @return void* 
 */
void *handle_request(void *arg)
{
    // Get the request
    request_t *request = (request_t *)arg;

    // Get the response from the cache
    response_t *response = cache_get(request);

    // If the response is NULL, then the response is not in the cache
    if (response == NULL)
    {
        // Get the response from the server
        response = response_get(request);

        // If the response is NULL, then the server is down
        if (response == NULL)
        {
            // Send a 504 Gateway Timeout response
            response_send(NULL, clientfd);
        }
        else
        {
            // Send the response to the client
            response_send(response, clientfd);

            // Set the response in the cache
            cache_set(request, response);
        }
    }
    else
    {
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
