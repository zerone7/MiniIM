#include "modules.h"
#include "conn_list.h"

#define PARAM_UIN(x)    (uint32_t *)x->params
#define PARAM_IP(x)     (uint32_t *)(x->params + 4)
#define PARAM_TYPE(x)   (uint16_t *)(x->params + 8)

#ifdef DEBUG_STATUS
#define stat_dbg(format, arg...)        \
    printf("STATUS: " format, ##arg);
#else
#define stat_dbg(format, arg...)        \
({                                      \
    if(0)                               \
        printf("STATUS: " format, ##arg); \
    0;                                  \
})
#endif

#define stat_err(format, arg...)        \
    printf("STATUS: " format, ##arg);

/*
 * stauts_info - user status cache
 * @node: list node
 * @con_ip: connection server ip
 */
struct status_info
{
    struct list_head node;
    int con_ip;
};

/* user_status - 用户状态信息 */
struct user_status
{
    uint32_t uin;
    uint16_t stat;
}__attribute__((packed));

int status_packet(struct packet *inpack, struct packet *outpack,\
        int sockfd);
void get_multi_status(uint32_t *uins, uint16_t num, \
        struct user_status *multi_stat);
struct status_info * status_list_get();
int status_conn_init();
int status_list_init();
int set_status(uint32_t uin, uint32_t ip, uint16_t stat);
int get_status(uint32_t uin, uint32_t *pip, uint16_t *pstat);
