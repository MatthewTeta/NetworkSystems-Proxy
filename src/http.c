/**
 * @file http.h
 * @brief HTTP definitions and helpers
 * @author Matthew Teta (matthew.teta@coloradod.edu)
 * @version 0.1
 */

#include "http.h"

#include <stdlib.h>
#include <string.h>

/**
 * @brief Parse HTTP headers
 * @details Parses HTTP headers from a string. The string must include the
 * \r\n\r\n delimiter. If the function returns NULL, the headers were malformed
 * and the list is automatically freed.
 *
 * @param headers_str Headers string including \r\n\r\n delimiter
 * @return http_headers_t* Parsed headers
 */
http_headers_t *http_headers_parse(char *headers_str) {
    // TODO: Skip the first line (request line)
    http_headers_t *headers = malloc(sizeof(http_headers_t));
    headers->headers =
        malloc(sizeof(http_header_t *) * HTTP_HEADER_COUNT_DEFAULT);
    headers->size = HTTP_HEADER_COUNT_DEFAULT;
    char *line    = strtok(headers_str, "\r\n");
    while (line != NULL) {
        if (strcmp(line, "\r\n") == 0) {
            // End of headers
            break;
        }
        char *key = strtok(line, ":");
        char *val = strtok(NULL, ":");
        if (key == NULL || val == NULL) {
            // Malformed header
            DEBUG_PRINT("Malformed header: %s\n", line);
            http_headers_free(headers);
            return NULL;
        }
        http_headers_set(headers, key, val);
        line = strtok(NULL, "\r\n");
    }
}

/**
 * @brief Free HTTP headers
 *
 * @param headers Headers to free
 */
void http_headers_free(http_headers_t *headers) {
    if (headers == NULL) {
        return;
    }
    for (int i = 0; i < headers->count; i++) {
        free(headers->headers[i]->key);
        free(headers->headers[i]->value);
        free(headers->headers[i]);
    }
    free(headers->headers);
    free(headers);
}

/**
 * @brief Get a header value from a list of headers
 *
 * @param headers Headers
 * @param key Header key
 * @return char* Header value
 */
char *http_headers_get(http_headers_t *headers, char *key) {
    // TODO: Use a hash table for faster lookup
    // TODO: Convert to lowercase for case-insensitive comparison
    for (int i = 0; i < headers->count; i++) {
        if (strcmp(headers->headers[i]->key, key) == 0) {
            return headers->headers[i]->value;
        }
    }
    return NULL;
}

/**
 * @brief Set a header value in a list of headers
 *
 * @param headers Headers
 * @param key Header key
 * @param value Header value
 */
void http_headers_set(http_headers_t *headers, char *key, char *value) {
    // TODO: Use a hash table for faster lookup
    // TODO: Convert to lowercase for case-insensitive comparison
    // First search for the header
    for (int i = 0; i < headers->count; i++) {
        if (strcmp(headers->headers[i]->key, key) == 0) {
            // Header already exists, just update the value
            free(headers->headers[i]->value)
            headers->headers[i]->value = strdup(value);
            return;
        }
    }
    // Header does not exist, add it
    // Realloc for additional headers
    if (headers->count >= headers->size) {
        headers->size *= 2;
        headers->headers =
            realloc(headers->headers, sizeof(http_header_t *) * headers->size);
    }
    http_header_t *header = headers->headers[n] = malloc(sizeof(http_header_t));
    header->key                                 = strdup(key);
    header->value                               = strdup(val);
    headers->count++;
}
