#include "message.h"
#include "message_db.h"

#define ACCEPT          5   //connection num that can accept
#define MAX_EVENTS      10
#define MAX_USER        100000    
#define BUFSIZE         MAX_PACKET_LEN
#define MSG_PARAMS_LEN  MAX_PACKET_LEN - PACKET_HEADER_LEN

static int listen_fd, status_fd, epfd;
static struct con_info *conns;
static struct msg_list *map[MAX_USER+1];

/* send get status request to status module */
static inline void request_status(int uin, struct packet *outpack)
{
    fill_packet_header(outpack, PACKET_HEADER_LEN + 4, CMD_GET_STATUS, uin);
    *(uint32_t *)outpack->params = uin;
    write(status_fd, outpack, outpack->len);
}

#ifndef _MODULE_
void message()
#else
void main()
#endif
{
    int  tmpfd, client_fd, size, nfds, i, nread, left;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in client_addr;
    struct packet *inpack, *outpack;

    msg_dbg("Message start: %d\n", getpid());

    inpack = malloc(MAX_PACKET_LEN);
    outpack = malloc(MAX_PACKET_LEN);

    memset(&client_addr, 0, sizeof(client_addr));
    size = sizeof(struct sockaddr_in);

    /* Init database connection */
    if (message_db_init()) {
        msg_err("database connect error\n"); 
        goto exit;
    }

    /* Init network connection */
    if (message_conn_init()) {
        msg_err("message connection init error\n"); 
        goto exit;
    }
    msg_dbg("==> Message listened, Status connected\n");

    for (;;) {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if (nfds == -1) {
            msg_err("***************** epoll_wait error ****************\n");
            goto exit;
        }

        for (i = 0; i < nfds; i++) {
            tmpfd = events[i].data.fd;
            /* new connection from other module */
            if (tmpfd == listen_fd) {
                client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &size);
                if (client_fd < 0) {
                    msg_err("accept error\n");
                    goto exit;
                }
                msg_dbg("client %s, port %d connected\n", inet_ntoa(client_addr.sin_addr), \
                        ntohs(client_addr.sin_port));

                /* map ip address to socket fd */
                add_con((uint32_t)client_addr.sin_addr.s_addr, client_fd);
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    msg_err("epoll_ctl: add error\n");
                    goto exit;
                }
                continue;
            }

            /* receive a packet from socket */
            if (events[i].events & EPOLLIN) {
                msg_dbg("<=======  A Packet Arrive! =======>\n");
                if (tmpfd < 0) {
                    msg_dbg("tmpfd(%d) < 0\n", tmpfd);
                    continue;
                }
                
                nread = packet_read(tmpfd, (char *)inpack, PACKET_HEADER_LEN, epfd);
                if (nread) {
                    /* delete ip address mapping because of disconnection*/
                    if (nread == -1)
                        del_con(tmpfd);
                    continue;
                } else {
                    msg_dbg("PACKET: len %d, cmd %04x, uin %d\n", inpack->len, inpack->cmd, \
                            inpack->uin);
                    left = inpack->len - PACKET_HEADER_LEN;
                    if (left > 0) {
                        msg_dbg("packet left %d bytes to read\n", left);
                        nread = packet_read(tmpfd, inpack->params, left, epfd);
                        if (nread) {
                            if (nread == -1)
                                del_con(tmpfd);
                            continue;
                        }
                    }
                    
                    /* message packet processing */
                    message_packet(inpack, outpack, tmpfd);
                }
            }
        }
    }

exit:
    msg_dbg("==> Message process is going to exit !\n");
    //free(inpack);
    //free(outpack);
    message_db_close();
}

/* create an ip address to socket fd mapping */
int add_con(uint32_t ip, int sockfd)
{
    struct con_info *new_con;

    new_con = malloc(sizeof(struct con_info));
    if (!new_con) {
        msg_err("alloc struct con_info error\n");
        return -1;
    }
    
    new_con->ip = ip;
    new_con->sockfd = sockfd;
    new_con->next = conns;
    conns = new_con;

    return 0;
}

/* delete an ip mapping */
void del_con(int sockfd)
{
    struct con_info *con, *next;

    if (conns->sockfd == sockfd)
        conns == conns->next;
    else {
        con = conns;
        next = con->next;
        while (next) {
            if (next->sockfd == sockfd) {
                con->next = next->next;
                break;
            } else {
                con = next;
                next = next->next;
            }
        }
    }
}

/* map the ip address to socket fd */
int ip_to_fd(uint32_t ip)
{
    struct con_info *con;

    con = conns;
    while (con) {
        if (con->ip == ip)
            return conns->sockfd;
        else
            con = con->next;
    }

    return -1;
}

/* store user offline message */
int store_offline_msg(int uin)
{
    struct msg_list *msg;

    msg_dbg("Store offline msg to uin(%d)\n", uin);
    msg = map[uin];
    while (msg) {
        if (message_store(msg->msg.from_uin, uin, msg->msg.type, \
                    (char *)msg->msg.msg_str)) {
            message_update(msg->msg.from_uin, uin, (char *)msg->msg.msg_str);
        }
        msg = msg->next;
    }
    map[uin] = NULL;

    return 0;
}

/* send message to the server that the user connected to */
int send_msg(int uin, uint32_t ip, struct packet *outpack)
{
    int sockfd;
    struct msg_list *msg;

    assert(outpack);
    msg_dbg("Sending message -->\n");
    msg = map[uin];
    if (msg) {
        sockfd = ip_to_fd(ip);
        msg_dbg("Send msg to uin(%d), ip(%d), fd(%d)\n", uin, ip, sockfd);
        if (sockfd < 0)
            return sockfd;
        outpack->ver = 1;
        outpack->cmd = SRV_MESSAGE;
        outpack->uin = uin;
        while(msg) {
            outpack->len = PACKET_HEADER_LEN + MSG_HEADER_LEN + msg->msg.len;
            memcpy(outpack->params, (void *)&msg->msg, MSG_HEADER_LEN + msg->msg.len);
            write(sockfd, outpack, outpack->len);
            msg_dbg("Sent message: %s\n", msg->msg.msg_str);
            msg = msg->next;
        }
    }
    map[uin] = NULL;

    return 0;
}

/* create a new message struct */
struct msg_list *new_msg(int uin, int timestamp, int type, int len, char *message)
{
    struct msg_list *amsg;

    assert(message);
    amsg = malloc(sizeof(struct msg_list) + len);
    if (!amsg) {
        msg_err("msg allocate error\n");
        return NULL;
    }

    amsg->msg.from_uin = uin;
    amsg->msg.timestamp = timestamp;
    amsg->msg.type = type;
    amsg->msg.len = len;
    strcpy((char *)amsg->msg.msg_str, message);

    return amsg;
}

/*
 * message_packet - process message packet and send back response packet
 * @inpack: received packet
 * @outpack: response packet
 * @fd: socket that receive packet
 */
int message_packet(struct packet *inpack, struct packet *outpack, int fd)
{
    int msgs_left, sizeleft, uin;
    struct msg_list *msg;

    assert(inpack && outpack);
    switch (inpack->cmd) {
    case CMD_OFFLINE_MSG: //offline message request
        sizeleft = MSG_PARAMS_LEN;
        msgs_left = message_get(inpack->uin, (char *)outpack->params, &sizeleft);
        msg_dbg("There are %d messages left, buffsize %d left\n", msgs_left, sizeleft);
        if (msgs_left < 0) 
            return -1;
        else {
            /* pack messages into one packet */
            while (msgs_left) {
                fill_packet_header(outpack, PACKET_HEADER_LEN + MSG_PARAMS_LEN - sizeleft,\
                        SRV_OFFLINE_MSG, inpack->uin);
                write(fd, outpack, outpack->len);
                
                sizeleft = MSG_PARAMS_LEN;
                msgs_left = message_get(0, (char *)outpack->params, &sizeleft);
                msg_dbg("There are %d messages left, buffsize %d left\n", msgs_left, \
                        sizeleft);
            }
            
            fill_packet_header(outpack, PACKET_HEADER_LEN + MSG_PARAMS_LEN - sizeleft, \
                    SRV_OFFLINE_MSG_DONE, inpack->uin);
            msg_dbg("Packet Send %d bytes <-----------------\n", outpack->len);
            write(fd, outpack, outpack->len);
        }
        /* delete offline messages that have been sent */
        if (outpack->len > 14)
            message_delete(inpack->uin);
        break;
    case CMD_MESSAGE: //chat message
        msg_dbg("Chat_msg: from %d, to %d, len %d\n", inpack->uin, *PARAM_TO_UIN(inpack),\
                *PARAM_LENGTH(inpack));
        msg_dbg("Message: %s\n", inpack->params+10);
        msg = new_msg(inpack->uin, *PARAM_TIMESTAMP(inpack), MSG_TYPE_CHAT,\
                *PARAM_LENGTH(inpack), PARAM_MESSAGE(inpack));
        if (!msg)
            return -1;

        uin = *PARAM_TO_UIN(inpack);
        if (uin >= MAX_USER)
            return -1;
        /* put the message in receiver's message queue */
        msg->next = map[uin];
        map[uin] = msg;
        request_status(uin, outpack);
        break;
    case CMD_MSG_FRIEND: //friend message
        msg_dbg("Chat_msg: from %d, to %d, len %d\n", inpack->uin, *PARAM_TO_UIN(inpack),\
                *PARAM_TYPE(inpack));
        msg = new_msg(inpack->uin, *PARAM_TIMESTAMP(inpack), *PARAM_TYPE(inpack),\
                *PARAM_LENGTH(inpack), PARAM_MESSAGE(inpack));
        if (!msg)
            return -1;

        uin = *PARAM_TO_UIN(inpack);
        if (uin >= MAX_USER)
            return -1;
        /* put the message in receiver's message queue */
        msg->next = map[uin];
        map[uin] = msg;
        request_status(uin, outpack);
        break;
    case REP_STATUS: //send message or store offline message according to user status
        uin = *PARAM_TO_UIN(inpack);
        msg_dbg("Packet REP_STATUS: len %d, uin %d, stat %d, ip %d.\n", inpack->len, uin, \
                *PARAM_TYPE(inpack), *PARAM_IP(inpack));
        if (*PARAM_TYPE(inpack)) //online
            send_msg(uin, *PARAM_IP(inpack), outpack);
        else //offline
            store_offline_msg(uin);
        break;
    default:
        return -1;
    }

    return 0;
}

/* listen message packet and connect to status module */
int message_conn_init()
{
    struct epoll_event ev;

    listen_fd = service(MESSAGE, ACCEPT);
    if (listen_fd < 0)
        return -1;

    while ((status_fd = connect_to(STATUS)) < 0) {
        msg_dbg("wait for  connection to Status\n");
        sleep(2);
    }

    epfd = epoll_create(MAX_EVENTS);
    if (epfd == -1) {
        msg_err("epoll_create error\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        msg_err("epoll_ctl error: listen_fd\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = status_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, status_fd, &ev) == -1) {
        msg_err("epoll_ctl error: status_fd\n");
        return -1;
    }

    return 0;
}
