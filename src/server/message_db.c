#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mysql/mysql.h>

#include "message_db.h"
#include "protocol.h"

#define MSG_FROM_UIN_OFFSET     0
#define MSG_TIMESTAMP_OFFSET    4
#define MSG_TYPE_OFFSET         8
#define MSG_LENGTH_OFFSET       10
#define MSG_CONTENT_OFFSET      12

#define PARAM_HEADER_LEN        12

MYSQL       mysql;
MYSQL_RES   *result;
MYSQL_ROW   row;
int         rows;

int message_db_init()
{
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, "localhost","root","rooter","offline_msg",0,NULL,0))
    {
        printf("connection error: %s\n", mysql_error(&mysql));
        return -1;
    }

    return 0;
}

void message_db_close()
{
    //mysql_free_result(result);
    mysql_close(&mysql);
}

int  message_get(int uin, char *buff, int *pleft)
{
    uint16_t *pcount, *ptype, *plen;
    uint32_t *from_uin;
    char *content;
    int i;
    unsigned long *lengths;
    char query_str[100];

    /* 如果uin不为0，则表示是一次新的查询; 为0，则表示是上一次查询的继续 */
    if(uin) 
    {
        sprintf(query_str,"select from_uin, type, message from offline_msg where to_uin = %d", uin); 

        if(mysql_query(&mysql, query_str))
        {
            printf("Query Error: %s\n", mysql_error(&mysql));
            return -1;
        }

        result = mysql_store_result(&mysql);
        if(result)
        {
            rows = mysql_num_rows(result);
            printf("There are %d rows in query result!\n", rows);
        }
        else
        {
            rows = 0;
            printf("Use_result Error: %s\n", mysql_error(&mysql));
            return -1;
        }
    }

    /* COUNT 字段 */
    pcount = (uint16_t *)buff;
    *pcount = 0;
    buff += 2;
    *pleft -= 2;

    while(rows)
    {
        row = mysql_fetch_row(result);
        lengths = mysql_fetch_lengths(result);
        if(*pleft < lengths[2]+1+PARAM_HEADER_LEN)
        {
            printf("buff is gonna be full: left %d, msg_len %lu\n", *pleft, lengths[2]+12);
            break;
        }

        if(row)
        {
            /* FROM_UIN 字段 */
            from_uin = (uint32_t *) (buff + MSG_FROM_UIN_OFFSET);
            *from_uin = atoi(row[0]);

            /* TYPE 字段 */
            ptype = (uint16_t *) (buff + MSG_TYPE_OFFSET);
            *ptype = atoi(row[1]);

            /* LENGTH 字段 */
            plen = (uint16_t *) (buff + MSG_LENGTH_OFFSET);
            *plen = *ptype == MSG_TYPE_CHAT ? lengths[2]+1: 0;

            /* CONTENT 字段 */
            if(*ptype == MSG_TYPE_CHAT)
            {
                content = (char *) (buff + MSG_CONTENT_OFFSET);
                strcpy(content, row[2]);
            }
            printf("Message: from %d, type %d, len %d, content: %s\n", *from_uin, *ptype, *plen, content);
        }
        else
        {
            printf("message_get: fetch row error\n");
            rows = 0;
            mysql_free_result(result);
            return -1;
        }

        (*pcount)++;
        buff += *plen + PARAM_HEADER_LEN;
        *pleft -= *plen + PARAM_HEADER_LEN;
        rows--;
    }

    if(rows == 0)
        mysql_free_result(result);

    return rows;
}

int  message_store(int from, int to, int type, char *msg)
{
    char query_str[1000];

    sprintf(query_str,"insert into offline_msg set from_uin=%d, to_uin=%d, type=%d, message='%s'", \
            from, to, type, msg); 

    if(!mysql_query(&mysql, query_str))
    {
        printf("Inserted %lu rows\n", (unsigned long)mysql_affected_rows(&mysql));
        return 0;
    }
    else
    {
        printf("Insert Error: %s\n", mysql_error(&mysql));
        return -1;
    }
}

int message_delete(int uin)
{
    char query_str[100];
    sprintf(query_str,"delete from offline_msg where to_uin = %d", uin); 
    if(!mysql_query(&mysql, query_str))
    {
        printf("Deleted %lu rows\n", (unsigned long)mysql_affected_rows(&mysql));
        return 0;
    }
    else
    {
        printf("Deleted Error: %s\n", mysql_error(&mysql));
        return -1;
    }
}
