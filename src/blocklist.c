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

#include <arpa/inet.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "IP.h"
#include "debug.h"

// Struct definitions
struct blocklist {
    int    size;  // Size of array
    int    count; // Number of entries in array
    char **list;  // Array of entries
};

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
    hostname_to_ip(test, new_ip, INET_ADDRSTRLEN);
    if (strlen(new_ip) == 0) {
        DEBUG_PRINT("Could not convert %s to an IP\n", test);
        free(new_ip);
        return -1;
    }
    // Add the entry to the blocklist
    printf("INFO: Adding %s:%s to blocklist\n", test, new_ip);

    // Check if the blocklist is full
    if (blocklist->count == blocklist->size) {
        // Double the size of the array
        blocklist->size *= 2;
        blocklist->list =
            realloc(blocklist->list, sizeof(blocklist_t *) * blocklist->size);
    }

    // Add the new ip to the blockblocklist
    blocklist->list[blocklist->count] = new_ip;
    blocklist->count++;

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
int blocklist_check(blocklist_t *blocklist, const char *test) {
    char new_ip[INET_ADDRSTRLEN];
    hostname_to_ip(test, new_ip, INET_ADDRSTRLEN);
    if (strlen(new_ip) == 0) {
        DEBUG_PRINT("Could not convert %s to an IP\n", test);
        return 0;
    }
    // Check if the entry is in the blocklist
    for (int i = 0; i < blocklist->count; i++) {
        if (strcmp(blocklist->list[i], new_ip) == 0) {
            return 1;
        }
    }
    return 0;
}
