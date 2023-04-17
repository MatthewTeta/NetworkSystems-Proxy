/**
 * @file IP.c
 * @brief IP contains helper functions for working with IP addresses and hostnames.
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @version 0.1
 * @date 2023-04-14
 * 
 * @copyright Copyright (c) 2023
*/

#include "IP.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Convert a hostname (or IP address) to an IP address
 *
 * @param host Host / IP string
 * @param ipstr IP string (output)
 * @param ipstr_len Length of ipstr buffer
 */
void host_to_ip(const char *host, char *ipstr, size_t ipstr_len) {
    memset(ipstr, 0, ipstr_len);
    // Check if the host matches the IP regex
    if (regexec(&ip_regex, host, 0, NULL, 0) == 0) {
        // Move the host to the ipstr
        strncpy(ipstr, host, ipstr_len);
    } else if (regexec(&host_regex, host, 0, NULL, 0) == 0) {
        // Perform a DNS lookup
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        int status        = getaddrinfo(host, NULL, &hints, &res);
        if (status != 0) {
            DEBUG_PRINT("Could not resolve host: %s\n", host);
            return;
        }
        // Convert to a string
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        void               *addr = &(ipv4->sin_addr);
        if (inet_ntop(res->ai_family, addr, ipstr, ipstr_len) == NULL) {
            DEBUG_PRINT("Could not convert IP to string\n");
            return;
        }
        // Free the addrinfo
        freeaddrinfo(res);
        // Move the host to the ipstr
        strncpy(ipstr, host, ipstr_len);
    } else {
        fprintf(stderr, "WARNING: Invalid blocklist entry: %s\n", host);
    }
}
