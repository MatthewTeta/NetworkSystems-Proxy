/**
 * @file request.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 *
 * @brief HTTP Request socket handler and parser
 * @date 2023-04-14
 */

#include "request.h"

#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "response.h"

// Private function prototypes
int        request_header_parse(request_t *request);
request_t *request_new();

request_t *request_recv(connection_t *connection) {
    request_t *request = request_new();
    request->message   = http_message_recv(connection);
    // DEBUG_PRINT("http_message_recv complete\n");
    if (request->message == NULL) {
        request_free(request);
        return NULL;
    }

    // Parse the request line
    int status = request_header_parse(request);
    if (status != 0) {
        request_free(request);
        return NULL;
    }

    return request;
}

/**
 * @brief Send a request to the server. Prepares the http_message_t with the
 * header line, host header.
 *
 * @param request Request to send
 * @param connection Connection
 * @return int 0 on success, -1 on failure
 */
int request_send(request_t *request, connection_t *connection) {
    // Prepare the request line
    char request_line[1024];
    memset(request_line, 0, 1024);
    sprintf(request_line, "%s %s %s\r\n", request->method, request->uri,
            request->version);
    DEBUG_PRINT("REQUEST_LINE: %s\n", request_line);
    http_message_set_header_line(request->message, request_line);
    // Prepare the host header
    char host[1024];
    memset(host, 0, 1024);
    if (request->port != 0)
        sprintf(host, "%s:%d", request->host, request->port);
    else
        sprintf(host, "%s", request->host);
    http_message_header_set(request->message, "Host", host);
    // Send the request
    int status = http_message_send(request->message, connection);
    return status;
}

void request_free(request_t *request) {
    if (request == NULL) {
        return;
    }
    if (request->message != NULL) {
        http_message_free(request->message);
    }
    if (request->uri != NULL) {
        free(request->uri);
    }
    if (request->host != NULL) {
        free(request->host);
    }
    if (request->method != NULL) {
        free(request->method);
    }
    if (request->version != NULL) {
        free(request->version);
    }
    free(request);
}

// Private function definitions
int request_header_parse(request_t *request) {
    http_message_t *message = request->message;
    // Parse the request line using regex
    regex_t    uri_regex;
    regmatch_t uri_matches[7];
    int        status = regcomp(&uri_regex, REQUEST_URI_REGEX, REG_EXTENDED);
    if (status != 0) {
        DEBUG_PRINT("Error compiling regex.\n");
        return -1;
    }
    char *header_line = http_message_get_header_line(message);
    if (header_line == NULL) {
        DEBUG_PRINT("Error getting header line.\n");
        return -1;
    }
    // DEBUG_PRINT("HEADER_LINE: %s\n", header_line);
    status = regexec(&uri_regex, header_line, 8, uri_matches, 0);
    if (status != 0) {
        char error_message[1024];
        regerror(status, &uri_regex, error_message, 1024);
        DEBUG_PRINT("Error parsing request line: %s\n", error_message);
        return -1;
    }
    // Get the method
    request->method = strndup(header_line + uri_matches[1].rm_so,
                              uri_matches[1].rm_eo - uri_matches[1].rm_so);
    DEBUG_PRINT(" -- METHOD: %s\n", request->method);
    // Get http vs https
    if (uri_matches[2].rm_so != -1) {
        if (strncmp(header_line + uri_matches[2].rm_so, "https", 5) == 0) {
            request->https = 1;
        }
    }
    DEBUG_PRINT(" -- HTTPS: %d\n", request->https);
    // Get the host
    if (uri_matches[3].rm_so != -1) {
        request->host = strndup(header_line + uri_matches[3].rm_so,
                                uri_matches[3].rm_eo - uri_matches[3].rm_so);
    }
    DEBUG_PRINT(" -- HOST: %s\n", request->host);
    // Get the port
    if (uri_matches[5].rm_so != -1) {
        char *port_str = strndup(header_line + uri_matches[5].rm_so,
                                 uri_matches[5].rm_eo - uri_matches[5].rm_so);
        request->port  = atoi(port_str);
        free(port_str);
    } else {
        request->port = 80;
    }
    DEBUG_PRINT(" -- PORT: %d\n", request->port);
    // Get the uri
    if (uri_matches[6].rm_so != -1) {
        request->uri = strndup(header_line + uri_matches[6].rm_so,
                               uri_matches[6].rm_eo - uri_matches[6].rm_so);
    } else {
        request->uri = strdup("/");
    }
    DEBUG_PRINT(" -- URI: %s\n", request->uri);
    // Get the http version
    if (uri_matches[7].rm_so != -1) {
        request->version = strndup(header_line + uri_matches[7].rm_so,
                                   uri_matches[7].rm_eo - uri_matches[7].rm_so);
    }
    DEBUG_PRINT(" -- VERSION: %s\n", request->version);

    // Get the Host header
    char *host = http_message_header_get(message, "Host");
    if (host != NULL) {
        request->host = strndup(host, strlen(host));
    }

    return 0;
}

/**
 * Returns 1 if the request is a keep-alive connection, 0 otherwise.
 */
int request_is_connection_keep_alive(request_t *request) {
    http_message_t *message = request->message;
    // Check for the connection header
    if (http_message_header_get(message, "Connection") == NULL) {
        // // Set the connection header to close
        // http_message_header_set(message, "Connection", "close");
        return 0;
    } else {
        // Check if the connection header is keep-alive
        if (strcmp(http_message_header_get(message, "Connection"),
                   "keep-alive") == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Parse a request from a message
 *
 * @param message Message to parse
 * @return request_t* Request
 */
request_t *request_parse(http_message_t *message) {
    if (message == NULL) {
        return NULL;
    }
    request_t *request = request_new();
    request->message   = message;
    int status         = request_header_parse(request);
    if (status != 0) {
        request_free(request);
        return NULL;
    }
    return request;
}

// Private function definitions
request_t *request_new() {
    request_t *request = malloc(sizeof(request_t));
    request->message   = NULL;
    request->uri       = NULL;
    request->host      = NULL;
    request->method    = NULL;
    request->version   = NULL;
    request->https     = 0;
    request->port      = 0;
    return request;
}

/**
 * @brief Determine if a request is cacheable
 * 
 * @param request Request to check
 * @return int 1 if cacheable, 0 otherwise
*/
int request_is_cacheable(request_t *request) {
    // Check if the request is cacheable
    if (request->method == NULL || strcmp(request->method, "GET") != 0) {
        return 0;
    }
    if (request->version == NULL) {
        return 0;
    }
    if (request->host == NULL) {
        return 0;
    }
    if (request->uri == NULL) {
        return 0;
    }
    char *cache_control =
        http_message_header_get(request->message, "Cache-Control");
    if (cache_control != NULL) {
        if (strcmp(cache_control, "no-cache") == 0) {
            DEBUG_PRINT("Request is not cacheable because of Cache-Control\n");
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Get a key to hash the request on. Return NULL if the request is not
 * cacheable.
 *
 * @param request Request to hash
 * @param key Output key
 * @param len Length of the key
 */
void request_get_key(request_t *request, char *key, size_t len) {
    *key = '\0';
    if (request_is_cacheable(request)) {
        // Create the key
        snprintf(key, len, "%s%s", request->host, request->uri);
    }
}
