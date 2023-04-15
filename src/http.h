/**
 * @file http.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief 
 * @version 0.1
 * @date 2023-04-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef HTTP_H
#define HTTP_H

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
    http_header_t **headers; // Array of headers
    int             size;    // Number of headers
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

#endif
