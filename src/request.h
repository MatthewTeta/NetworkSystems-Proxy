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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"

/**
 * @brief Request structure
 *
 */
typedef struct request {
    FILE           *request_fp; // Request string stream
    char           *host;        // Request host
    char           *method;      // Request method
    char           *uri;         // Request URI
    char           *version;     // Request version
    http_headers_t *headers;     // Request headers
    char           *body;        // Request body
} request_t;

/**
 * @brief Parse a request
 *
 * @param request_fp Request string
 * @return request_t* Parsed request
 */
request_t *request_parse(FILE *request_fp);

/**
 * @brief Free a request
 *
 * @param request Request to free
 */
void request_free(request_t *request);

/**
 * @brief Get a key to hash the request on. Return NULL if the request is not
 * cacheable.
 *
 * @param request
 * @param key
 */
void request_get_key(request_t *request, char *key);

#endif
