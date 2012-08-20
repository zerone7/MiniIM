#include "status.h"

#define MAX_USER        100000    
#define MAX_ONLINE_USER 100
#define MAX_EVENTS      10
#define ACCEPT          5

static int listen_fd, epfd; //file descriptor
static struct status_info  *cache, *map[MAX_USER+1]; //status cache
static struct list_head    free_list_head; //free list 

/* recycle the user status cache, put it in free list */
static inline void status_list_put(struct status_info *info)
{
    list_add_tail(&info->node, &free_list_head);
}

#ifndef _MODULE_
void status()
#else
void main()
#endif
{
    int client_fd ,tmpfd; //file descriptor
    int nfds, left, size, i;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in client_addr;
    struct packet *inpack, *outpack;

    inpack = malloc(MAX_PACKET_LEN);
    outpack = malloc(MAX_PACKET_LEN);

    memset(&client_addr, 0, sizeof(client_addr));
    size = sizeof(struct sockaddr_in);

    stat_dbg("Status start: %d\n", getpid());

    /* init the free cache list */
    if (status_list_init()) {
        stat_dbg("status_list_init failed !\n");
        goto exit;
    }

    /* init the network connection */
    if (status_conn_init()) {
        stat_dbg("Status connection init failed !\n");
        goto exit;
    }
    stat_dbg("==> Status listened\n");

    for (;;) {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            stat_err("************* epoll_wait error *************\n");
            goto exit;
        }

        for (i = 0; i < nfds; i++) {
            tmpfd = events[i].data.fd;
            /* new connection */
            if (tmpfd == listen_fd) {
                client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &size);
                if (client_fd < 0) {
                    stat_err("accept error\n");
                    goto exit;
                }
                stat_dbg("<== client %s, port %d connected\n", inet_ntoa(client_addr.sin_addr),\
                        ntohs(client_addr.sin_port));

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    stat_err("epoll_ctl: add error\n");
                    goto exit;
                }
                continue;
            }

            /* recieve a packet */
            if (events[i].events & EPOLLIN)
            {
                stat_dbg("<=======  A Packet Arrive! =======>\n");
                if (tmpfd < 0) {
                    stat_dbg("tmpfd(%d) < 0\n", tmpfd);
                    continue;
                }

                if (packet_read(tmpfd, (char *)inpack, PACKET_HEADER_LEN, epfd))
                    continue;
                else {
                    stat_dbg("PACKET: len %d, cmd %04x, uin %d\n", inpack->len, inpack->cmd, \
                            inpack->uin);
                    left = inpack->len - PACKET_HEADER_LEN;
                    if (left > 0) {
                        stat_dbg("packet left %d bytes to read\n", left);
                        if (packet_read(tmpfd, inpack->params, left, epfd))
                            continue;
                    }
                    
                    /* packet processing */ 
                    status_packet(inpack, outpack, tmpfd);
                }
            }
        }
    }

exit:
    stat_dbg("STATUS Exit\n");
   // free(cache);
   // free(inpack);
   // free(outpack);
}

/* listen status packet */
int status_conn_init()
{
    struct epoll_event ev;

    listen_fd = service(STATUS, ACCEPT);
    if (listen_fd < 0) {
        stat_dbg("listen fd error\n");
        return -1;
    }

    epfd = epoll_create(MAX_EVENTS);
    if (epfd == -1) {
        stat_err("epoll_create error\n");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        stat_err("epoll_ctl error: listen_fd\n");
        return -1;
    }

    return 0;
}

/* init status cache list */
int status_list_init()
{
    int i;

    cache = (struct status_info *)malloc(MAX_ONLINE_USER * sizeof(struct status_info));
    if (!cache) {
        stat_err("malloc for cache error\n");
        return -1;
    }
    memset(cache, 0, sizeof(struct status_info) * MAX_ONLINE_USER);

    INIT_LIST_HEAD(&free_list_head);
    for(i = 1; i <= MAX_ONLINE_USER; i++)
        list_add(&cache[i].node, &free_list_head);

    return 0;
}

/* get a cache item from free list */
struct status_info * status_list_get()
{
    struct status_info *info;
    if (list_empty(&free_list_head))
        return NULL;
    else {
        info = list_first_entry(&free_list_head, struct status_info, node);
        list_del(&info->node);
        return info;
    }
}

/* set user stat and ip address of the server that user connected to */
int set_status(uint32_t uin, uint32_t ip, uint16_t stat)
{
    struct status_info *info;

    if (uin >= MAX_USER) {
        stat_dbg("uin %d overflow\n", uin);
        return -1;
    }

    if (stat) { //online
        if (map[uin])
            return -1;
        info = status_list_get();
        map[uin] = info;
        info->con_ip = ip; 
    }
    else { //offline
        if (!map[uin])
            return -1;
        info = map[uin];
        map[uin] = NULL;
        status_list_put(info);
    }

    return 0;
}

/* get user status info and store it in pip and pstat */
int get_status(uint32_t uin, uint32_t *pip, uint16_t *pstat)
{
    assert(pip && pstat);

    if (uin >= MAX_USER) {
        stat_dbg("uin %d overflow\n", uin);
        return -1;
    }

    if (map[uin]) {
        *pstat = 1;
        *pip = map[uin]->con_ip;
    } else {
        *pstat = 0;
        *pip = 0;
    }

    return 0;
}

/* get status of users in list uins and store them in multi_stat */
void get_multi_status(uint32_t *uins, uint16_t num, struct user_status *multi_stat)
{
    uint32_t uin;

    assert(uins && multi_stat);
    while (num) {
        uin = uins[num-1];
        multi_stat[num-1].uin = uin;
        multi_stat[num-1].stat = map[uin] ? 1 : 0;
        num--;
    }
}

/*
 * status_packet - process status packet, return 0 on success processing, -1 on error
 * @inpack: received packet
 * @outpack：response packet
 * @sockfd: the socket that receive packet
 */
int status_packet(struct packet *inpack, struct packet *outpack, int sockfd)
{
    uint16_t num;

    assert(inpack && outpack);
    stat_dbg("Status_packet: processing packet --->\n");

    switch (inpack->cmd) {
    case CMD_STATUS_CHANGE: //status chage request
        stat_dbg("STATUS_CHANGE type: uin %d, stat: %d\n", *PARAM_UIN(inpack), \
                *PARAM_TYPE(inpack));
        if (set_status(*PARAM_UIN(inpack), *PARAM_IP(inpack), *PARAM_TYPE(inpack)))
            return -1;
        
        fill_packet_header(outpack, PACKET_HEADER_LEN, REP_STATUS_CHANGED, \
                *PARAM_UIN(inpack));
        break;
    case CMD_GET_STATUS: //user status request
        *PARAM_UIN(outpack) = *PARAM_UIN(inpack);
        if (get_status(*PARAM_UIN(inpack), PARAM_IP(outpack), PARAM_TYPE(outpack)))
            return -1;

        stat_dbg("GET_STATUS: uin %d, stat %d\n", *PARAM_UIN(inpack), \
                *PARAM_TYPE(outpack));
        fill_packet_header(outpack, PACKET_HEADER_LEN+10, REP_STATUS, inpack->uin);
        break;
    case CMD_MULTI_STATUS: //multi-user status request
        num = *(uint16_t *)inpack->params;
        stat_dbg("%d uins to got status\n", num);

        *(uint16_t *)outpack->params = num;
        fill_packet_header(outpack, PACKET_HEADER_LEN + 2 + num * 6, \
                REP_MULTI_STATUS, inpack->uin);
        get_multi_status((uint32_t *)(inpack->params + 2), num, \
                (struct user_status *)(outpack->params + 2));
        break;
    default:
        return -1;
    }

    /* send back response packet */
    write(sockfd, outpack, outpack->len);

    return 0;
}
