#include "link.h"

// -- define protocol packet structure: --

enum opcode {
    OP_WRQ  = 2,
    OP_DATA = 3,
    OP_ACK  = 4,
    OP_ERR  = 5
};

struct data_payload {
    short int block_number;
    char data[MAX_PACKET_LENGTH-4]; // get true size from ip header
} __attribute__((packed));

struct wrq_payload {
    char filename[MAX_PACKET_LENGTH-2];
} __attribute__((packed));

struct ack_payload {
    short int block_number;
    char rsvd[MAX_PACKET_LENGTH-4]; // nothing
} __attribute__((packed));

struct err_payload {
    short int error_code;
    char msg[MAX_PACKET_LENGTH-4];
} __attribute__((packed));


union payload {
    struct data_payload data;
    struct wrq_payload  wrq;
    struct ack_payload  ack;
    struct err_payload  err;
};

struct packet {
    short int opcode; // dont forget converting
    union payload payload;
} __attribute__((packed));


// -- helper functions: --
int legal_name(char const* name) {
    if(name[0] == '\0')
        return 0;
    for(char const* ch = name; ch[0] != '\0'; ch++)
        if(ch[0] == '/')
            return 0;
    return 1;
}
int is_octet(char const *data, int length) { // 0=good
    char const *ch;
    if(data[length-1] != '\0')
        return -1;
    for(ch=data; *ch != '\0'; ch++);
    ch++;
    return strcmp(ch, "octet");
}

int do_select_pexit(int sock, int secs) {
    int res = do_select(sock, secs);
    if(res < 0)
        PEXIT();
    return res;
}

void build_ack(short int block_num, struct packet *result, int *sendMsgSize) {
    LOG("ack %d\n", block_num);
    result->opcode = htons(OP_ACK);
    result->payload.ack.block_number = htons(block_num);
    *sendMsgSize = 4;
}

void build_err(short int error_code, char const* msg, struct packet *result, int *sendMsgSize) {
    LOG("err %d %s\n", error_code, msg);
    result->opcode = htons(OP_ERR);
    result->payload.err.error_code = htons(error_code);
    strcpy(result->payload.err.msg, msg);
    *sendMsgSize = htons(4 + strlen(msg) + 1);
}

// -- main: --

int main(int argc, char const *argv[]) {
    int WAIT_FOR_PACKET_TIMEOUT; // TODO from argv
    int NUMBER_OF_FAILURES;
    short int SERVER_PORT;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr; // dont bother with converting formats, unique anyway
    unsigned int client_addr_len;
    struct packet packet; // replaces: char buffer[MAX_PACKET_LENGTH];
    struct packet result;
    int recvMsgSize;
    int sendMsgSize; // 4 for ack, dynamic for errors
    int sock;
    time_t now;

    SessionList *list;
    Session *session;
    Node *node, *prev; // for timeout execution
    int status;

    // input
    if(argc != 4) {
        printf("TTFTP_ERROR: illegal arguments\n");
        exit(1);
    }
    SERVER_PORT = atoi(argv[1]);
    WAIT_FOR_PACKET_TIMEOUT = atoi(argv[2]);
    NUMBER_OF_FAILURES = atoi(argv[3]);
    // init
    list = list_create();
    if(!list)
        PEXIT();
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        list_destroy(list);
        PEXIT();
    }
    LOG("initialized\n");
    // config
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);
    LOG("configured\n");
    if (bind(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        PERR();
        list_destroy(list);
        close(sock);
        exit(1);
    }
    LOG("binded\n");

    // run:
    for(;;) {
        client_addr_len = sizeof(client_addr); // reset address length
        // receive with timeout (else timeout all sessions):
        if (do_select_pexit(sock, 1) > 0) {
            if ((recvMsgSize = recvfrom(sock, &packet, MAX_PACKET_LENGTH, 0, (struct sockaddr *) &client_addr, &client_addr_len)) < 0) {
                PEXIT();
            }
            LOG("recived\n");
            switch (ntohs(packet.opcode)) {
                case OP_WRQ:
                    LOG("WRQ\n");
                    if(is_octet(packet.payload.wrq.filename, recvMsgSize-2) != 0 || ! legal_name(packet.payload.wrq.filename)) {
                        // TODO: terminate ongoing related sessions? I dont think so, not from wrq
                        build_err(4, "Illegal WRQ", &result, &sendMsgSize);
                    } else {
                        status = list_add(list, client_addr, packet.payload.wrq.filename);
                        LOG("status = %d\n", status);
                        if(status == 0) {
                            build_ack(0, &result, &sendMsgSize);
                        } else {
                            if(status == -1) {
                                build_err(4, "Unexpected packet", &result, &sendMsgSize);
                            } else { // == -2
                                build_err(6, "File already exists", &result, &sendMsgSize);
                            }
                            // TODO: if session exists, abort? see above
                        }
                    }
                    break;

                case OP_DATA:
                    LOG("DATA\n");
                    session = list_get(list, client_addr);
                    if(!session) {
                        build_err(7, "Unknown user", &result, &sendMsgSize);
                    } else if( ntohs(packet.payload.data.block_number) != (session->last_block_number +1) ) { // TODO: maybe allow same block_number and just ack without writing? - no need according to forum
                        build_err(0, "Bad block number", &result, &sendMsgSize); // ABORT!
                        list_close(list, client_addr, 0);
                    } else {
                         session_add_data(session, packet.payload.data.data, recvMsgSize-4);
                         if(recvMsgSize-4 < 512) {
                             list_close(list, client_addr, 1);
                             LOG("Saved\n");
                         }
                         build_ack(ntohs(packet.payload.data.block_number), &result, &sendMsgSize);
                    }
                    break;

                default:
                    build_err(4, "Illegal TFTP operation", &result, &sendMsgSize);
                    break;
            } // end of switch-case
            // send result:
            if (sendto(sock, &result, sendMsgSize, 0, (struct sockaddr *) &client_addr, sizeof(client_addr)) != sendMsgSize) {
                PEXIT();
            }
        } // end of selected

        // -- check timeout for every live session: --
        now = time(NULL);
        node = list->_first;
        prev = NULL;
        while(node) {
            status = 0;
            session = &(node->session);
            client_addr = session->client_id; // already in network form
            //client_addr_len = sizeof(client_addr);

            if(difftime(now, session->changed) > WAIT_FOR_PACKET_TIMEOUT) {
                // timed out:
                LOG("timed out: %s\n", session->filename);
                session->num_of_fails++;
                if(session->num_of_fails > NUMBER_OF_FAILURES) { // ugly implementation
                    LOG("terminate: %s\n", session->filename);
                    status = -1;
                    session_close(session, 0);
                    if(prev)
                        prev->next = node->next;
                    else
                        list->_first = node->next;
                    free(node);
                    node = prev ? prev : list->_first;
                    build_err(0, "Abandoning file transmission", &result, &sendMsgSize);
                } else {
                    build_ack(session->last_block_number, &result, &sendMsgSize);
                }
                if (sendto(sock, &result, sendMsgSize, 0, (struct sockaddr *) &client_addr, sizeof(client_addr)) != sendMsgSize) {
                    PEXIT();
                }
                if(status<0) {
                    // TODO: list_destroy(list); close(sock); exit(1); // ?
                }
            }
            prev = node;
            node = node->next;
        }
    } // end of infinite loop

}
