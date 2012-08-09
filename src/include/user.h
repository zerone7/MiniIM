#include "protocol.h"

#define PARAM_PASSLEN(x)    (uint16_t *)x->params 
#define PARAM_PASSWD(x)     (char *)(x->params + 2) 
#define PARAM_NICKLEN(x)    (uint16_t *)x->params
#define PARAM_NICK(x)       (char *)(x->params + 2)

int user_packet(struct packet *inpack, struct packet *outpack);
int passwd_verify(int uin, char *passwd);
