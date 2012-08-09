#include "protocol.h"
#include "conn_list.h"

#define PARAM_IP(x)     (uint32_t *)x->params
#define PARAM_TYPE(x)   (uint16_t *)(x->params + 4)

/*
 * 缓存的用户状态信息：
 * con_ip: 接入服务器的ip地址
 */
struct status_info
{
    struct list_head node;
    int con_ip;
};


int status_list_init();
struct status_info * status_list_get();
void status_list_put(struct status_info *info);
int status_packet(struct packet *inpack, struct packet *outpack);
