/*************************************************************
 * status.c
 *      状态模块，负责状态的维护及查询功能
 *
 ************************************************************/
#include "modules.h"
#include "status.h"

#define MAX_USER        100000    
#define MAX_ONLINE_USER 100
#define MAX_EVENTS      10
#define ACCEPT          5

struct status_info  *cache;
struct status_info  *map[MAX_USER+1];
struct list_head    free_list_head; 

int status_list_init()
{
    int i;

    cache = (struct status_info *)malloc(MAX_ONLINE_USER * sizeof(struct status_info));
    if(!cache)
    {
        stat_err("malloc for cache error\n");
        return -1;
    }
    memset(cache, 0, sizeof(struct status_info) * MAX_ONLINE_USER);

    INIT_LIST_HEAD(&free_list_head);
    for(i = 1; i <= MAX_ONLINE_USER; i++)
        list_add(&cache[i].node, &free_list_head);

    return 0;
}

struct status_info * status_list_get()
{
    struct status_info *info;
    if(list_empty(&free_list_head))
        return NULL;
    else
    {
        info = list_first_entry(&free_list_head, struct status_info, node);
        list_del(&info->node);
        return info;
    }
}

void status_list_put(struct status_info *info)
{
    list_add_tail(&info->node, &free_list_head);
}

int status_packet(struct packet *inpack, struct packet *outpack)
{
    uint32_t *pcon_ip, uin, *puin;
    uint16_t *ptype;
    struct status_info *info;

    stat_dbg("Status_packet: processing packet --->\n");

    switch(inpack->cmd)
    {
        case CMD_STATUS_CHANGE:
            uin = *PARAM_UIN(inpack);
            if(uin >= MAX_USER)
            {
                stat_dbg("uin %d overflow\n", uin);
                return -1;
            }
            ptype = PARAM_TYPE(inpack);
            pcon_ip = PARAM_IP(inpack);
            stat_dbg("STATUS_CHANGE type: %d, ip: %d\n", *ptype, *pcon_ip);
            if(*ptype)  //上线
            {
                if(map[uin])
                    return -1;
                info = status_list_get();
                map[uin] = info;
                info->con_ip = *pcon_ip; 
            }
            else  //下线
            {
                if(!map[uin])
                    return -1;
                info = map[uin];
                map[uin] = NULL;
                status_list_put(info);
            }
            outpack->len = PACKET_HEADER_LEN;
            outpack->ver = 1;
            outpack->cmd = REP_STATUS_CHANGED;
            outpack->uin = uin;
            break;
        case CMD_GET_STATUS:
            uin = *(uint32_t *)inpack->params;
            if(uin >= MAX_USER)
            {
                stat_dbg("uin %d overflow\n", uin);
                return -1;
            }
            ptype = PARAM_TYPE(outpack);
            pcon_ip = PARAM_IP(outpack);
            *PARAM_UIN(outpack) = uin;
            if(map[uin])
            {
                *ptype = 1;
                *pcon_ip = map[uin]->con_ip;
            }
            else
            {
                *ptype = 0;
                *pcon_ip = 0;
            }

            stat_dbg("GET_STATUS stat: %d\n", *ptype);
            outpack->ver =1;
            outpack->len = PACKET_HEADER_LEN + 10;
            outpack->cmd = REP_STATUS;
            outpack->uin = inpack->uin;
            break;
        default:
            return -1;
    }

    return 0;
}

#ifdef _MODULE_
void status()
#else
void main()
#endif
{
    int listen_fd, client_fd, epfd ,tmpfd; //file descriptor
    int nfds, n, left, size, i;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in client_addr;
    struct packet *inpack, *outpack;

    inpack = malloc(MAX_PACKET_LEN);
    outpack = malloc(MAX_PACKET_LEN);

    memset(&client_addr, 0, sizeof(client_addr));
    size = sizeof(struct sockaddr_in);

    stat_dbg("Status start: %d\n", getpid());

    /* Init database connection */
    if(status_list_init())
    {
        stat_dbg("status_list_init failed !\n");
        goto exit;
    }

    listen_fd = service(STATUS, ACCEPT);
    if(listen_fd < 0)
        goto exit;
    stat_dbg("==> Status listened\n");

    epfd = epoll_create(MAX_EVENTS);
    if(epfd == -1)
    {
        stat_err("epoll_create error\n");
        goto exit;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1)
    {
        stat_err("epoll_ctl error: listen_fd\n");
        goto exit;
    }

    for(;;)
    {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if(nfds == -1)
        {
            stat_err("epoll_wait error\n");
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
                    stat_err("accept error\n");
                    goto exit;
                }
                stat_dbg("client %s, port %d connected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if(epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
                {
                    stat_err("epoll_ctl: add error\n");
                    goto exit;
                }
                continue;
            }

            /* TCP连接中收到数据包 */
            if(events[i].events & EPOLLIN)
            {
                stat_dbg("<=======  A Packet Arrive! =======>\n");
                if(tmpfd < 0)
                {
                    stat_dbg("tmpfd(%d) < 0\n", tmpfd);
                    continue;
                }

                n = read(tmpfd, inpack, PACKET_HEADER_LEN);
                stat_dbg("read %d\n", n);

                /* To be removed */
                if(!strcmp((char *)inpack, "close"))
                    goto exit;

                if(n = 0)
                {
                    ev.data.fd = tmpfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, tmpfd, &ev);
                    close(tmpfd);
                    continue;
                }
                else if(n < 0)
                {
                    stat_dbg("%s\n", strerror(errno));
                }
                else
                {
                    stat_dbg("PACKET: len %d, cmd %04x, uin %d\n", inpack->len, inpack->cmd, inpack->uin);
                    left = inpack->len - PACKET_HEADER_LEN;
                    if(left > 0)
                    {
                        n = read(tmpfd, inpack->params, left);
                        stat_dbg("left %d, read %d\n", left, n);
                        if(n < 0)
                        {
                            stat_dbg("%s\n", strerror(errno));
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
                            stat_dbg("read: n != left\n");
                            continue;
                        }
                    }   
                    
                    if(status_packet(inpack, outpack))
                    {   
                        write(tmpfd, " ", 1);
                        continue;
                    }
                            
                    write(tmpfd, outpack, outpack->len);
                }
            }
        }
    }

exit:
    stat_dbg("STATUS Exit\n");
   // free(cache);
   // free(inpack);
   // free(outpack);
    close(listen_fd);
    
}
