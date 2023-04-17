/**
 * @file pid_list.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * 
 * @brief Implementation of pid_list.h
 * 
 * @version 0.1
 * @date 2023-04-14
*/

#include "pid_list.h"

#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "debug.h"

pid_list_t *pid_list_create(pid_t pid) {
    pid_list_t *list = malloc(sizeof(pid_list_t));
    list->pid        = pid;
    list->next       = NULL;
    return list;
}

void pid_list_append(pid_list_t *list, pid_t pid) {
    if (list == NULL) {
        DEBUG_PRINT("Cannot append to NULL list\n");
        return;
    }
    pid_list_t *new_node = pid_list_create(pid);
    pid_list_t *current  = list;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = new_node;
}

pid_list_t *pid_list_remove(pid_list_t *list, pid_t pid) {
    if (list == NULL) {
        DEBUG_PRINT("Cannot remove from NULL list\n");
        return list;
    }
    pid_list_t *current = list;
    pid_list_t *prev    = NULL;
    while (current != NULL) {
        if (current->pid == pid) {
            if (prev == NULL) {
                // Removing the head
                DEBUG_PRINT("Removing head node\n");
                pid_list_t *next = current->next;
                free(current);
                current = next;
                return current;
            } else {
                // Removing a node in the middle
                DEBUG_PRINT("Removing node in the middle\n");
                prev->next = current->next;
                free(current);
                return list;
            }
        } else {
            prev    = current;
            current = current->next;
        }
    }
    return list;
}

void pid_list_free(pid_list_t *list) {
    pid_list_t *current = list;
    while (current != NULL) {
        pid_list_t *next = current->next;
        free(current);
        current = next;
    }
}

void pid_list_print(pid_list_t *list) {
    printf("CHILD PROCESSES: ");
    pid_list_t *current = list;
    while (current != NULL) {
        printf("%d ", current->pid);
        current = current->next;
    }
    printf("\n");
}

void pid_list_reap(pid_list_t *list) {
    pid_list_t *current = list;
    while (current != NULL) {
        int status;
        pid_t pid = waitpid(current->pid, &status, WNOHANG);
        if (pid == -1) {
            perror("waitpid");
        } else if (pid == 0) {
            // Child process is still running
        } else {
            // Child process has exited
            DEBUG_PRINT("Child process %d has exited\n", pid);
            list = pid_list_remove(list, pid);
        }
        current = current->next;
    }
}
