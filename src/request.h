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

#include "connection.h"
#include "http.h"

// #define REQUEST_REGEX_PATH "([^ \\?]*)?"

#define REQUEST_REGEX_WHITESPACE     "[ \t]+"
#define REQUEST_REGEX_METHOD         "(GET)"
#define REQUEST_REGEX_PROTOCOL       "(http[s]?://)?"
#define REQUEST_REGEX_HOSTNAME       "([^/:\\?]+)?"
#define REQUEST_REGEX_PORT           "(:([0-9]+))?"
#define REQUEST_REGEX_PATH           "([^ \\?]*)"
#define REQUEST_REGEX_QUERY          "(\\?([^ ]*))?"
#define REQUEST_REGEX_VERSION        "(HTTP/[0-9]+\\.?[0-9]*)"
#define REQUEST_REGEX_INDEX_METHOD   1
#define REQUEST_REGEX_INDEX_PROTOCOL 2
#define REQUEST_REGEX_INDEX_HOSTNAME 3
#define REQUEST_REGEX_INDEX_PORT     5
#define REQUEST_REGEX_INDEX_PATH     6
#define REQUEST_REGEX_INDEX_QUERY    8
#define REQUEST_REGEX_INDEX_VERSION  9
#define REQUEST_REGEX_INDEX_COUNT    10
#define REQUEST_REGEX                                                          \
    REQUEST_REGEX_METHOD REQUEST_REGEX_WHITESPACE REQUEST_REGEX_PROTOCOL       \
        REQUEST_REGEX_HOSTNAME REQUEST_REGEX_PORT REQUEST_REGEX_PATH           \
            REQUEST_REGEX_QUERY REQUEST_REGEX_WHITESPACE REQUEST_REGEX_VERSION
// #define REQUEST_REGEX
//     REQUEST_REGEX_METHOD REQUEST_REGEX_WHITESPACE REQUEST_REGEX_PROTOCOL
//         REQUEST_REGEX_HOSTNAME REQUEST_REGEX_PORT REQUEST_REGEX_PATH
//             REQUEST_REGEX_QUERY REQUEST_REGEX_WHITESPACE
//             REQUEST_REGEX_VERSION

// #define REQUEST_REGEX_URI
//     "(\\w+)[ ]+(http[s]?://)?([^/:]+)?(:([0-9]+))?([^ \\?]*)?(\\?[^ ]+)?[ "
//     "]+(HTTP\\/[0-9]+\\.?[0-9]+)"

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
    char           *query;   // Request query string
    char           *version; // Request version
} request_t;

/**
 * @brief Recieve a request from a client socket
 *
 * @param connection socket to recv from
 * @return request_t* Request
 */
request_t *request_recv(connection_t *connection);

/**
 * @brief Send a request to the server. Prepares the http_message_t with the
 * header line, host header.
 *
 * @param request Request to send
 * @param connection Connection
 * @return int 0 on success, -1 on failure
 */
int request_send(request_t *request, connection_t *connection);

/**
 * @brief Free a request
 *
 * @param request Request to free
 */
void request_free(request_t *request);

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
 * @param key Output key
 * @param len Length of key
 */
void request_get_key(request_t *request, char *key, size_t len);

/**
 * @brief Parse a request from a message
 *
 * @param message Message to parse
 * @return request_t* Request
 */
request_t *request_parse(http_message_t *message);

/**
 * @brief Determine if a request is cacheable
 *
 * @param request Request to check
 * @return int 1 if cacheable, 0 otherwise
 */
int request_is_cacheable(request_t *request);

#endif
