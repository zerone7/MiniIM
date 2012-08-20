#include "friend.h"
#include "friend_db.h"

#define ACCEPT          5  //connection num that can accept
#define MAX_EVENTS      10
#define MAX_USER        100000    
#define BUFSIZE         MAX_PACKET_LEN

static int listen_fd, status_fd, message_fd, user_fd, epfd;
static int fdmap[MAX_USER];

#ifndef _MODULE_
void friend()
#else
void main()
#endif
{
    int tmpfd, client_fd, size, nfds, i, nread, left;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in client_addr;
    struct packet *inpack, *outpack;

    inpack = malloc(MAX_PACKET_LEN);
    outpack = malloc(MAX_PACKET_LEN);
    memset(&client_addr, 0, sizeof(client_addr));
    size = sizeof(struct sockaddr_in);
    frd_dbg("Friend start: %d\n", getpid());

    /* init database connection */
    if (friend_db_init()) {
        frd_err("database connect error\n"); 
        goto exit;
    }

    /* init connections */
    if (friend_conn_init()) {
        frd_err("friend_conn_init error\n");
        goto exit;
    }
    frd_dbg("==> Friend listened, Statsu & Message connected\n");

    for (;;) {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            frd_err("************* epoll_wait error *************\n");
            goto exit;
        }

        for (i = 0; i < nfds; i++) {
            tmpfd = events[i].data.fd;
            /* new connection from other module */
            if (tmpfd == listen_fd) {
                client_fd = accept(listen_fd, (struct sockaddr *)&client_addr,\
                        &size);
                if (client_fd < 0) {
                    frd_err("accept error\n");
                    goto exit;
                }
                frd_dbg("client %s, port %d connected\n", \
                        inet_ntoa(client_addr.sin_addr), \
                        ntohs(client_addr.sin_port));

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    frd_err("epoll_ctl: add error\n");
                    goto exit;
                }
                continue;
            }

            /* receive a packet from socket */
            if (events[i].events & EPOLLIN) {
                frd_dbg("<=======  A Packet Arrive! =======>\n");
                if (tmpfd < 0) {
                    frd_dbg("tmpfd(%d) < 0\n", tmpfd);
                    continue;
                }

                nread = packet_read(tmpfd, (char *)inpack, PACKET_HEADER_LEN, epfd);
                if (nread)
                    continue;
                else {
                    frd_dbg("PACKET: len %d, cmd %04x, uin %d\n", inpack->len, \
                            inpack->cmd, inpack->uin);
                    left = inpack->len - PACKET_HEADER_LEN;
                    if (left > 0) {
                        frd_dbg("packet left %d bytes to read\n", left);
                        nread = packet_read(tmpfd, inpack->params, left, epfd);
                        if (nread) 
                            continue;
                    }
 
                    /* friend packet processing */
                    friend_packet(inpack, outpack, tmpfd);
                }
            }
        }
    }

exit:
    frd_dbg("==> Friend process is going to exit !\n");
    //free(inpack);
    //free(outpack);
    friend_db_close();
    close(listen_fd);
}

/* listen friend packet, connect to status and message module */
int friend_conn_init()
{
    struct epoll_event ev;

    listen_fd = service(FRIEND, ACCEPT);
    if (listen_fd < 0) {
        frd_err("Friend service start error\n");
        return -1;
    }

    while ((status_fd = connect_to(STATUS)) < 0) {
        frd_err("wait for  connection to status\n");
        sleep(2);
    }

    while ((message_fd = connect_to(MESSAGE)) < 0) {
        frd_err("wait for  connection to message\n");
        sleep(2);
    }

    while ((user_fd = connect_to(USER)) < 0) {
        frd_err("wait for  connection to USER\n");
        sleep(2);
    }

    epfd = epoll_create(MAX_EVENTS);
    if (epfd == -1) {
        frd_err("epoll_create error\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        frd_err("epoll_ctl error: listen_fd\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = status_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, status_fd, &ev) == -1) {
        frd_err("epoll_ctl error: status_fd\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = message_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, message_fd, &ev) == -1)
    {
        frd_err("epoll_ctl error: message_fd\n");
        return -1;
    }

    return 0;
}

/* 
 * check whether user(friend) is already in user(uin)'s friend list,
 * and whether the contact which the user want to add is valid or not.
 * return -1, if friend not exist; return 1 if friend already in contact
 * list; return 0 if it ok to add contact; return other if error happend.
 */
int check_friend(int uin, int friend)
{
    int num;
    uint32_t *friends;
    
    /* check whether uin is valid */
    if (friend_check_uin(friend))
        return -1;
    else {
        /* check user friend list */
        num = get_friend_num(uin);
        frd_dbg("user %d has %d friends\n", uin, num);

        if (num == 0)
            return 0;
        else if (num < 0)
            return -2;
        else {
            friends = malloc(num * 4);            
            get_friend_list(uin, num, friends);
            while (num > 0) { 
                num--;
                if (friend == friends[num])
                    return 1;
            }
            free(friends);
            return 0;
        }
    }
}

/* set friend message to message module */
int send_friend_msg(struct packet *outpack, int from, int to, uint16_t type)
{
    struct friend_msg *fmsg;

    assert(outpack);
    fill_packet_header(outpack, PACKET_HEADER_LEN + 12, CMD_MSG_FRIEND, from);
    fmsg = (struct friend_msg *)outpack->params;
    fmsg->to_uin = to;
    fmsg->type = type;
    fmsg->len = 0;

    return write(message_fd, outpack, outpack->len);
}

/*
 * friend_packet - process friend packet and send back response packet
 * @inpack: received packet
 * @outpack: response packet
 * @fd: socket that receive packet
 */
int friend_packet(struct packet *inpack, struct packet *outpack, int sockfd)
{
    int uin, num, ret;

    assert(inpack && outpack);
    switch (inpack->cmd) {
    case CMD_ADD_CONTACT: // add friend request
        uin = *(uint32_t *)inpack->params;
        frd_dbg("user %d add friend %d\n", inpack->uin, uin);
        ret = check_friend(inpack->uin, uin);
        if (ret) {
            if (ret == 1) {
                /* they are already friends */
                send_error_packet(uin, CMD_ADD_CONTACT, ERR_ALREADY_FRIEDN, sockfd);
            } else if (ret == -1) {
                /* friend is not a valid user */
                send_error_packet(uin, CMD_ADD_CONTACT, ERR_NOT_EXIST, sockfd);
            }
        } else {
            /* they are not friend now */
            send_friend_msg(outpack, inpack->uin, uin, MSG_TYPE_REQUEST);

            fill_packet_header(outpack, PACKET_HEADER_LEN + 12, \
                    SRV_ADD_CONTACT_WAIT, inpack->uin);
            *(uint32_t *)outpack->params = uin;
            get_friend_nick(uin, (uint16_t *)(outpack->params+4), outpack->params+6);
            outpack->len = PACKET_HEADER_LEN + 6 + *(uint16_t *)(outpack->params+4); 
            write(sockfd, outpack, outpack->len);
        }
        break;
    case CMD_ADD_CONTACT_REPLY: // add friend request reply
        uin = *(uint32_t *)inpack->params;
        /* add each other to their friend list and increase their friend count */
        if (*PARAM_REPLY(inpack) == 1) {
            send_friend_msg(outpack, inpack->uin, uin, MSG_TYPE_ACCEPT);
            friend_add_contact(uin, inpack->uin);
            friend_add_contact(inpack->uin, uin);
            change_friend_num(uin, -1);
            change_friend_num(inpack->uin, -1);
        }
        else
            send_friend_msg(outpack, inpack->uin, uin, MSG_TYPE_REFUSE);
        break;
    case CMD_CONTACT_LIST: // get friend list request
        uin = inpack->uin;
        num = get_friend_num(uin);
        frd_dbg("user %d has %d friends\n", uin, num);
        if (num < 0) {
            frd_err("get friend num error\n");
            return -1;
        }
        else if (num == 0) {
            fill_packet_header(outpack, PACKET_HEADER_LEN + 2, SRV_CONTACT_LIST, uin);
            *PARAM_COUNT(outpack) = 0;
            write(sockfd, outpack, outpack->len);
        } else {
            fill_packet_header(outpack, PACKET_HEADER_LEN + 2 + num * 4, \
                    SRV_CONTACT_LIST, uin);
            *PARAM_COUNT(outpack) = (uint16_t)num;
            get_friend_list(uin, num, PARAM_FRIENDS(outpack));
            write(sockfd, outpack, outpack->len);
        }
        break;
    case CMD_CONTACT_INFO_MULTI: // get multi-contacts info
        fdmap[inpack->uin] = sockfd;
        memcpy(outpack, inpack, inpack->len); 
        outpack->cmd = CMD_MULTI_STATUS;
        write(status_fd, outpack, outpack->len);
        break;
    case REP_MULTI_STATUS: // user status reply from status module
        num = *PARAM_COUNT(inpack);
        outpack->ver = 1;
        outpack->cmd = SRV_CONTACT_INFO_MULTI;
        outpack->uin = inpack->uin;
        get_contacts_info(num, (struct status_info *)(inpack->params + 2), outpack);
        write(fdmap[inpack->uin], outpack, outpack->len);
        break;
    default:
        return -1;
    }

    return 0;
}

/* send friend add packet to user module */
int change_friend_num(int uin, int num)
{
    struct packet userpack;
    if (num == -1) {
        userpack.len = PACKET_HEADER_LEN;
        userpack.cmd = CMD_FRIEND_ADD;
        userpack.ver = 1;
        userpack.uin = uin;
        write(user_fd, &userpack, userpack.len);
    }

    return 0;
}

/* get contact infomation */
int get_contacts_info(int num, struct status_info *stats, struct packet *outpack)
{
    uint32_t i;
    char *pstart;
    struct contact_info *pcontact;

    assert(stats && outpack);
    *PARAM_COUNT(outpack) = (uint16_t)num;
    outpack->len = PACKET_HEADER_LEN + 2;
    pstart = outpack->params+2;
    for (i = 0; i < num; i++) {
        pcontact = (struct contact_info *)pstart;
        pcontact->uin = stats[i].uin;
        pcontact->stat = stats[i].stat;
        get_friend_nick(pcontact->uin, &pcontact->len, (char *)pcontact->nick);
        pstart += 8 + pcontact->len;
        outpack->len += 8 + pcontact->len;
    }

    return 0;
}
