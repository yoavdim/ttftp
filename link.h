#ifndef __LINK_H__
#define __LINK_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include "do_select.h"

#define MAX_PACKET_LENGTH 1500

#define LOG(...) do { /*fprintf(stderr, __VA_ARGS__)*/ } while(0)
// only for syscall errors:
#define PERR()  do { LOG("%d %s: ", __LINE__, __func__); perror("TTFTP_ERROR"); } while(0)
#define PEXIT() do { PERR(); exit(1); } while(0)

// maintain the state of each session
typedef struct {
    struct sockaddr_in client_id;
    short int last_block_number;
    int num_of_fails;
    time_t changed;
    char filename[MAX_PACKET_LENGTH-2]; // keeping the fd will limit the num of active sessions
} Session;

// recycle the linked list from the old wets
typedef struct acntnode {
    struct acntnode *next;
    Session session;
} Node;

typedef struct {
    Node *_first;
} SessionList;

#define list_foreach(temp_node) for(; temp_node; temp_node = temp_node->next)

SessionList* list_create();
void list_destroy(SessionList* list);
int list_add(SessionList* list, struct sockaddr_in client_id, char const* filename); // fail on exists or file exists
int list_close(SessionList* list, struct sockaddr_in client_id, int save); // fail not exists
Session* list_get(SessionList* list, struct sockaddr_in client_id);
int session_add_data(Session* session, char const* buffer, int length, int sock, SessionList *list);
void session_close(Session *session, int save);
int addr_cmp(struct sockaddr_in a, struct sockaddr_in b);
#endif
