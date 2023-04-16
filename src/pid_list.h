/**
 * @file pid_list.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Header file for pid_list.c
 *
 * @version 0.1
 */

#ifndef PID_LIST_H
#define PID_LIST_H

#include <sys/types.h>

// Linked list vector implementation for storing child processes pids
typedef struct pid_list {
    pid_t            pid;
    struct pid_list *next;
} pid_list_t;

/**
 * @brief Create a new pid_list_t
 */
pid_list_t *pid_list_create(pid_t pid);

/**
 * @brief Append a pid to the list
 *
 * @param list List to append to
 * @param pid Pid to append
 */
void pid_list_append(pid_list_t *list, pid_t pid);

/**
 * @brief Remove a pid from the list
 *
 * @param list List to remove from
 * @param pid Pid to remove
 */
pid_list_t *pid_list_remove(pid_list_t *list, pid_t pid);

/**
 * @brief Free the list
 *
 * @param list List to free
 */
void pid_list_free(pid_list_t *list);

/**
 * @brief Print the list
 *
 * @param list List to print
 */
void pid_list_print(pid_list_t *list);

/**
 * @brief Reap the list
 *
 * @param list List to reap
 */
void pid_list_reap(pid_list_t *list);

#endif