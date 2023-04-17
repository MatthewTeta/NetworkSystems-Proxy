/**
 * @file blocklist.h
 * @brief Blocklist is a list of blocked IP addresses read in from a file.
 * The blocklist will allow for either hostnames or IP addresses.
 * @author Matthew Teta (matthewtetadev@gmail.com)
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef BLOCKLIST_H
#define BLOCKLIST_H

#define BLOCKLIST_SIZE_DEFAULT 1024

typedef struct blocklist blocklist_t;

/**
 * @brief Initialize the blocklist from a file.
 *
 * @param filepath The path to the file containing the blocklist.
 * @return blocklist_t* The blocklist. NULL if the file could not be opened or
 * is malformed.
 */
blocklist_t *blocklist_init(const char *filepath);

/**
 * @brief Free the blocklist.
 *
 * @param blocklist The blocklist to free.
 */
void blocklist_free(blocklist_t *blocklist);

/**
 * @brief Add an address to the blocklist.
 *
 * @param clocklost The blocklist to operate on
 * @param test The address to add.
 * @return int 0 if the address was added successfully, 1 if it was not.
 */
int blocklist_add(blocklist_t *blocklist, const char *test);

/**
 * @brief Check if an IP address is blocked.
 *
 * @param clocklost The blocklist to operate on
 * @param test Either an IP address or hostname to check.
 * @return int 1 if the IP is blocked, 0 if it is not.
 */
int blocklist_check(blocklist_t *blocklist, const char *test);

#endif
