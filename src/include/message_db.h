int message_db_init();
void message_db_close();
int message_store(int from, int to, int type, char *msg);
int message_get(int uin, char *buff, int *left);
int message_delete(int uin);
