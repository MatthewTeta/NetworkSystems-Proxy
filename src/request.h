/**
 * @file request.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef REQUEST_H
#define REQUEST_H

#include "request.h"

#include <stdio.h>

#include "http.h"
#include "server.h"

#define REQUEST_URI_REGEX                                                      \
    "^(\\w+)\\s+(http[s]?://)?([^/:]+)(:[\\d]+)?(.*)\\s+(HTTP/[\\d\\.]+)"

/**
 * @brief Request structure
 *
 */
typedef struct request {
    http_message_t *message; // HTTP message
    int             https;   // HTTPS request
    int             port;    // Request port
    char           *host;    // Request host
    char           *method;  // Request method
    char           *uri;     // Request URI
    char           *version; // Request version
} request_t;

/**
 * @brief Recieve a request from a client socket
 *
 * @param clientfd Client socket
 * @return request_t* Request
 */
request_t *request_recv(connection_t *connection);

/**
 * @brief Free a request
 *
 * @param request Request to free
 */
void request_free(request_t *request);

/**
 * @brief Check if a request is cacheable
 *
 * @param request
 * @return int
 */
int request_is_cacheable(request_t *request);

/**
 * @brief Check if a request is keep-alive
 *
 * @param request
 */
int request_is_connection_keep_alive(request_t *request);

/**
 * @brief Get a key to hash the request on. Return NULL if the request is not
 * cacheable.
 *
 * @param request
 * @param key
 */
void request_get_key(request_t *request, char *key);

#endif
