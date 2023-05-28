/**
 * @file IP.c
 * @brief IP contains helper functions for working with IP addresses and
 * hostnames.
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 */

#include "IP.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>


// Global variables
regex_t ip_regex, host_regex;

/**
 * @brief Convert a hostname (or IP address) to an IP address
 *
 * @param host Host / IP string
 * @param ipstr IP string (output)
 * @param ipstr_len Length of ipstr buffer
 */
void hostname_to_ip(const char *hostname, char *ipstr, size_t ipstr_len) {
    unsigned char buf[sizeof(struct in6_addr)];
    char          str[INET6_ADDRSTRLEN];
    int           s;

    s = inet_pton(AF_INET, hostname, buf);
    if (s <= 0) {
        if (s == 0) {
            // Resolve hostname to IP address and return it
            struct addrinfo hints, *res;
            int             errcode;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family   = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            if ((errcode = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
                return;
            }

            struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
            void               *addr = &(ipv4->sin_addr);
            inet_ntop(res->ai_family, addr, str, sizeof(str));
            freeaddrinfo(res);
            strncpy(ipstr, str, ipstr_len);
            return;
        } else
            perror("inet_pton");
        return;
    }

    if (inet_ntop(AF_INET, buf, str, INET6_ADDRSTRLEN) == NULL) {
        perror("inet_ntop");
        return;
    }

    strncpy(ipstr, str, ipstr_len);
}
