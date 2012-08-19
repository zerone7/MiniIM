#include "modules.h"

#define PARAM_COUNT(x)      (uint16_t *)x->params
#define PARAM_FRIENDS(x)    (uint32_t *)(x->params + 2)
#define PARAM_REPLY(x)      (uint16_t *)(x->params+4)

#ifdef DEBUG_FRIEND
#define frd_dbg(format, arg...)         \
    printf("FRIEND: " format, ##arg);
#else
#define frd_dbg(format, arg...)         \
({                                      \
    if(0)                               \
        printf("FRIEND: " format, ##arg); \
    0;                                  \
})
#endif

#define frd_err(format, arg...)         \
    printf("FRIEND: " format, ##arg);

/* status_info - user status
 * @uin: user uin
 * @stat: user online stat
 */
struct status_info
{
    uint32_t uin;
    uint16_t stat;
}__attribute__((packed));

/*
 * contact_info - user contact infomation
 * @uin: contact uin
 * @stat: contact online stat
 * @len: contanct nickname length
 * @nick: contact nickname
 */
struct contact_info
{
    uint32_t uin;
    uint16_t stat;
    uint16_t len;
    char    nick[0];
}__attribute__((packed));

/*
 * friend_msg - friend message from friend module to message module
 * @to_uin: message reciever
 * @timestamp: message timestamp
 * @type: message type
 * @len: message length (friend message length is 0)
 */
struct friend_msg
{
    uint32_t to_uin;
    uint32_t timestamp;
    uint16_t type;
    uint16_t len;
}__attribute__((packed));

int get_contacts_info(int num, struct status_info *stats, struct packet *outpack);
int friend_packet(struct packet *inpack, struct packet *outpack, int sockfd);
int send_friend_msg(struct packet *outpack, int from, int to, uint16_t type);
int friend_conn_init();
int check_friend(int uin, int friend);
