/**
 * @file http.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief HTTP definitions and helpers
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef HTTP_H
#define HTTP_H

#include <stdio.h>

#include "server.h"

#define HTTP_HEADER_COUNT_DEFAULT    16
#define MESSAGE_CHUNK_SIZE           1024
#define KEEP_ALIVE_TIMEOUT_MS        10000
#define HTTP_MESSAGE_MAX_HEADER_SIZE 8192
#define HTTP_MESSAGE_MAX_BODY_SIZE   (4 * 1024 * 1024 * 1024) // 4 GB

typedef struct http_message http_message_t;

/**
 * @brief Create a new HTTP message
 *
 * @param data Message data
 * @param size Message size
 * @return http_message_t* HTTP message
 */
http_message_t *http_message_create(char *data, size_t size);

/**
 * @brief Recv an HTTP message from a connection
 *
 * @param connection Connection
 * @return http_message_t* HTTP message
 */
http_message_t *http_message_recv(connection_t *connection);

/**
 * @brief Send an http message to the socket including the header line, headers,
 * and body. This will reconstruct the message from the headers and body.
 *
 * @param message The message to send
 */
int http_message_send(http_message_t *message, connection_t *connection);

/**
 * @brief Free an HTTP message
 *
 * @param message HTTP message
 */
void http_message_free(http_message_t *message);

/**
 * @brief Set the header line from an HTTP message
 *
 * @param message HTTP message
 * @return char* Header line
 */
void http_message_set_header_line(http_message_t *message, char *header_line);

/**
 * @brief Get the header line from an HTTP message
 *
 * @param message HTTP message
 * @return char* Header line
 */
char *http_message_get_header_line(http_message_t *message);

/**
 * @brief Set the body from an HTTP message
 *
 * @param message HTTP message
 * @param body Body
 * @param len Length of body
 * @return char* Body
 */
void http_message_set_body(http_message_t *message, char *body, size_t len);

/**
 * @brief Get the body from an HTTP message
 *
 * @param message HTTP message
 * @return char* Body
 */
char *http_message_get_body(http_message_t *message);

/**
 * @brief Get a header value from a message
 *
 * @param message HTTP message
 * @param key Header key
 * @return char* Header value or NULL if not found
 */
char *http_message_header_get(http_message_t *message, char *key);

/**
 * @brief Set a header value in a list of headers
 *
 * @param headers Headers
 * @param key Header key
 * @param value Header value
 */
void http_message_header_set(http_message_t *message, char *key, char *value);

/**
 * @brief Print HTTP headers
 *
 * @param message HTTP message
 */
void http_message_headers_print(http_message_t *message);

/**
 * @brief Get the HTTP data buffer from a message
 *
 * @param message HTTP message
 * @param data Data buffer (output)
 * @param size Size of data buffer (output)
 */
void http_get_message_buffer(http_message_t *message, char **data,
                                  size_t *size);

#endif
