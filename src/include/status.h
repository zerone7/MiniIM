#include "protocol.h"
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
 * 缓存的用户状态信息：
 * con_ip: 接入服务器的ip地址
 */
struct status_info
{
    struct list_head node;
    int con_ip;
};

struct user_status
{
    uint32_t uin;
    uint16_t stat;
}__attribute__((packed));

int status_list_init();
struct status_info * status_list_get();
void status_list_put(struct status_info *info);
int status_packet(struct packet *inpack, struct packet *outpack);
