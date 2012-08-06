#include <time.h>
#include <string.h>
#include <stdio.h>
#include "conn_log.h"

/* global log file pointer */
FILE *log_fp = NULL;

/* generic log function used by log_* */
void va_log(const char *level_str, const char *fmt, va_list args)
{
	time_t rawtime;
	struct tm *timeinfo;
	char buffer[64];

	/* generate log time and level string */
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	memset(buffer, 0, sizeof(buffer));
	strftime(buffer, sizeof(buffer), "[%Y-%m-%d %X", timeinfo);
	sprintf(buffer + strlen(buffer), " %s]", level_str);
	fwrite(buffer, 1, strlen(buffer), log_fp);

	/* output log message after log time and level string */
	vfprintf(log_fp, fmt, args);
}
