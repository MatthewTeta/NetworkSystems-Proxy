/**
 * @file request.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 *
 * @brief HTTP Request socket handler and parser
 * @date 2023-04-14
 */

#include "request.h"

#include <regex.h>

#include "response.h"

// Private function prototypes
int request_header_parse(request_t *request);

request_t *request_recv(connection_t *connection) {
    request_t *request = malloc(sizeof(request_t));
    memset(request, 0, sizeof(request_t));
    request->message = http_message_recv(connection);
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
    status = regexec(&uri_regex, message, 7, uri_matches, 0);
    if (status != 0) {
        DEBUG_PRINT("Error parsing request line.\n");
        return -1;
    }
    // Get the method
    request->method = strndup(message + uri_matches[1].rm_so,
                              uri_matches[1].rm_eo - uri_matches[1].rm_so);
    // Get http vs https
    if (uri_matches[2].rm_so != -1) {
        if (strncmp(message + uri_matches[2].rm_so, "https", 5) == 0) {
            request->https = 1;
        }
    }
    // Get the host
    request->host = strndup(message + uri_matches[3].rm_so,
                            uri_matches[3].rm_eo - uri_matches[3].rm_so);
    // Get the port
    if (uri_matches[4].rm_so != -1) {
        request->port =
            strndup(message + uri_matches[4].rm_so + 1,
                    uri_matches[4].rm_eo - uri_matches[4].rm_so - 1);
    }
    // Get the uri
    request->uri = strndup(message + uri_matches[5].rm_so,
                           uri_matches[5].rm_eo - uri_matches[5].rm_so);
    // Get the http version
    request->version = strndup(message + uri_matches[6].rm_so,
                               uri_matches[6].rm_eo - uri_matches[6].rm_so);

    // Get the Host header
    char *host = http_headers_get(message->headers, "Host");
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
    if (http_headers_get(message, "Connection") == NULL) {
        // Set the connection header to close
        http_headers_set(message, "Connection", "close");
    } else {
        // Check if the connection header is keep-alive
        if (strcmp(http_headers_get(message, "Connection"), "keep-alive") ==
            0) {
            return 1;
        }
    }
    return 0;
}
