#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <mysql/mysql.h>

#include "friend_db.h"

static MYSQL       user, contact;
static MYSQL_RES   *result;
static MYSQL_ROW   row;

/* init contact & user database connection */
int friend_db_init()
{
    mysql_init(&user);
    mysql_init(&contact);

    if (!mysql_real_connect(&user, "localhost", "im_user", "im_user_pass", \
                "user", 0, NULL, 0)) {
        printf("connection error: %s\n", mysql_error(&user));
        return -1;
    }

    if (!mysql_real_connect(&contact, "localhost", "im_user", "im_user_pass", \
                "contact", 0, NULL, 0)) {
        printf("connection error: %s\n", mysql_error(&contact));
        return -1;
    }

    return 0;
}

/* close database connection */
void friend_db_close()
{
    //mysql_free_result(result);
    mysql_close(&user);
    mysql_close(&contact);
}

/* get user friend count */
int get_friend_num(int uin)
{
    int num;
    char query_str[100];

    sprintf(query_str,"select contact_count from user where uin = %d", uin); 
    if (mysql_query(&user, query_str)) {
        printf("Query Error: %s\n", mysql_error(&user));
        return -1;
    }

    result = mysql_use_result(&user);
    if (result) {
        row = mysql_fetch_row(result);
        if (row) {
            num = atoi(row[0]);
            mysql_free_result(result);
            return num;
        } else {
            mysql_free_result(result);
            return 0;
        }
    } else {
        printf("Use_result Error: %s\n", mysql_error(&user));
        return -1;
    }
}

/* get user friend list */
int  get_friend_list(int uin, int num, uint32_t *friends)
{
    int table, i;
    char query_str[100];

    assert(friends);
    if (num <= 50 && num > 0)
        table = 1;
    else if (num <= 100)
        table = 2;
    else if (num <= 200)
        table = 3;
    else if (num <= 500)
        table = 4;
    else if (num <= 999)
        table = 5;
    else
        return -1;

    sprintf(query_str, "select * from contact_%d where uin = %d", table, uin);
    if (mysql_query(&contact, query_str)) {
        printf("Query Error: %s\n", mysql_error(&contact));
        return -1;
    }

    result = mysql_use_result(&contact);
    if (result) {
        row = mysql_fetch_row(result);
        if (row) {
            for (i = 0; i < num; i++)
                friends[i] = atoi(row[i+1]);
            mysql_free_result(result);
            return 0;
        } else {
            mysql_free_result(result);
            return -1;
        }
    } else {
        printf("Use_result Error: %s\n", mysql_error(&contact));
        return -1;
    }
}

/* get user friend nickname */
int get_friend_nick(uint32_t uin, uint16_t *plen, char *nick)
{
    char query_str[100];

    assert(plen && nick);
    sprintf(query_str,"select nick from user where uin = %d", uin); 
    if (mysql_query(&user, query_str)) {
        printf("Query Error: %s\n", mysql_error(&user));
        return -1;
    }

    result = mysql_use_result(&user);
    if (result) {
        row = mysql_fetch_row(result);
        if (row) {
            strcpy(nick, row[0]);
            *plen = (uint16_t)strlen(nick) + 1;
            mysql_free_result(result);
            return 1;
        } else {
            *plen = 0;
            mysql_free_result(result);
            return 0;
        }
    } else {
        *plen = 0;
        printf("Use_result Error: %s\n", mysql_error(&user));
        return -1;
    }
}

/* add friend to user's friend list */
int friend_add_contact(int uin, int friend)
{
    int table, i, num;
    char query_str[100];

    num = get_friend_num(uin);
    if (num == 0) {
        sprintf(query_str,"insert into contact_1 set uin=%d, contact_1=%d", uin, friend);

        if (!mysql_query(&contact, query_str)) {
            printf("Inserted %lu rows\n", (unsigned long)mysql_affected_rows(&contact));
            return 0;
        } else {
            printf("Insert Error: %s\n", mysql_error(&contact));
            return -1;
        }
    } else if (num == 50 || num == 100 || num == 200 || num == 500) {
        if (friend_table_move(uin, friend, num))
            return -1;
    }
    else if (num < 50 && num > 0)
        table = 1;
    else if (num < 100)
        table = 2;
    else if (num < 200)
        table = 3;
    else if (num < 500)
        table = 4;
    else if (num < 999)
        table = 5;
    else
        return -1;

    sprintf(query_str,"update contact_%d set contact_%d = %d where uin = %d", table, num+1, friend, uin);
    if (mysql_query(&contact, query_str)) {
        printf("Update error\n");
        return -1;
    }

    return 0;
}

/* move friend list data from one table to a larger table */
int friend_table_move(int uin, int friend, int num)
{
    uint32_t friends[num], table, i;
    char query_str[5000];
    char tmp_str[50];

    if (num == 50)
        table = 2;
    else if (num == 100)
        table = 3;
    else if (num == 200)
        table = 4;
    else if (num == 500)
        table = 5;
    else
        return -1;
    
    get_friend_list(uin, num, friends);
    sprintf(query_str, "insert into contact_%d set uin=%d", table, uin);
    for (i = 0; i < num; i++) {
        sprintf(tmp_str, ",contact_%d=%d", i+1, friends[i]); 
        strcat(query_str, tmp_str);
    }

    if (!mysql_query(&contact, query_str)) {
        printf("Inserted %lu rows\n", (unsigned long)mysql_affected_rows(&contact));
        friend_delete(uin, table-1);
        return 0;
    } else {
        printf("Insert Error: %s\n", mysql_error(&contact));
        return -1;
    }
}

/* delete friend from friend list */
int friend_delete(int uin, int table)
{
    char query_str[100];

    sprintf(query_str,"delete from contact_%d where uin = %d", table, uin); 
    if (!mysql_query(&contact, query_str)) {
        printf("Deleted %lu rows\n", (unsigned long)mysql_affected_rows(&contact));
        return 0;
    } else {
        printf("Deleted Error: %s\n", mysql_error(&contact));
        return -1;
    }
}
