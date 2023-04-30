/**
 * @file response.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef RESPONSE_H
#define RESPONSE_H

#include "http.h"
#include "request.h"

#define RESPONSE_STATUS_REGEX "(HTTP/[0-9]+\\.[0-9]+)?\\s+([0-9]+)\\s+(.*)"

/**
 * @brief Response structure
 *
 */
typedef struct response {
    http_message_t *message;     // HTTP message
    char           *version;     // Response version
    char           *reason;      // Response reason
    int             status_code; // Response status
} response_t;

/**
 * @brief Send the request to the server and get the response
 *
 * @param request Request to send
 * @return response_t* Response from server
 */
response_t *response_fetch(request_t *request);

/**
 * @brief Create a response
 *
 * @param status_code Status code
 * @param reason Reason
 * @return response_t* Response
 */
response_t *response_create(int status_code, char *reason);

/**
 * @brief Set the response body
 *
 * @param response Response to set body of
 * @param body Body to set
 * @param len Length of body
 *
 * @return int 0 on success, -1 on failure
 */
int response_set_body(response_t *response, char *body, size_t len);

/**
 * @brief Set the response body
 *
 * @param response Response to set body of
 * @param f File to read from
 *
 * @return int 0 on success, -1 on failure
 */
int response_set_body_f(response_t *response, FILE *f);

/**
 * @brief Send the response to the client
 *
 * @param response Response to send
 * @param connection Connection
 * @return int 0 on success, -1 on failure
 */
int response_send(response_t *response, connection_t *connection);

/**
 * @brief Free a response
 *
 * @param response Response to free
 */
void response_free(response_t *response);

/**
 * @brief Parse a response
 *
 * @param request Request to parse
 * @return int 0 on success, -1 on failure
 */
int response_header_parse(response_t *response);

/**
 * @brief Create a response from a message
 *
 * @param message Message
 *
 * @return response_t* Response
 */
response_t *response_parse(http_message_t *message);

/**
 * @brief Send an error response
 *
 * @param connection Connection to send response on
 * @param status_code Status code
 * @param reason Reason
 *
 * @return int 0 on success, -1 on failure
 */
int response_send_error(connection_t *connection, int status_code,
                        char *reason);

/**
 * @brief Write a response to a file
 *
 * @param response Response to write
 * @param f File to write to
 * @return int 0 on success, -1 on failure
 */
int response_write(response_t *response, FILE *f);

/**
 * @brief Read a response from a file
 *
 * @param f File to read from
 * @return response_t* Response
 */
response_t *response_read(FILE *f);

#endif
