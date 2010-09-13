#ifndef __LOG_H__
#define __LOG_H__

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

enum log_level {
	QUIET,
	ERR,
	INFO,
	NOTICE,
	WARNING,
	DEBUG
};


extern int current_log_level;

#define LOG_LINE_BUFFEER_SIZE	2048

void log_printf(enum log_level level, const char *format, ...);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#endif
