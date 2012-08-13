#include "protocol.h"

#define PARAM_TO_UIN(x)         (uint32_t *)x->params
#define PARAM_TIMESTAMP(x)      (uint32_t *)(x->params + 4)
#define PARAM_LENGTH(x)         (uint16_t *)(x->params + 8)
#define PARAM_MESSAGE(x)        (char *) (x->params + 10)

#define MSG_HEADER_LEN          12

#ifdef DEBUG_MESSAGE
#define msg_dbg(format, arg...)             \
    printf("MESSAGE: " format, ##arg);
#else
#define msg_dbg(format, arg...)             \
({              g                           \
    if(0)                                   \
        printf("MESSAGE: " format, ##arg);  \
    0;                                      \
})
#endif

#define msg_err(format, arg...)             \
    printf("MESSAGE: " format, ##arg);

struct message
{
    uint32_t    from_uin;
    uint32_t    timestamp;
    uint16_t    type;
    uint16_t    len;
    char        msg_str[0];
}__attribute__((packed));

struct msg_list
{
    struct msg_list *next;
    struct message  msg;
};

struct con_info
{
    struct con_info *next;
    uint32_t        ip;
    int             sockfd;
};

int add_con(uint32_t ip, int sockfd);
void del_con(int sockfd);
int ip_to_fd(uint32_t ip);
void get_status(int uin, struct packet *outpack);
struct msg_list *new_msg(int uin, int timestamp, int type, int len, char *message);
int send_msg(int uin, uint32_t ip, struct packet *outpack);
int store_offline_msg(int uin);
int message_packet(struct packet *inpack, struct packet *outpack, int fd);
void message();
