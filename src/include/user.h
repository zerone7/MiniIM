#include "modules.h"

#define PARAM_PASSLEN(x)    (uint16_t *)x->params 
#define PARAM_PASSWD(x)     (char *)(x->params + 2) 
#define PARAM_NICKLEN(x)    (uint16_t *)x->params
#define PARAM_NICK(x)       (char *)(x->params + 2)

#define PARAM_UIN(x)        (uint32_t *)x->params
#define PARAM_IP(x)         (uint32_t *)(x->params + 4)
#define PARAM_TYPE(x)       (uint16_t *)(x->params + 8)

#ifdef DEBUG_USER
#define user_dbg(format, arg...)        \
    printf("USER: " format, ##arg);
#else
#define user_dbg(format, arg...)        \
({                                      \
    if(0)                               \
        printf("USER: " format, ##arg); \
    0;                                  \
})
#endif


#define user_err(format, arg...) \
    printf("USER: " format, ##arg);

int user_conn_init();
int user_add(char *nick, char *passwd);
int request_status_change(int uin, int sockfd, uint16_t stat);
int user_packet(struct packet *inpack, struct packet *outpack, int sockfd);
int passwd_verify(int uin, char *passwd);
