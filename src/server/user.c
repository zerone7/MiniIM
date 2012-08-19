#include "user.h"
#include "user_db.h"

#define ACCEPT      5   //connection num that can accept
#define MAX_EVENTS  10
#define BUFSIZE     MAX_PACKET_LEN

static struct packet *inpack, *outpack, *status_pack;
static int listen_fd, status_fd, epfd;

#ifndef _MODULE_
void user()
#else
void main()
#endif
{
    int tmpfd, client_fd, size, nfds, i, left;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in client_addr;

    inpack = malloc(MAX_PACKET_LEN);
    outpack = malloc(MAX_PACKET_LEN);
    status_pack = malloc(MAX_PACKET_LEN);

    size = sizeof(struct sockaddr_in);
    memset(&client_addr, 0, sizeof(client_addr));

    user_dbg("User start: %d\n", getpid());
    /* Init database connection */
    if (user_db_init()) {
        user_err("database connect error\n"); 
        goto exit;
    }

    /* Init network connection */
    if (user_conn_init()) {
        user_err("user connection init error\n");
        goto exit;
    }
    user_dbg("User listened, Status connected\n");

    for (;;) {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            user_err("************* epoll_wait error *************\n");
            goto exit;
        }

        for (i = 0; i < nfds; i++) {
            tmpfd = events[i].data.fd;
            /* new connection from connection module */
            if (tmpfd == listen_fd) {
                client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &size);
                if (client_fd < 0) {
                    user_err("accept error\n");
                    goto exit;
                }
                user_dbg("client %s, port %d connected\n", inet_ntoa(client_addr.sin_addr),\
                        ntohs(client_addr.sin_port));

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    user_err("epoll_ctl: add \n");
                    goto exit;
                }
                continue;
            }

            /* receive a packet */
            if (events[i].events & EPOLLIN) {
                if (tmpfd < 0) {
                    user_dbg("tmpfd(%d) < 0\n", tmpfd);
                    continue;
                }
                user_dbg("<=======  A Packet Arrive! =======>\n");

                if (packet_read(tmpfd, (char *)inpack, PACKET_HEADER_LEN, epfd))
                    continue; //connection closed or error happened
                else {
                    user_dbg("PACKET: len %d, cmd %04x, uin %d\n", inpack->len, inpack->cmd, \
                            inpack->uin);
                    left = inpack->len - PACKET_HEADER_LEN;
                    if (left > 0) {
                        user_dbg("packet left %d bytes to read\n", left);
                        if(packet_read(tmpfd, inpack->params, left, epfd))
                            continue;
                    }
                    /* packet processing */ 
                    user_packet(inpack, outpack, tmpfd);
                }
            }
        }
    }

exit:
    user_dbg("==> User process is going to exit !\n");
    user_db_close();
}

/* listen user packet, connect to status module */
int user_conn_init()
{
    struct epoll_event ev;

    listen_fd = service(USER, ACCEPT);
    if(listen_fd < 0)
        return -1;

    while ((status_fd = connect_to(STATUS)) < 0) {
        user_err("wait for connection to status\n");
        sleep(2);
    }

    epfd = epoll_create(MAX_EVENTS);
    if (epfd == -1) {
        user_err("epoll_create error\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        user_err("epoll_ctl: listen_fd error\n");
        return -1;
    }
    ev.events = EPOLLIN;
    ev.data.fd = status_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, status_fd, &ev) == -1) {
        user_err("epoll_ctl: status_fd error\n");
        return -1;
    }

    return 0;
}

/* check uin and password */
int passwd_verify(int uin, char *passwd)
{
    int ret;
    char user_pass[32];
    
    ret = user_get_passwd(uin, user_pass);
    if (ret < 1 || strcmp(passwd, user_pass)) {
        user_dbg("user[%d] password: '%s' is wrong\n", uin, passwd);
        return -1;
    }

    return 0;
}

/*
 * user_packet - process packet and send response packet
 * @inpack: recieved packet
 * @outpack: response packet
 * @sockfd: the socket that recieve packet
 */
int user_packet(struct packet *inpack, struct packet *outpack, int sockfd)
{
    char nick[50], *pnick;

    assert(inpack && outpack);
    user_dbg("User_packet: processing packet\n");
    switch (inpack->cmd) {
    case CMD_LOGIN: //user login
        user_dbg("UIN %d, PSSWD: %s\n", inpack->uin, PARAM_PASSWD(inpack));
        if(passwd_verify(inpack->uin, PARAM_PASSWD(inpack)))
            send_error_packet(inpack->uin, CMD_LOGIN, 1, sockfd);
        else {
            struct sockaddr_in addr;
            socklen_t   len;
            getpeername(sockfd, (struct sockaddr *)&addr, &len);
            /* login success, change user status  */
            fill_packet_header(status_pack, PACKET_HEADER_LEN + 10, \
                    CMD_STATUS_CHANGE, inpack->uin);
            *PARAM_UIN(status_pack) = inpack->uin;
            *PARAM_IP(status_pack)  = (uint32_t)addr.sin_addr.s_addr;
            *PARAM_TYPE(status_pack) = 1;
            write(status_fd, status_pack, status_pack->len);

            /* send back login result and nickname*/
            user_get_nick(inpack->uin, nick);
            *PARAM_NICKLEN(outpack) = (uint16_t) strlen(nick) +1; 
            strcpy(PARAM_NICK(outpack), nick);
            /* packet len = headerlen(12) + nicklen(2) + strlen + strend(1) */
            fill_packet_header(outpack, PACKET_HEADER_LEN + 3 + strlen(nick), \
                    SRV_LOGIN_OK, inpack->uin); 

            write(sockfd, outpack, outpack->len);
        }
        break;
    case CMD_SET_NICK: //user change nickname
        user_dbg("UIN %d, nick: %s, nicklen %d\n", inpack->uin, PARAM_NICK(inpack), \
                *PARAM_NICKLEN(inpack));

        pnick = PARAM_NICK(inpack);
        user_set_nick(inpack->uin, pnick);
        *PARAM_NICKLEN(outpack) = *PARAM_NICKLEN(inpack); 
        strcpy(PARAM_NICK(outpack), pnick);

        /* packet len = headerlen(12) + nicklen(2) + strlen + strend(1) */
        fill_packet_header(outpack, PACKET_HEADER_LEN + 3 + strlen(pnick), \
                SRV_SET_NICK_OK, inpack->uin);
        write(sockfd, outpack, outpack->len);
        break;
    case CMD_FRIEND_ADD: //increase user friend count
        user_friend_add(inpack->uin);
        return 1;
        break;
    default:
        return -1;
    }

    return 0;
}

