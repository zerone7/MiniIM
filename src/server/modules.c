#include "modules.h"

/* connect to one of the server modules */
int connect_to(int module)
{
    int fd;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    switch (module) {
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

    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
        perror("connect");
        return -1;
    }
    
    return fd;
}

/* bind and listen on specific ip address and port */
int service(int module, int con_num)
{
    int fd, optval;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    switch (module) {
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

    if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket create");
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) {
        perror("setsockopt");
        return -1;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0) {
        perror("bind");
        return -1;
    }

    if (listen(fd, con_num) < 0) {
        perror("listen");
        return -1;
    }

    return fd;
}

/* set socket fd to nonblocking mode */
void set_nonblock(int fd)
{
    int opts;

    opts = fcntl(fd, F_GETFL);
    if (opts < 0) {
        perror("fcntl: F_GETFL");
        exit(EXIT_FAILURE);
    }

    opts = opts | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, opts) < 0) {
        perror("fcntl: F_SETFL");
        exit(EXIT_FAILURE);
    }
}

/* send error packet */
int send_error_packet(uint32_t uin, uint16_t cmd, uint16_t error, int sockfd)
{
    struct error_packet errpack;

    errpack.len = net16(16);
    errpack.ver = net16(1);
    errpack.cmd = net16(SRV_ERROR);
    errpack.uin = net32(uin);
    errpack.client_cmd = net16(cmd);
    errpack.type = net16(error);

    return send(sockfd, &errpack, 16, 0);
}

/* read len byte from sockfd to buff */
int packet_read(int sockfd, char *buff, int len, int epfd)
{
    int nread;
    char *readbuff;
    struct epoll_event ev;

    nread = read(sockfd, buff, len);
    printf("Packet_read: len %d, read %d bytes\n", len, nread);
    if (nread < 0) {
        perror("packet_read");
        return -2;
    } else if (nread == 0) {
        printf("Close Socket(%d)\n", sockfd);
        ev.data.fd = sockfd;
        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, &ev);
        close(sockfd);
        return -1;
    } else if (nread < len) {
        len -= nread;
        readbuff = buff + nread;
        while (len) {
            nread = read(sockfd, readbuff, len);
            printf("Packet_read: left %d, read %d bytes\n", len, nread);
            if (nread < 0) {
                perror("packet_read");
                return -2;
            } else if (nread == 0) {
                printf("Close Socket(%d)\n", sockfd);
                ev.data.fd = sockfd;
                epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, &ev);
                close(sockfd);
                return -1;
            } else {
                len -= nread;
                readbuff += nread;
            }
        }
    }

    return 0;
}
