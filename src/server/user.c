/***************************************************************
 * user.c
 *      用户模块，负责登录及加好友功能
 *
 **************************************************************/
#include "user.h"
#include "modules.h"
#include "user_db.h"

#define ACCEPT      5   //监听端口要监听的连接数
#define MAX_EVENTS  10
#define BUFSIZE     MAX_PACKET_LEN

int passwd_verify(int uin, char *passwd)
{
    int ret;
    char user_pass[32];
    
    ret = user_get_passwd(uin, user_pass);
    if(ret < 1 || strcmp(passwd, user_pass))
    {
        printf("user[%d] password: '%s' is wrong\n", uin, passwd);
        return -1;
    }

    return 0;
}

int user_packet(struct packet *inpack, struct packet *outpack)
{
    char nick[50], *pnick;

    printf("User_packet: processing packet\n");
    switch(inpack->cmd)
    {
        case CMD_LOGIN:
            printf("UIN %d, PSSWD: %s\n", inpack->uin, (char *)inpack->params);
            if(passwd_verify(inpack->uin, (char *)inpack->params))
            {
                outpack->len = (uint16_t) PACKET_HEADER_LEN + 4;
                outpack->ver = (uint16_t) 1;
                outpack->cmd = (uint16_t) SRV_ERROR;
                outpack->uin = inpack->uin;
                
                *((uint32_t *)outpack->params) = CMD_LOGIN << 16 | 1;
            }
            else
            {
                user_get_nick(inpack->uin, nick);
                *((uint16_t *)outpack->params) = (uint16_t) strlen(nick) +1; 
                sprintf(outpack->params + 2,"%s", nick);

                outpack->len = (uint16_t) PACKET_HEADER_LEN + 2 + strlen(nick);
                outpack->ver = (uint16_t) 1;
                outpack->cmd = (uint16_t) SRV_LOGIN_OK;
                outpack->uin = inpack->uin; 

                /* 发送给状态模块状态改变请求  */
                //To be continue..
            }
            break;
        case CMD_SET_NICK:
            printf("UIN %d, nick: %s, nicklen %d\n", inpack->uin, (char *)inpack->params+2, *(uint16_t *)inpack->params);

            pnick = (char *)inpack->params+2;
            user_set_nick(inpack->uin, pnick);

            *((uint16_t *)outpack->params) = (uint16_t) strlen(pnick) + 1; 
            sprintf(outpack->params + 2,"%s", pnick);

            outpack->len = (uint16_t) PACKET_HEADER_LEN + 2 + strlen(pnick) +1;// header + 2 + strlen + '\0'
            outpack->ver = (uint16_t) 1;
            outpack->cmd = (uint16_t) SRV_SET_NICK_OK;
            outpack->uin = inpack->uin; 
            break;
        default:
            return -1;
    }

    return 0;
}

#ifdef MODULE
void user()
#else
void main()
#endif
{
    int listen_fd, status_fd, tmpfd, client_fd, epfd, size, nfds, i, n, left, ret;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in client_addr;
    struct packet *inpack, *outpack;

    inpack = malloc(MAX_PACKET_LEN);
    outpack = malloc(MAX_PACKET_LEN);

    memset(&client_addr, 0, sizeof(client_addr));
    size = sizeof(struct sockaddr_in);

    printf("User start: %d\n", getpid());

    /* Init database connection */
    ret = user_db_init();
    if(ret)
    {
        perror("database connect error\n"); 
        goto exit;
    }


    listen_fd = service(USER, ACCEPT);
    if(listen_fd < 0)
        goto exit;
    printf("==> User listened\n");

/*  status_fd = connect_to(STATUS);
    if(status_fd < 0)
        goto exit;
*/
    epfd = epoll_create(MAX_EVENTS);
    if(epfd == -1)
    {
        perror("epoll_create");
        goto exit;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1)
    {
        perror("epoll_ctl: listen_fd");
        goto exit;
    }

    for(;;)
    {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if(nfds == -1)
        {
            perror("epoll_wait");
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
                    perror("accept");
                    goto exit;
                }
                printf("client %s, port %d connected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if(epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
                {
                    perror("epoll_ctl: add");
                    goto exit;
                }
                continue;
            }

            /* TCP连接中收到数据包 */
            if(events[i].events & EPOLLIN)
            {
                printf("<=======  A Packet Arrive! =======>\n");
                if(tmpfd < 0)
                {
                    printf("tmpfd(%d) < 0\n", tmpfd);
                    continue;
                }

                n = read(tmpfd, inpack, PACKET_HEADER_LEN);
                printf("read %d\n", n);

                if(n = 0)
                {
                    ev.data.fd = tmpfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, tmpfd, &ev);
                    close(tmpfd);
                    continue;
                }
                else if(n < 0)
                {
                    printf("%s\n", strerror(errno));
                }
                else
                {
                    printf("PACKET: len %d, cmd %04x, uin %d\n", inpack->len, inpack->cmd, inpack->uin);
                    left = inpack->len - PACKET_HEADER_LEN;
                    if(left > 0)
                    {
                        n = read(tmpfd, inpack->params, left);
                        printf("left %d, read %d\n", left, n);
                        if(n < 0)
                        {
                            printf("%s\n", strerror(errno));
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
                            printf("read: n != left\n");
                            continue;
                        }
                    }   
                    
                    if(user_packet(inpack, outpack))
                        continue;
                            
                    write(tmpfd, outpack, outpack->len);
                }
            }
        }
    }

exit:
    print("==> User process is going to exit !");
    free(inpack);
    free(outpack);
    use_db_close();
    close(listen_fd);
}
