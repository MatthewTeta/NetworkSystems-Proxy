/**
 * @file request.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 *
 * @brief HTTP Request socket handler and parser
 * @date 2023-04-14
 */

#include "request.h"

#include <ctype.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#include "response.h"

// Private function prototypes
int        request_header_parse(request_t *request);
request_t *request_new();

request_t *request_recv(connection_t *connection) {
    request_t *request = request_new();
    request->message   = http_message_recv(connection);
    // fprintf(stderr, "http_message_recv complete\n");
    if (request->message == NULL) {
        request_free(request);
        return NULL;
    }

    // Parse the request line
    int status = request_header_parse(request);
    // fprintf(stderr, "request_header_parse complete (%d)\n", status);
    if (status != 0) {
        request_free(request);
        return NULL;
    }

    fprintf(stderr, "<-- %s %s %s\n", request->method, request->uri,
                request->version);

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
    if (request->query != NULL) {
        sprintf(request_line, "%s %s?%s %s\r\n", request->method, request->uri,
                request->query, request->version);
    } else {
        sprintf(request_line, "%s %s %s\r\n", request->method, request->uri,
                request->version);
    }
    // sprintf(request_line, "%s %s %s\r\n", request->method, request->uri,
    // request->version); fprintf(stderr, "REQUEST_LINE: %s\n", request_line);
    http_message_set_header_line(request->message, request_line);
    // Prepare the host header
    char host[1024];
    memset(host, 0, 1024);
    // if (request->port != -1) {
    //     if (request->https != -1) {
    //         if (request->https == 1) {
    //             sprintf(host, "https://%s:%d", request->host, request->port);
    //         } else {
    //             sprintf(host, "http://%s:%d", request->host, request->port);
    //         }
    //     } else {
    //         sprintf(host, "%s:%d", request->host, request->port);
    //     }
    // } else {
    //     sprintf(host, "%s", request->host);
    // }
    // if (request->https != -1) {
    //     strcat(host, request->https ? "https://" : "http://");
    // }
    strcat(host, request->host);
    if (request->port != -1) {
        char port[10];
        memset(port, 0, 10);
        sprintf(port, ":%d", request->port);
        strcat(host, port);
    }
    // strcat(host, request->uri);
    // if (request->query != NULL) {
    //     strcat(host, "?");
    //     strcat(host, request->query);
    // }
    fprintf(stderr, "HOST: %s\n", host);

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
    regex_t req_reg;
    // Parse the request line using regex
    int reg_status = regcomp(&req_reg, REQUEST_REGEX, REG_EXTENDED);
    if (reg_status != 0) {
        fprintf(stderr, "Error compiling regex.\n");
        return -1;
    }
    http_message_t *message = request->message;
    regmatch_t      uri_matches[REQUEST_REGEX_INDEX_COUNT];
    int             status      = 0;
    char           *header_line = http_message_get_header_line(message);
    fprintf(stderr, "HEADER_LINE: %s\n", header_line);
    if (header_line == NULL) {
        fprintf(stderr, "Error getting header line.\n");
        return -1;
    }
    // fprintf(stderr, "HEADER_LINE: %s\n", header_line);
    status = regexec(&req_reg, header_line, REQUEST_REGEX_INDEX_COUNT,
                     uri_matches, 0);
    regfree(&req_reg);
    if (status != 0) {
        char error_message[1024];
        regerror(status, &req_reg, error_message, 1024);
        fprintf(stderr, "Error parsing request line: %s\n", error_message);
        return -1;
    }
    // Get the method
    request->method =
        strndup(header_line + uri_matches[REQUEST_REGEX_INDEX_METHOD].rm_so,
                uri_matches[REQUEST_REGEX_INDEX_METHOD].rm_eo -
                    uri_matches[REQUEST_REGEX_INDEX_METHOD].rm_so);
    // Get http vs https
    if (uri_matches[REQUEST_REGEX_INDEX_PROTOCOL].rm_so != -1) {
        if (strncmp(header_line +
                        uri_matches[REQUEST_REGEX_INDEX_PROTOCOL].rm_so,
                    "https", 5) == 0) {
            request->https = 1;
        } else {
            request->https = 0;
        }
    } else {
        request->https = -1;
    }
    // Get the host
    if (uri_matches[REQUEST_REGEX_INDEX_HOSTNAME].rm_so != -1) {
        request->host = strndup(
            header_line + uri_matches[REQUEST_REGEX_INDEX_HOSTNAME].rm_so,
            uri_matches[REQUEST_REGEX_INDEX_HOSTNAME].rm_eo -
                uri_matches[REQUEST_REGEX_INDEX_HOSTNAME].rm_so);
    }
    // Get the port
    if (uri_matches[REQUEST_REGEX_INDEX_PORT].rm_so != -1) {
        char *port_str =
            strndup(header_line + uri_matches[REQUEST_REGEX_INDEX_PORT].rm_so,
                    uri_matches[REQUEST_REGEX_INDEX_PORT].rm_eo -
                        uri_matches[REQUEST_REGEX_INDEX_PORT].rm_so);
        request->port = atoi(port_str);
        free(port_str);
    } else {
        request->port = -1;
    }
    // Get the uri
    if (uri_matches[REQUEST_REGEX_INDEX_PATH].rm_so != -1) {
        request->uri =
            strndup(header_line + uri_matches[REQUEST_REGEX_INDEX_PATH].rm_so,
                    uri_matches[REQUEST_REGEX_INDEX_PATH].rm_eo -
                        uri_matches[REQUEST_REGEX_INDEX_PATH].rm_so);
    } else {
        request->uri = strdup("/");
    }
    // Get the query string
    if (uri_matches[REQUEST_REGEX_INDEX_QUERY].rm_so != -1) {
        request->query =
            strndup(header_line + uri_matches[REQUEST_REGEX_INDEX_QUERY].rm_so,
                    uri_matches[REQUEST_REGEX_INDEX_QUERY].rm_eo -
                        uri_matches[REQUEST_REGEX_INDEX_QUERY].rm_so);
    }
    // Get the http version
    if (uri_matches[REQUEST_REGEX_INDEX_VERSION].rm_so != -1) {
        request->version = strndup(
            header_line + uri_matches[REQUEST_REGEX_INDEX_VERSION].rm_so,
            uri_matches[REQUEST_REGEX_INDEX_VERSION].rm_eo -
                uri_matches[REQUEST_REGEX_INDEX_VERSION].rm_so);
    }

    // Get the Host header
    char *host = http_message_header_get(message, "Host");
    if (host != NULL) {
        request->host  = strndup(host, strlen(host));
        char *port_str = strchr(host, ':');
        if (port_str != NULL) {
            request->port = atoi(port_str + 1);
            *port_str     = '\0';
        }
    }

    fprintf(stderr, " -- METHOD: %s\n", request->method);
    fprintf(stderr, " -- HTTPS: %d\n", request->https);
    fprintf(stderr, " -- HOST: %s\n", request->host);
    fprintf(stderr, " -- PORT: %d\n", request->port);
    fprintf(stderr, " -- URI: %s\n", request->uri);
    fprintf(stderr, " -- QUERY: %s\n", request->query);
    fprintf(stderr, " -- VERSION: %s\n", request->version);

    return 0;
} // int request_header_parse(request_t *request) {
//     http_message_t *message = request->message;
//     // Parse the request line using regex
//     static int        regex_compiled = 0;
//     static regex_t    req_reg;
//     static regmatch_t uri_matches[7];
//     static int        r_status;
//     if (regex_compiled == 0) {
//         r_status = regcomp(&req_reg, REQUEST_URI_REGEX, REG_EXTENDED);
//         if (r_status != 0) {
//             fprintf(stderr, "Error compiling regex.\n");
//             return -1;
//         }
//         regex_compiled = 1;
//     }
//     int   status;
//     char *header_line = http_message_get_header_line(message);
//     if (header_line == NULL) {
//         fprintf(stderr, "Error getting header line.\n");
//         return -1;
//     }
//     fprintf(stderr, " -- HEADER: %s\n", header_line);
//     status = regexec(&req_reg, header_line, 8, uri_matches, 0);
//     if (status != 0) {
//         char error_message[1024];
//         regerror(status, &req_reg, error_message, 1024);
//         fprintf(stderr, "Error parsing request line: %s\n", error_message);
//         return -1;
//     }
//     // Get the method
//     if (uri_matches[1].rm_so != -1) {
//         request->method = strndup(header_line + uri_matches[1].rm_so,
//                                   uri_matches[1].rm_eo -
//                                   uri_matches[1].rm_so);
//     } else {
//         request->method = NULL;
//     }

//     // Get http vs https
//     if (uri_matches[2].rm_so != -1) {
//         if (strncmp(header_line + uri_matches[2].rm_so, "https", 5) == 0) {
//             request->https = 1;
//         }
//     // } else {
//     //     request->https = -1;
//     }

//     // Get the host
//     if (uri_matches[3].rm_so != -1) {
//         request->host = strndup(header_line + uri_matches[3].rm_so,
//                                 uri_matches[3].rm_eo - uri_matches[3].rm_so);
//     // } else {
//     //     request->host = strdup("");
//     }

//     // Get the port
//     if (uri_matches[5].rm_so != -1) {
//         char *port_str = strndup(header_line + uri_matches[5].rm_so,
//                                  uri_matches[5].rm_eo -
//                                  uri_matches[5].rm_so);
//         request->port  = atoi(port_str);
//         free(port_str);
//     } else {
//         request->port = -1;
//     }

//     // Get the uri
//     if (uri_matches[6].rm_so != -1) {
//         request->uri = strndup(header_line + uri_matches[6].rm_so,
//                                uri_matches[6].rm_eo - uri_matches[6].rm_so);
//     } else {
//         request->uri = strdup("/");
//     }
//     fprintf(stderr, " -- URI: %s\n", request->uri);

//     // Get the http version
//     if (uri_matches[7].rm_so != -1) {
//         request->version = strndup(header_line + uri_matches[7].rm_so,
//                                    uri_matches[7].rm_eo -
//                                    uri_matches[7].rm_so);
//     }
//     // } else {
//     //     request->version = strdup("HTTP/1.1");
//     // }

//     // Get the Host header
//     char *host_hdr = http_message_header_get(message, "Host");

//     // if (host_hdr != NULL) {
//     //     // If the host header is present, use it
//     //     int   https, port;
//     //     char *host = NULL;
//     //     char *uri  = NULL;
//     //     status     = http_parse_host(host_hdr, &host, &port, &uri,
//     &https);
//     //     if (status != 0) {
//     //         fprintf(stderr, "Error parsing host header.\n");
//     //     } else {
//     //         // Move the values to the request struct
//     //         if (https != -1) {
//     //             request->https = https;
//     //         }
//     //         if (port != -1) {
//     //             request->port = port;
//     //         }
//     //         if (host != NULL) {
//     //             if (request->host != NULL) {
//     //                 free(request->host);
//     //             }
//     //             request->host = host;
//     //         }
//     //         if (uri != NULL) {
//     //             if (strlen(uri) > strlen(request->uri)) {
//     //                 if (request->uri != NULL) {
//     //                     free(request->uri);
//     //                 }
//     //                 request->uri = uri;
//     //             } else {
//     //                 free(uri);
//     //             }
//     //         }
//     //     }
//     // }

//     if (host_hdr != NULL) {
//         char  *host      = strdup(host_hdr);
//         char **saveptr   = NULL;
//         char  *host_port = strtok_r(host, ":", saveptr);
//         if (host_port != NULL) {
//             if (request->host != NULL) {
//                 free(request->host);
//             }
//             request->host = strdup(host_port);
//             host_port     = strtok_r(NULL, ":", saveptr);
//             if (host_port != NULL) {
//                 request->port = atoi(host_port);
//             }
//         }
//         free(host);
//     }

//     if (request->port == -1) {
//         if (request->https == 1) {
//             request->port = 443;
//         } else {
//             request->port = 80;
//         }
//     }

//     fprintf(stderr, " -- METHOD: %s\n", request->method);
//     fprintf(stderr, " -- HTTPS: %d\n", request->https);
//     fprintf(stderr, " -- HOST: %s\n", request->host);
//     fprintf(stderr, " -- PORT: %d\n", request->port);
//     fprintf(stderr, " -- URI: %s\n", request->uri);
//     fprintf(stderr, " -- VERSION: %s\n", request->version);

//     // #ifdef DEBUG
//     //     // Print any whitespace in the request line
//     //     fprintf(stderr, " -- WHITESPACE: ");
//     //     char *w = request->host;
//     //     while (*w != '\0') {
//     //         if (isspace(*w)) {
//     //             fprintf(stderr, ".");
//     //         } else {
//     //             fprintf(stderr, "%c", *w);
//     //         }
//     //         w++;
//     //     }
//     //     fprintf(stderr, "\n");
//     // #endif

//     return 0;
// }

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
    request->https     = -1;
    request->port      = -1;
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
    // char *cache_control =
    //     http_message_header_get(request->message, "Cache-Control");
    // if (cache_control != NULL) {
    //     if (strcmp(cache_control, "no-cache") == 0) {
    //         fprintf(stderr, "Request is not cacheable because of Cache-Control\n");
    //         return 0;
    //     }
    // }
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
