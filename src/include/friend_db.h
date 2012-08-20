int friend_db_init();
void friend_db_close();
int get_friend_num(int uin);
int friend_check_uin(int uin);
int get_friend_list(int uin, int num, uint32_t *friends);
int get_friend_nick(uint32_t uin, uint16_t *plen, char *nick);
int friend_add_contact(int uin, int friend);
