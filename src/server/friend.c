/****************************************************
 * friend.c
 *      好友模块，负责添加好友及查询好友功能
 *
 ***************************************************/
#include "modules.h"
#include "friend.h"
#include "friend_db.h"

#define ACCEPT          5   //监听端口要监听的连接数
#define MAX_EVENTS      10
#define MAX_USER        100000    
#define BUFSIZE         MAX_PACKET_LEN

static int listen_fd, status_fd, message_fd, user_fd, epfd;
static int fdmap[MAX_USER];

int friend_conn_init()
{
    struct epoll_event ev;

    listen_fd = service(FRIEND, ACCEPT);
    if(listen_fd < 0)
    {
        frd_err("Friend service start error\n");
        return -1;
    }

    while((status_fd = connect_to(STATUS)) < 0)
    {
        frd_err("wait for connection to status\n");
        sleep(2);
    }

    while((message_fd = connect_to(MESSAGE)) < 0)
    {
        frd_err("wait for connection to message\n");
        sleep(2);
    }

    while((user_fd = connect_to(USER)) < 0)
    {
        frd_err("wait for connection to USER\n");
        sleep(2);
    }

    epfd = epoll_create(MAX_EVENTS);
    if(epfd == -1)
    {
        frd_err("epoll_create error\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1)
    {
        frd_err("epoll_ctl error: listen_fd\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = status_fd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, status_fd, &ev) == -1)
    {
        frd_err("epoll_ctl error: status_fd\n");
        return -1;
    }
/*
    ev.events = EPOLLIN;
    ev.data.fd = message_fd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, message_fd, &ev) == -1)
    {
        frd_err("epoll_ctl error: message_fd\n");
        return -1;
    }
*/
    return 0;
}

int check_friend(int uin, int friend)
{
    int num;
    uint32_t *friends;
    
    num = get_friend_num(uin);
    frd_dbg("user %d has %d friends\n", uin, num);

    if(num == 0)
        return 0;
    else if(num < 0)
        return -1;
    else
    {
        friends = malloc(num * 4);            
        get_friend_list(uin, num, friends);
        while(num > 0)
        {
            num--;
            if(friend == friends[num])
                return 1;
        }
        free(friends);

        return 0;
    }
}

int send_friend_msg(struct packet *outpack, int from, int to, uint16_t type)
{
    struct friend_msg *fmsg;

    outpack->len = PACKET_HEADER_LEN + 12;
    outpack->ver = 1;
    outpack->cmd = CMD_MSG_FRIEND;
    outpack->uin = from;

    fmsg = (struct friend_msg *)outpack->params;
    fmsg->to_uin = to;
    fmsg->type = type;
    fmsg->len = 0;

    return write(message_fd, outpack, outpack->len);
}

int friend_packet(struct packet *inpack, struct packet *outpack, int sockfd)
{
    static int uin, num;
    static struct status_info *stats;
    static uint16_t reply;

    switch(inpack->cmd)
    {
        case CMD_ADD_CONTACT:
            uin = *(uint32_t *)inpack->params;
            frd_dbg("user %d add friend %d\n", inpack->uin, uin);
            if(check_friend(inpack->uin, uin))
            {
                /* 已经是好友 */
                outpack->len = PACKET_HEADER_LEN + 4;
                outpack->ver = 1;
                outpack->cmd = SRV_ERROR;
                outpack->uin = uin;
                *(uint32_t *)outpack->params = CMD_ADD_CONTACT<<16 | 0x1;
                write(sockfd, outpack, outpack->len);
            }
            else
            {
                /* 目前不是好友 */
                send_friend_msg(outpack, inpack->uin, uin, MSG_TYPE_REQUEST);

                outpack->len = PACKET_HEADER_LEN + 12;
                outpack->ver = 1;
                outpack->cmd = SRV_ADD_CONTACT_WAIT;
                outpack->uin = inpack->uin;
                *(uint32_t *)outpack->params = uin;
                get_friend_nick(uin, (uint16_t *)(outpack->params+4), outpack->params+6);
                outpack->len = PACKET_HEADER_LEN + 6 + *(uint16_t *)(outpack->params+4); 
                write(sockfd, outpack, outpack->len);
            }
            break;
        case CMD_ADD_CONTACT_REPLY:
            uin = *(uint32_t *)inpack->params;
            reply = *(uint16_t *)(inpack->params+4);
            if(reply == 1)
            {
                send_friend_msg(outpack, inpack->uin, uin, MSG_TYPE_ACCEPT);
                friend_add_contact(uin, inpack->uin);
                change_friend_num(uin, -1);
            }
            else
                send_friend_msg(outpack, inpack->uin, uin, MSG_TYPE_REFUSE);
            break;
        case CMD_CONTACT_LIST:
            uin = inpack->uin;
            num = get_friend_num(uin);
            frd_dbg("user %d has %d friends\n", uin, num);
            if(num < 0)
            {
                frd_err("get friend num error\n");
                return -1;
            }
            else if(num == 0)
            {
                outpack->len = PACKET_HEADER_LEN + 2;
                outpack->ver = 1;
                outpack->cmd = SRV_CONTACT_LIST;
                outpack->uin = uin;
                *PARAM_COUNT(outpack) = 0;
                write(sockfd, outpack, outpack->len);
            }
            else
            {
                outpack->len = PACKET_HEADER_LEN + 2 + num*4;
                outpack->ver = 1;
                outpack->cmd = SRV_CONTACT_LIST;
                outpack->uin = uin;
                *PARAM_COUNT(outpack) = (uint16_t)num;
                get_friend_list(uin, num, PARAM_FRIENDS(outpack));
                write(sockfd, outpack, outpack->len);
            }
            break;
        case CMD_CONTACT_INFO_MULTI:
            fdmap[inpack->uin] = sockfd;
            memcpy(outpack, inpack, inpack->len); 
            outpack->cmd = CMD_MULTI_STATUS;
            write(status_fd, outpack, outpack->len);
            break;
        case REP_MULTI_STATUS:
            num = *PARAM_COUNT(inpack);
            stats = (struct status_info *)(inpack->params + 2);
            outpack->ver = 1;
            outpack->cmd = SRV_CONTACT_INFO_MULTI;
            outpack->uin = inpack->uin;
            get_contacts_info(num, stats, outpack);
            write(fdmap[inpack->uin], outpack, outpack->len);
            break;
        default:
            return -1;
    }
    return 0;
}

int change_friend_num(int uin, int num)
{
    struct packet userpack;
    if(num == -1)
    {
        userpack.len = PACKET_HEADER_LEN;
        userpack.cmd = CMD_FRIEND_ADD;
        userpack.ver = 1;
        userpack.uin = uin;
        write(user_fd, &userpack, userpack.len);
    }

    return 0;
}

int get_contacts_info(int num, struct status_info *stats, struct packet *outpack)
{
    uint32_t i;
    char *pstart;
    struct contact_info *pcontact;

    *PARAM_COUNT(outpack) = (uint16_t)num;
    outpack->len = PACKET_HEADER_LEN + 2;
    pstart = outpack->params+2;
    for(i = 0; i < num; i++)
    {
        pcontact = (struct contact_info *)pstart;
        pcontact->uin = stats[i].uin;
        pcontact->stat = stats[i].stat;
        get_friend_nick(pcontact->uin, &pcontact->len, (char *)pcontact->nick);
        pstart += 8 + pcontact->len;
        outpack->len += 8 + pcontact->len;
    }

    return 0;
}

#ifndef _MODULE_
void friend()
#else
void main()
#endif
{
    int tmpfd, client_fd, size, nfds, i, n, left;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in client_addr;
    struct packet *inpack, *outpack;

    inpack = malloc(MAX_PACKET_LEN);
    outpack = malloc(MAX_PACKET_LEN);

    memset(&client_addr, 0, sizeof(client_addr));
    size = sizeof(struct sockaddr_in);

    frd_dbg("Friend start: %d\n", getpid());

    /* Init database connection */
    if(friend_db_init())
    {
        frd_err("database connect error\n"); 
        goto exit;
    }

    /* Init connections */
    if(friend_conn_init())
    {
        frd_err("friend_conn_init error\n");
        goto exit;
    }
    frd_dbg("==> Friend listened, Statsu & Message connected\n");

    for(;;)
    {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if(nfds == -1)
        {
            frd_err("epoll_wait error\n");
            goto exit;
        }

        for(i = 0; i < nfds; i++)
        {
            tmpfd = events[i].data.fd;
            /* 产生新的TCP连接 */
            if(tmpfd == listen_fd)
            {
                client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &size);
                if(client_fd < 0)
                {
                    frd_err("accept error\n");
                    goto exit;
                }
                frd_dbg("client %s, port %d connected\n", inet_ntoa(client_addr.sin_addr), \
                        ntohs(client_addr.sin_port));

                //add_con((uint32_t)client_addr.sin_addr.s_addr, client_fd);
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if(epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
                {
                    frd_err("epoll_ctl: add error\n");
                    goto exit;
                }
                continue;
            }

            /* TCP连接中收到数据包 */
            if(events[i].events & EPOLLIN)
            {
                frd_dbg("<=======  A Packet Arrive! =======>\n");
                if(tmpfd < 0)
                {
                    frd_dbg("tmpfd(%d) < 0\n", tmpfd);
                    continue;
                }

                n = read(tmpfd, inpack, PACKET_HEADER_LEN);
                frd_dbg("read %d\n", n);

                /* To be removed */
                if(!strcmp((char *)inpack, "close"))
                    goto exit;

                if(n == 0)
                {
                    ev.data.fd = tmpfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, tmpfd, &ev);
                    close(tmpfd);
                    continue;
                }
                else if(n < 0)
                {
                    frd_dbg("%s\n", strerror(errno));
                }
                else
                {
                    frd_dbg("PACKET: len %d, cmd %04x, uin %d\n", inpack->len, inpack->cmd, inpack->uin);
                    left = inpack->len - PACKET_HEADER_LEN;
                    if(left > 0)
                    {
                        n = read(tmpfd, inpack->params, left);
                        frd_dbg("left %d, read %d\n", left, n);
                        if(n < 0)
                        {
                            frd_dbg("%s\n", strerror(errno));
                        }
                        else if(n == 0)
                        {
                            ev.data.fd = tmpfd;
                            epoll_ctl(epfd, EPOLL_CTL_DEL, tmpfd, &ev);
                            close(tmpfd);
                            continue;
                        }

                        if(n != left)
                        {
                            frd_dbg("read: n != left\n");
                            continue;
                        }
                    }   
                    
                    /* 处理数据包 */
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
