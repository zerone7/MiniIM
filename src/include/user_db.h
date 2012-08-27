int user_db_init();
void user_db_close();
int user_get_passwd(int uin, unsigned char *passwd);
int user_set_passwd(int uin, unsigned char *passwd);
int user_get_nick(int uin, char *nick);
int user_set_nick(int uin, char *nick);
int user_friend_add(int uin);
int user_add(char *nick, unsigned char *passwd);
