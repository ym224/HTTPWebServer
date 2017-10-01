#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void log_init(char *file);
void log_write(char *fmt, ...);
void log_close();

#endif