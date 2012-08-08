/**********************************************************
 * moudules.c
 *
 *********************************************************/
#include "modules.h"

int connect_to(int module)
{
    int fd;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    switch(module)
    {
        case USER:
            addr.sin_addr.s_addr = inet_addr(USER_IP);
            addr.sin_port = htons(USER_PORT);
            break;
        case FRIEND:
            addr.sin_addr.s_addr = inet_addr(FRIEND_IP);
            addr.sin_port = htons(FRIEND_PORT);
            break;
        case MESSAGE:
            addr.sin_addr.s_addr = inet_addr(MESSAGE_IP);
            addr.sin_port = htons(MESSAGE_PORT);
            break;
        case STATUS:
            addr.sin_addr.s_addr = inet_addr(STATUS_IP);
            addr.sin_port = htons(STATUS_PORT);
            break;
        default:
            return -1;
    }

    if((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }

    if(connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0)
    {
        perror("connect");
        return -1;
    }
    
    return fd;
}

int service(int module, int con_num)
{
    int fd;
    int ret;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    switch(module)
    {
        case USER:
            addr.sin_addr.s_addr = inet_addr(USER_IP);
            addr.sin_port = htons(USER_PORT);
            break;
        case FRIEND:
            addr.sin_addr.s_addr = inet_addr(FRIEND_IP);
            addr.sin_port = htons(FRIEND_PORT);
            break;
        case MESSAGE:
            addr.sin_addr.s_addr = inet_addr(MESSAGE_IP);
            addr.sin_port = htons(MESSAGE_PORT);
            break;
        case STATUS:
            addr.sin_addr.s_addr = inet_addr(STATUS_IP);
            addr.sin_port = htons(STATUS_PORT);
            break;
        default:
            return -1;
    }

    if((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket create");
        return -1;
    }

    if(bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0)
    {
        perror("bind");
        return -1;
    }

    if(listen(fd, con_num) < 0)
    {
        perror("listen");
        return -1;
    }

    return fd;
}

void set_nonblock(int fd)
{
    int opts;

    opts = fcntl(fd, F_GETFL);
    if(opts < 0)
    {
        perror("fcntl: F_GETFL");
        exit(EXIT_FAILURE);
    }

    opts = opts | O_NONBLOCK;
    if(fcntl(fd, F_SETFL, opts) < 0)
    {
        perror("fcntl: F_SETFL");
        exit(EXIT_FAILURE);
    }
}

