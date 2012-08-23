#ifndef _PACKET_DUMP_H_
#define _PACKET_DUMP_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "log.h"
#include "list_packet.h"

#if DEFAULT_LOG_LEVEL == DEBUG_LOG_LEVEL
#define DUMP_INIT(dump_file)							\
do {										\
	char buf[128];								\
	sprintf(buf, "%s-%u", dump_file, getpid());				\
	dump_fp = fopen(buf, "w");						\
	if (!dump_fp) {								\
		fprintf(stderr, "open dump file %s failed\n", dump_file);	\
		dump_fp = stderr;						\
	}									\
} while (0)
#define DUMP_DESTROY()								\
do {										\
	if (dump_fp && dump_fp != stdout && dump_fp != stderr) {		\
		fclose(dump_fp);						\
	}									\
} while (0)
#else
#define DUMP_INIT(dump_file)	do { } while (0)
#define DUMP_DESTROY()		do { } while (0)
#endif

#define SEND_PACKET	1
#define RECV_PACKET	2

extern FILE *dump_fp;

static inline void dump_packet(struct list_packet *lp, int type, char *str)
{
#if DEFAULT_LOG_LEVEL != DEBUG_LOG_LEVEL
	return;
#else
	if (!dump_fp) {
		return;
	}

	time_t rawtime;
	struct tm *timeinfo;
	char buffer[64];
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	memset(buffer, 0, sizeof(buffer));
	strftime(buffer, sizeof(buffer), "[%Y-%m-%d %X]", timeinfo);
	fprintf(dump_fp, "%s\n", buffer);

	int length = get_length(lp);
	if (type == SEND_PACKET) {
		fprintf(dump_fp, "send packet len %hu, cmd 0x%04hx, uin %u to %s:\n",
				length, get_command(lp), get_uin(lp), str);
	} else {
		fprintf(dump_fp, "recv packet len %hu, cmd 0x%04hx, uin %u from %s:\n",
				length, get_command(lp), get_uin(lp), str);
	}

	fprintf(dump_fp, "header :");
	int i;
	char *p = (char *)&lp->packet;
	for (i = 0; i < PACKET_HEADER_LEN; i++) {
		fprintf(dump_fp, "%02hhx ", *p++);
	}

	if (length > PACKET_HEADER_LEN) {
		fprintf(dump_fp, "\nparams: ");
	}

	for (i = PACKET_HEADER_LEN; i < length; i++) {
		fprintf(dump_fp, "%02hhx ", *p++);
	}

	fprintf(dump_fp, "\n\n");
	fflush(dump_fp);
#endif
}

#endif
