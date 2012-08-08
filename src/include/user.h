#include "protocol.h"

int user_packet(struct packet *inpack, struct packet *outpack);
int passwd_verify(int uin, char *passwd);
