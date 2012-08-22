#include "modules.h"
#include "list.h"

#define PARAM_TO_UIN(x)         (uint32_t *)x->params
#define PARAM_TIMESTAMP(x)      (uint32_t *)(x->params + 4)
#define PARAM_IP(x)             (uint32_t *)(x->params + 4)
#define PARAM_LENGTH(x)         (uint16_t *)(x->params + 8)
#define PARAM_TYPE(x)           (uint16_t *)(x->params + 8)
#define PARAM_PORT(x)           (uint16_t *)(x->params + 8)
#define PARAM_STAT(x)           (uint16_t *)(x->params + 10)
#define PARAM_MESSAGE(x)        (char *) (x->params + 10)

#define MSG_HEADER_LEN          12

#ifdef DEBUG_MESSAGE
#define msg_dbg(format, arg...)             \
    printf("MESSAGE: " format, ##arg);
#else
#define msg_dbg(format, arg...)             \
({                                          \
    if(0)                                   \
        printf("MESSAGE: " format, ##arg);  \
    0;                                      \
})
#endif

#define msg_err(format, arg...)             \
    printf("MESSAGE: " format, ##arg);

/*
 * message - chat message
 * @from_uin: message sender
 * @timestamp: message timestamp
 * @type: message type
 * @len: message length
 * @msg_str: message text string
 */
struct message
{
    uint32_t    from_uin;
    uint32_t    timestamp;
    uint16_t    type;
    uint16_t    len;
    char        msg_str[0];
}__attribute__((packed));

/* message list */
struct msg_list
{
    struct msg_list *next;
    struct message  msg;
};

/*
 * con_info - connections accept by message module
 * @next: point to next connection
 * @ip: ip address of this connection peer
 * @sockfd: sock file descriptor of this connection
 */
struct con_info
{
    struct list_head    node;
    uint32_t            ip;
    uint16_t            port;
    int                 sockfd;
};

int send_msg(int uin, uint32_t ip, uint16_t port, struct packet *outpack);
int store_offline_msg(int uin);
int message_packet(struct packet *inpack, struct packet *outpack, int fd);
int message_conn_init();
struct msg_list *new_msg(int uin, int timestamp, int type, int len, \
        char *message);
