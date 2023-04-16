/**
 * @file blocklist.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 *
 * @brief Implementation of blocklist.h
 *
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "blocklist.h"

#include <stdio.h>
#include <stdlib.h>

// Struct definitions
struct blocklist {
    int    size;  // Size of array
    int    count; // Number of entries in array
    char **list;  // Array of entries
};

// Private function prototypes
void blocklist_test_to_ip(const char *test, char *ip, size_t ip_len);

// Global variables
int     regex_initialized = 0;
regex_t ip_regex, host_regex;

/**
 * @brief Create a new blocklist
 *
 * @return blocklist_t* New blocklist
 */
blocklist_t *blocklist_init(const char *filepath) {
    blocklist_t *list = malloc(sizeof(blocklist_t));
    list->size        = BLOCKLIST_SIZE_DEFAULT;
    list->count       = 0;
    list->list        = malloc(sizeof(blocklist_t *) * list->size);

    // Compile the regexs
    if (!regex_initialized) {
        regex_initialized = 1;
        regcomp(&ip_regex, BLOCKLIST_IP_REGEX, REG_EXTENDED);
        regcomp(&host_regex, BLOCKLIST_HOST_REGEX, REG_EXTENDED);
        int status;
        status = regcomp(&ip_regex, BLOCKLIST_IP_REGEX, REG_EXTENDED);
        if (status) {
            DEBUG_PRINT("Could not compile ip regex\n");
            blocklist_free(list);
            return NULL;
        }
        status = regcomp(&host_regex, BLOCKLIST_HOST_REGEX, REG_EXTENDED);
        if (status) {
            DEBUG_PRINT("Could not compile host regex\n");
            blocklist_free(list);
            return NULL;
        }
    }

    // Open the blocklist file
    FILE *fp = fopen(filepath, "r");
    if (fp == NULL) {
        DEBUG_PRINT("Could not open blocklist file\n");
        blocklist_free(list);
        return NULL;
    }
    // Read each line of the file
    char  *line = NULL;
    size_t len  = 0;
    while (getline(&line, &len, fp) != -1) {
        // Remove the newline
        line[strlen(line) - 1] = '\0';
        // Add the entry to the blocklist
        blocklist_add(list, line);
    }

    return list;
}

/**
 * @brief Free a blocklist
 *
 * @param list Blocklist to free
 */
void blocklist_free(blocklist_t *list) {
    if (list == NULL) {
        return;
    }
    for (int i = 0; i < list->count; i++) {
        free(list->list[i]);
    }
    free(list->list);
    free(list);
}

/**
 * @brief Add an entry to the blocklist
 *
 * @param blocklist Blocklist
 * @param test Entry to add
 *
 * @return int 0 on success, -1 on failure
 */
int blocklist_add(blocklist_t *blocklist, const char *test) {
    char *new_ip = malloc(INET_ADDRSTRLEN);
    blocklist_test_to_ip(test, new_ip, INET_ADDRSTRLEN);
    if (strlen(new_ip) == 0) {
        DEBUG_PRINT("Could not convert %s to an IP\n", test);
        free(new_ip);
        return -1;
    }
    // Add the entry to the blocklist
    printf("INFO: Adding %s to blocklist\n", test);

    // Check if the blocklist is full
    if (list->count == list->size) {
        // Double the size of the array
        list->size *= 2;
        list->list = realloc(list->list, sizeof(blocklist_t *) * list->size);
    }

    // Add the new ip to the blocklist
    list->list[list->count] = new_ip;
    list->count++;

    return 0;
}

/**
 * @brief Check if an entry is in the blocklist
 *
 * @param blocklist Blocklist
 * @param test Entry to check
 *
 * @return int 0 if not in blocklist, 1 if in blocklist
 */
int blocklist_check(blocklist_t *blocklist, char *test) {
    char *new_ip[INET_ADDRSTRLEN];
    blocklist_test_to_ip(test, new_ip, INET_ADDRSTRLEN);
    if (strlen(new_ip) == 0) {
        DEBUG_PRINT("Could not convert %s to an IP\n", test);
        return 0;
    }
    // Check if the entry is in the blocklist
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->list[i]->value, new_ip) == 0) {
            return 1;
        }
    }
    return 0;
}

// Private functions

/**
 * @brief Convert a blocklist test to an IP if it is a hostname
 *
 * @param test Blocklist test
 * @param ipstr IP string (output)
 * @param ipstr_len Length of ipstr
 */
void blocklist_test_to_ip(const char *test, char *ipstr, size_t ipstr_len) {
    memset(ipstr, 0, ipstr_len);
    // Check if the test matches the IP regex
    if (regexec(&ip_regex, test, 0, NULL, 0) == 0) {
        // Move the test to the ipstr
        strncpy(ipstr, test, ipstr_len);
    } else if (regexec(&host_regex, test, 0, NULL, 0) == 0) {
        // Perform a DNS lookup
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        int status        = getaddrinfo(test, NULL, &hints, &res);
        if (status != 0) {
            DEBUG_PRINT("Could not resolve host: %s\n", test);
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
        // Move the test to the ipstr
        strncpy(ipstr, test, ipstr_len);
    } else {
        fprintf(stderr, "WARNING: Invalid blocklist entry: %s\n", test);
    }
}
