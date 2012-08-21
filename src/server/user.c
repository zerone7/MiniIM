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
    unsigned char user_pass[16];
    //unsigned char pass_hash[16];
    
    //MD5(passwd, strlen(passwd), pass_hash);
    if(user_get_passwd(uin, user_pass) < 1)
    {
        user_err("get_pass word error\n");
        return -1;
    }

    //if (memcmp(pass_hash, password)) {
    if (strcmp(passwd, user_pass)) {
        user_dbg("user[%d] password: '%s' is wrong\n", uin, passwd);
        return -1;
    }

    return 0;
}

/* send status change request to Status module */
int request_status_change(int uin, int sockfd, uint16_t stat)
{
    struct sockaddr_in addr;
    socklen_t   len;

    getpeername(sockfd, (struct sockaddr *)&addr, &len);
    /* login success, change user status  */
    fill_packet_header(status_pack, PACKET_HEADER_LEN + 12, \
            CMD_STATUS_CHANGE, uin);
    *PARAM_UIN(status_pack) = uin;
    *PARAM_IP(status_pack)  = (uint32_t)addr.sin_addr.s_addr;
    *PARAM_PORT(status_pack) = (uint16_t)addr.sin_port;
    *PARAM_TYPE(status_pack) = stat;
    write(status_fd, status_pack, status_pack->len);
}

/*
 * user_packet - process packet and send response packet
 * @inpack: recieved packet
 * @outpack: response packet
 * @sockfd: the socket that recieve packet
 */
int user_packet(struct packet *inpack, struct packet *outpack, int sockfd)
{
    char  *pnick, *password;
    uint16_t len;

    assert(inpack && outpack);
    user_dbg("User_packet: processing packet\n");
    switch (inpack->cmd) {
    case CMD_LOGIN: //user login
        password = PARAM_PASSWD(inpack);
        password[*PARAM_PASSLEN(inpack)] = '\0';
        user_dbg("UIN %d, PSSWD: %s\n", inpack->uin, password);
        if(passwd_verify(inpack->uin, PARAM_PASSWD(inpack)))
            send_error_packet(inpack->uin, CMD_LOGIN, 1, sockfd);
        else {
            /*login ok, change status */
            request_status_change(inpack->uin, sockfd, 1);
            /* send back login result and nickname*/
            user_get_nick(inpack->uin, PARAM_NICK(outpack));
            *PARAM_NICKLEN(outpack) = (uint16_t)strlen(PARAM_NICK(outpack)) + 1; 
            /* packet len = headerlen(12) + nicklen(2) + strlen + strend(1) */
            fill_packet_header(outpack, \
                    PACKET_HEADER_LEN + 2 + *PARAM_NICKLEN(outpack),\
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
    case CMD_REGISTER: //user register
        pnick = PARAM_NICK(inpack);
        len = *(uint16_t *)(inpack->params + 2 + *PARAM_NICKLEN(inpack));
        password = (char *)(inpack->params + 4 + *PARAM_NICKLEN(inpack));
        password[len] = '\0';
        inpack->uin = user_add(pnick, password);
        if (inpack->uin <= 0) {
            user_err("user register error\n");
            return -1;
        }
        fill_packet_header(outpack, PACKET_HEADER_LEN, SRV_REGISTER_OK, inpack->uin); 
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

