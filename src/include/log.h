#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#define EMERG_LOG_LEVEL		0
#define ALERT_LOG_LEVEL		1
#define CRIT_LOG_LEVEL		2
#define ERR_LOG_LEVEL		3
#define WARNING_LOG_LEVEL	4
#define NOTICE_LOG_LEVEL	5
#define INFO_LOG_LEVEL		6
#define DEBUG_LOG_LEVEL		7
/* static default log level, can ONLY config at compile time */
#define DEFAULT_LOG_LEVEL	DEBUG_LOG_LEVEL

/* initialize logger, open log file */
#define LOG_INIT(log_file)						\
do {									\
	/* TODO: need to change to log to file */			\
	log_fp = stdout;						\
	/*log_fp = fopen(log_file, "w");*/				\
	if (!log_fp) {							\
		fprintf(stderr, "open log file %s failed\n", log_file);	\
		log_fp = stdout;					\
	}								\
} while (0)

/* destroy logger, close log file */
#define LOG_DESTROY()							\
do {									\
	if (log_fp != stdout) {						\
		fclose(log_fp);						\
	}								\
} while (0)

extern FILE *log_fp;

/* generic log function used by log_* */
static inline void va_log(const char *level_str, const char *fmt, va_list args)
{
	static uint32_t count = 0;
	time_t rawtime;
	struct tm *timeinfo;
	char buffer[64];

	if (!log_fp) {
		log_fp = stderr;
	}

	/* generate log time and level string */
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	memset(buffer, 0, sizeof(buffer));
	strftime(buffer, sizeof(buffer), "[%Y-%m-%d %X", timeinfo);
	sprintf(buffer + strlen(buffer), " %s]", level_str);
	fwrite(buffer, 1, strlen(buffer), log_fp);

	/* output log message after log time and level string */
	vfprintf(log_fp, fmt, args);
#if DEFAULT_LOG_LEVEL > NOTICE_LOG_LEVEL
	fflush(log_fp);
#else
	if (count % 5 == 0) {
		fflush(log_fp);
	}
#endif
	count++;
}

static inline void log_emerg(const char *fmt, ...)
{
#if !(EMERG_LOG_LEVEL > DEFAULT_LOG_LEVEL)
	va_list args;
	va_start(args, fmt);
	va_log("EMERG", fmt, args);
	va_end(args);
#endif
}

static inline void log_alert(const char *fmt, ...)
{
#if !(ALERT_LOG_LEVEL > DEFAULT_LOG_LEVEL)
	va_list args;
	va_start(args, fmt);
	va_log("ALERT", fmt, args);
	va_end(args);
#endif
}

static inline void log_crit(const char *fmt, ...)
{
#if !(CRIT_LOG_LEVEL > DEFAULT_LOG_LEVEL)
	va_list args;
	va_start(args, fmt);
	va_log("CRIT", fmt, args);
	va_end(args);
#endif
}

static inline void log_err(const char *fmt, ...)
{
#if !(ERR_LOG_LEVEL > DEFAULT_LOG_LEVEL)
	va_list args;
	va_start(args, fmt);
	va_log("ERR", fmt, args);
	va_end(args);
#endif
}

static inline void log_warning(const char *fmt, ...)
{
#if !(WARNING_LOG_LEVEL > DEFAULT_LOG_LEVEL)
	va_list args;
	va_start(args, fmt);
	va_log("WARNING", fmt, args);
	va_end(args);
#endif
}

static inline void log_notice(const char *fmt, ...)
{
#if !(NOTICE_LOG_LEVEL > DEFAULT_LOG_LEVEL)
	va_list args;
	va_start(args, fmt);
	va_log("NOTICE", fmt, args);
	va_end(args);
#endif
}

static inline void log_info(const char *fmt, ...)
{
#if !(INFO_LOG_LEVEL > DEFAULT_LOG_LEVEL)
	va_list args;
	va_start(args, fmt);
	va_log("INFO", fmt, args);
	va_end(args);
#endif
}

static inline void log_debug(const char *fmt, ...)
{
#if !(DEBUG_LOG_LEVEL > DEFAULT_LOG_LEVEL)
	va_list args;
	va_start(args, fmt);
	va_log("DEBUG", fmt, args);
	va_end(args);
#endif
}

#endif
