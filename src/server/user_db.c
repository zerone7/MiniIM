#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mysql/mysql.h>

#include "user_db.h"

static MYSQL       mysql;
static MYSQL_RES   *result;
static MYSQL_ROW   row;

int user_db_init()
{
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, "localhost","root","rooter","user",0,NULL,0))
    {
        printf("connection error: %s\n", mysql_error(&mysql));
        return -1;
    }

    return 0;
}

void user_db_close()
{
    //mysql_free_result(result);
    mysql_close(&mysql);
}

int  user_get_passwd(int uin , char *passwd)
{
    int ret;
    char query_str[100];

    sprintf(query_str,"select password from user where uin = %d", uin); 
    ret = mysql_query(&mysql, query_str);
    if(ret)
    {
        printf("Query Error: %s\n", mysql_error(&mysql));
        return -1;
    }

    result = mysql_use_result(&mysql);
    if(result)
    {
    //    rows = mysql_num_rows(result);
    //    printf("%d rows!\n", rows);
        row = mysql_fetch_row(result);
        if(row)
        {
            sprintf(passwd, "%s", row[0]);
            mysql_free_result(result);
            return 1;
        }
        else
        {
            mysql_free_result(result);
            return 0;
        }
    }
    else
    {
        printf("Use_result Error: %s\n", mysql_error(&mysql));
        return -1;
    }
}

int  user_set_passwd(int uin , char *passwd)
{
    int ret;
    char query_str[100];

    sprintf(query_str,"update user set password = '%s' where uin = %d", passwd, uin); 
    ret = mysql_query(&mysql, query_str);
    if(ret)
    {
        printf("update Error: %s\n", mysql_error(&mysql));
        return -1;
    }

    return 0;
}

int user_get_nick(int uin, char *nick)
{
    int ret;
    char query_str[100];

    sprintf(query_str,"select nick from user where uin = %d", uin); 
    ret = mysql_query(&mysql, query_str);
    if(ret)
    {
        printf("Query Error: %s\n", mysql_error(&mysql));
        return -1;
    }

    result = mysql_use_result(&mysql);
    if(result)
    {
    //    rows = mysql_num_rows(result);
    //    printf("%d rows!\n", rows);
        row = mysql_fetch_row(result);
        if(row)
        {
            sprintf(nick, "%s", row[0]);
            mysql_free_result(result);
            return 1;
        }
        else
        {
            mysql_free_result(result);
            return 0;
        }
    }
    else
    {
        printf("Use_result Error: %s\n", mysql_error(&mysql));
        return -1;
    }
}

int user_set_nick(int uin, char *nick)
{
    int ret;
    char query_str[100];

    sprintf(query_str,"update user set nick = '%s' where uin = %d", nick, uin); 
    ret = mysql_query(&mysql, query_str);
    if(ret)
    {
        printf("Query Error: %s\n", mysql_error(&mysql));
        return -1;
    }

    return 0;
}

int user_friend_add(int uin)
{
    char query_str[100];

    sprintf(query_str,"update user set contact_count = contact_count+1 where uin = %d", uin); 
    if(mysql_query(&mysql, query_str))
    {
        printf("Query Error: %s\n", mysql_error(&mysql));
        return -1;
    }

    return 0;
}
