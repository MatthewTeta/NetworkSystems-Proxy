/**
 * @file server.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief 
 * @version 0.1
 * @date 2023-04-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef SERVER_H
#define SERVER_H

#include "request.h"

/**
 * @brief Server structure
 * 
 */
typedef struct server {
    int    port;    // Server port
    int    verbose; // Verbose mode
    void (*handle_request)(request_t *request, int clientfd); // Request handler
} server_t;

/**
 * @brief Initialize the server
 * 
 * @param port Port to listen on
 * @param verbose Verbose mode
 * @param handle_request Request handler
 */
void server_init(int port, int verbose, void (*handle_request)(request_t *request, int clientfd));

/**
 * @brief Start the server
 * 
 */
void server_start();

#endif
