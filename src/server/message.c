/************************************************************
 * message.c
 *      消息模块，负责消息的发送及离线消息的功能
 *
 ***********************************************************/
#include "modules.h"
#include "message.h"
#include "message_db.h"

#define ACCEPT          5   //监听端口要监听的连接数
#define MAX_EVENTS      10
#define MAX_USER        100000    
#define BUFSIZE         MAX_PACKET_LEN
#define MSG_PARAMS_LEN  MAX_PACKET_LEN - PACKET_HEADER_LEN

static int listen_fd, status_fd, epfd;
static struct con_info *conns;
static struct msg_list *map[MAX_USER+1];

int add_con(uint32_t ip, int sockfd)
{
    struct con_info *new_con;

    new_con = malloc(sizeof(struct con_info));
    if(!new_con)
    {
        msg_err("alloc struct con_info error\n");
        return -1;
    }
    
    new_con->ip = ip;
    new_con->sockfd = sockfd;
    
    new_con->next = conns;
    conns = new_con;

    return 0;
}

void del_con(int sockfd)
{
    struct con_info *con, *next;

    if(conns->sockfd == sockfd)
        conns == conns->next;
    else
    {
        con = conns;
        next = con->next;
        while(next)
        {
            if(next->sockfd == sockfd)
            {
                con->next = next->next;
                break;
            }
            else
            {
                con = next;
                next = next->next;
            }
        }
    }
}

int ip_to_fd(uint32_t ip)
{
    struct con_info *con;

    con = conns;
    while(con)
    {
        if(con->ip == ip)
            return conns->sockfd;
        else
            con = con->next;
    }

    return -1;
}

int store_offline_msg(int uin)
{
    struct msg_list *msg;

    msg_dbg("Store offline msg to uin(%d)\n", uin);
    msg = map[uin];
    while(msg)
    {
        message_store(msg->msg.from_uin, uin, msg->msg.type, (char *)msg->msg.msg_str); 
        msg = msg->next;
    }
    map[uin] = NULL;

    return 0;
}

int send_msg(int uin, uint32_t ip, struct packet *outpack)
{
    int sockfd;
    struct msg_list *msg;

    msg_dbg("Send msg to uin(%d), ip(%d)\n", uin, ip);
    msg = map[uin];
    if(msg)
    {
        sockfd = ip_to_fd(ip);
        if(sockfd < 0)
            return sockfd;

        outpack->ver = (uint16_t)1;
        outpack->cmd = (uint16_t)SRV_MESSAGE;
        outpack->uin = uin;
        while(msg)
        {
            outpack->len = PACKET_HEADER_LEN + MSG_HEADER_LEN + msg->msg.len;
            memcpy(outpack->params, (void *)&msg->msg, MSG_HEADER_LEN + msg->msg.len);
            write(sockfd, outpack, outpack->len);
            msg = msg->next;
        }
    }
    map[uin] = NULL;

    return 0;
}

void get_status(int uin, struct packet *outpack)
{
    outpack->len = PACKET_HEADER_LEN + 4;
    outpack->ver = 1;
    outpack->cmd = CMD_GET_STATUS;
    outpack->uin = uin;
    *(uint32_t *)outpack->params = uin;

    msg_dbg("Sent get status request\n");
    write(status_fd, outpack, outpack->len);
}

struct msg_list *new_msg(int uin, int timestamp, int type, int len, char *message)
{
    struct msg_list *amsg;

    amsg = malloc(sizeof(struct msg_list) + len);
    if(!amsg)
    {
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

int message_packet(struct packet *inpack, struct packet *outpack, int fd)
{
    int msgs_left, sizeleft, uin;
    uint32_t *pip;
    uint16_t *pstat;
    struct msg_list *msg;

    switch(inpack->cmd)
    {
        case CMD_OFFLINE_MSG: //离线消息
            sizeleft = MSG_PARAMS_LEN;
            msgs_left = message_get(inpack->uin, (char *)outpack->params, &sizeleft);
            msg_dbg("There are %d messages left, buffsize %d left\n", msgs_left, sizeleft);
            if(msgs_left < 0) 
                return -1;
            else
            {
                while(msgs_left)
                {
                    outpack->len = PACKET_HEADER_LEN + MSG_PARAMS_LEN - sizeleft;
                    outpack->ver = 1;
                    outpack->cmd = SRV_OFFLINE_MSG;
                    outpack->uin = inpack->uin;
                    write(fd, outpack, outpack->len);
                    
                    sizeleft = MSG_PARAMS_LEN;
                    msgs_left = message_get(0, (char *)outpack->params, &sizeleft);
                    msg_dbg("There are %d messages left, buffsize %d left\n", msgs_left, sizeleft);
                }
                
                outpack->len = PACKET_HEADER_LEN + MSG_PARAMS_LEN - sizeleft;
                outpack->ver = 1;
                outpack->cmd = SRV_OFFLINE_MSG_DONE;
                outpack->uin = inpack->uin;
                msg_dbg("Packet Send %d bytes <-----------------\n", outpack->len);
                write(fd, outpack, outpack->len);
            }
            if(outpack->len > 14)
                message_delete(inpack->uin);
            break;
        case CMD_MESSAGE: //聊天消息
            msg_dbg("Chat_msg: from %d, to %d, len %d\n", inpack->uin, *PARAM_TO_UIN(inpack),\
                    *PARAM_LENGTH(inpack));
            msg_dbg("Message: %s\n", inpack->params+10);
            msg = new_msg(inpack->uin, *PARAM_TIMESTAMP(inpack), MSG_TYPE_CHAT,\
                    *PARAM_LENGTH(inpack), PARAM_MESSAGE(inpack));
            if(!msg)
                return -1;

            uin = *PARAM_TO_UIN(inpack);
            if(uin >= MAX_USER)
                return -1;
            /* 将消息挂到要发送的用户的消息队列中 */
            if(map[uin])
                msg->next = map[uin];
            else
                msg->next = NULL;
            map[uin] = msg;
            get_status(uin, outpack);
            break;
        case CMD_MSG_FRIEND: //好友相关的消息
            msg_dbg("Chat_msg: from %d, to %d, len %d\n", inpack->uin, *PARAM_TO_UIN(inpack),\
                    *PARAM_TYPE(inpack));
            msg = new_msg(inpack->uin, *PARAM_TIMESTAMP(inpack), *PARAM_TYPE(inpack),\
                    *PARAM_LENGTH(inpack), PARAM_MESSAGE(inpack));
            if(!msg)
                return -1;

            uin = *PARAM_TO_UIN(inpack);
            if(uin >= MAX_USER)
                return -1;
            /* 将消息挂到要发送的用户的消息队列中 */
            if(map[uin])
                msg->next = map[uin];
            else
                msg->next = NULL;
            map[uin] = msg;
            get_status(uin, outpack);
            break;
        case REP_STATUS:
            pstat = (uint16_t *)(inpack->params+8);
            pip = (uint32_t *)(inpack->params+4);
            uin = *(uint32_t *)inpack->params;
            msg_dbg("Packet REP_STATUS: len %d, uin %d, stat %d, ip %d.\n", inpack->len,uin,*pstat,*pip);
            if(*pstat)
                send_msg(uin, *pip, outpack);
            else
                store_offline_msg(uin);
            break;
        default:
            return -1;
    }

    return 0;
}

int message_conn_init()
{
    struct epoll_event ev;

    listen_fd = service(MESSAGE, ACCEPT);
    if(listen_fd < 0)
        return -1;

    while((status_fd = connect_to(STATUS)) < 0)
    {
        msg_dbg("wait for connection to Status\n");
        sleep(2);
    }

    epfd = epoll_create(MAX_EVENTS);
    if(epfd == -1)
    {
        msg_err("epoll_create error\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1)
    {
        msg_err("epoll_ctl error: listen_fd\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = status_fd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, status_fd, &ev) == -1)
    {
        msg_err("epoll_ctl error: status_fd\n");
        return -1;
    }

    return 0;
}

#ifndef _MODULE_
void message()
#else
void main()
#endif
{
    int  tmpfd, client_fd, size, nfds, i, n, left, ret;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in client_addr;
    struct packet *inpack, *outpack;

    msg_dbg("Message start: %d\n", getpid());

    inpack = malloc(MAX_PACKET_LEN);
    outpack = malloc(MAX_PACKET_LEN);

    memset(&client_addr, 0, sizeof(client_addr));
    size = sizeof(struct sockaddr_in);

    /* Init database connection */
    ret = message_db_init();
    if(ret)
    {
        msg_err("database connect error\n"); 
        goto exit;
    }

    ret = message_conn_init();
    if(ret)
    {
        msg_err("message connection init error\n"); 
        goto exit;
    }
    msg_dbg("==> Message listened, Status connected\n");

    for(;;)
    {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if(nfds == -1)
        {
            msg_err("epoll_wait error\n");
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
                    msg_err("accept error\n");
                    goto exit;
                }
                msg_dbg("client %s, port %d connected\n", inet_ntoa(client_addr.sin_addr), \
                        ntohs(client_addr.sin_port));

                add_con((uint32_t)client_addr.sin_addr.s_addr, client_fd);
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if(epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
                {
                    msg_err("epoll_ctl: add error\n");
                    goto exit;
                }
                continue;
            }

            /* TCP连接中收到数据包 */
            if(events[i].events & EPOLLIN)
            {
                msg_dbg("<=======  A Packet Arrive! =======>\n");
                if(tmpfd < 0)
                {
                    msg_dbg("tmpfd(%d) < 0\n", tmpfd);
                    continue;
                }

                n = read(tmpfd, inpack, PACKET_HEADER_LEN);
                msg_dbg("read %d\n", n);

                /* To be removed */
                if(!strcmp((char *)inpack, "close"))
                    goto exit;

                if(n == 0)
                {
                    ev.data.fd = tmpfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, tmpfd, &ev);
                    /* Delete connection struct */
                    del_con(tmpfd);
                    close(tmpfd);
                    continue;
                }
                else if(n < 0)
                {
                    msg_dbg("%s\n", strerror(errno));
                }
                else
                {
                    msg_dbg("PACKET: len %d, cmd %04x, uin %d\n", inpack->len, inpack->cmd, inpack->uin);
                    left = inpack->len - PACKET_HEADER_LEN;
                    if(left > 0)
                    {
                        n = read(tmpfd, inpack->params, left);
                        msg_dbg("left %d, read %d\n", left, n);
                        if(n < 0)
                        {
                            msg_dbg("%s\n", strerror(errno));
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
                            msg_dbg("read: n != left\n");
                            continue;
                        }
                    }   
                    
                    /* 处理数据包 */
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
    close(listen_fd);
}
