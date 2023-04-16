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

#define HTTP_HEADER_COUNT_DEFAULT 16

/**
 * @brief HTTP header structure
 *
 */
typedef struct http_header {
    char *key;   // Header key
    char *value; // Header value
} http_header_t;

/**
 * @brief HTTP headers structure
 *
 */
typedef struct http_headers {
    int             size;    // Size of array
    int             count;   // Number of headers in array
    http_header_t **headers; // Array of headers
} http_headers_t;

/**
 * @brief Parse HTTP headers
 *
 * @param headers_str Headers string
 * @return http_headers_t* Parsed headers
 */
http_headers_t *http_headers_parse(char *headers_str);

/**
 * @brief Free HTTP headers
 *
 * @param headers Headers to free
 */
void http_headers_free(http_headers_t *headers);

/**
 * @brief Get a header value from a list of headers
 * 
 * @param headers Headers
 * @param key Header key
 * @return char* Header value
 */
char *http_headers_get(http_headers_t *headers, char *key);

/**
 * @brief Set a header value in a list of headers
 * 
 * @param headers Headers
 * @param key Header key
 * @param value Header value
 */
void http_headers_set(http_headers_t *headers, char *key, char *value);

#endif
