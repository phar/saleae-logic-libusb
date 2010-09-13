#include "log.h"

int current_log_level = 0;

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>





void log_printf(enum log_level level, const char *format, ...){
char p[LOG_LINE_BUFFEER_SIZE];
va_list ap;


	if(current_log_level >= level){ 
		va_start(ap, format);
		(void)vsnprintf(p, sizeof(p), format, ap);
		va_end(ap);
		fprintf(stderr, "%s", p);
	}
}
