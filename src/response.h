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
 * @param version Version
 * @return response_t* Response
 */
response_t *response_create(int status_code, char *version);

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

#endif
