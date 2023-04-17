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

#define RESPONSE_STATUS_REGEX "^(HTTP/[\\d\\.]+)?\\s+(\\d+)\\s+(.*)"

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
response_t *response_recv(request_t *request);

/**
 * @brief Parse a response
 *
 * @param response Response to parse
 * @return int 0 on success, -1 on failure
 */
int response_header_parse(response_t *response);

/**
 * @brief Free a response
 *
 * @param response Response to free
 */
void response_free(response_t *response);

/**
 * @brief Send a response to a client
 *
 * @param response Response to send
 * @param clientfd Client socket
 * @return int 0 on success, -1 on failure
 */
int response_send(response_t *response, connection_t *connection);

#endif
