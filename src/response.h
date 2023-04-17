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

/**
 * @brief Response structure
 * 
 */
typedef struct response {
    char           *version; // Response version
    int             status;  // Response status
    char           *reason;  // Response reason
    http_headers_t *headers; // Response headers
    char           *body;    // Response body
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
 * @param response_str Response string
 * @return response_t* Parsed response
 */
response_t *response_parse(char *response_str);

/**
 * @brief Free a response
 * 
 * @param response Response to free
 */
void response_free(response_t *response);

#endif
