#include "link.h"

SessionList* list_create(){
    SessionList *list = calloc(1, sizeof(*list));
    return list;
}

#define list_foreach(temp_node) for(; temp_node; temp_node = temp_node->next)

int checkIfFileExists(const char * filename) {
    FILE *file;
    if (file = fopen(filename, "r")) {
        fclose(file);
        return 1;
    }
    return 0;
}

int list_add(SessionList* list, struct sockaddr_in client_id, char const* filename) {
    Node *tmp = list->_first;
    list_foreach(tmp) {
        if(tmp->session.client_id == client_id)
            return -1;
        if(strcmp(tmp.session.filename, filename) == 0)
            return -2; // act as if the file already exists
    }
    // ^ not a live session
    // legal file:
    if (checkIfFileExists(filename))
        return -2;
    // all good, add:
    // init:
    tmp = malloc(sizeof(*tmp)); // fast for the buffer
    if(!tmp)
        PEXIT();
    tmp->session.client_id = client_id;
    tmp->session.last_block_number = 0;
    tmp->session.num_of_fails = 0;
    strcpy(tmp->session.filename, filename);
    // add:
    tmp->next = list->_first;
    list->_first = tmp;
    return 0;
}

int list_close(SessionList* list, struct sockaddr_in client_id, int save) {
    Node *node, *prev;
    node = list->_first;
    while(node) {
        if(node->session.client_id == client_id) {
            if(!save) {
                if(remove(node->session.filename) != 0)
                    PEXIT();
            }
            prev->next = node->next;
            free(node);
            return 0;
        }
        prev = node;
        node = node->next;
    }
    return -1;
}

Session* list_get(SessionList* list, struct sockaddr_in client_id) {
    Node *tmp = list->_first;
    list_foreach(tmp) {
        if(tmp->session.client_id == client_id)
            return &(tmp->session);
    }
    return NULL;
}

int session_add_data(Session* session, char const* buffer, int length) { // return block_number
    FILE* f = fopen(session->filename, "a");
    if(!f)
        PEXIT();
    if(fwrite(buffer, sizeof(char), length, f) != length)
        PEXIT();
    fclose(f);
    return ++(session->last_block_number);
}

void node_destroy(Node *node) {
    if(!node)
        return;
    if(node->next)
        node_destroy(node->next)
    free(node);
}

void list_destroy(SessionList* list) {
    if(!list)
        return;
    node_destroy(list->_first);
    free(list);
}
