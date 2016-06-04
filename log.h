#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

typedef enum {
  LOG_SCREEN = 0x1,
  LOG_FILE   = 0x2
} log_t;

#define LOG(TARGET, ...)                            \
  do {                                              \
    if (TARGET & LOG_SCREEN)                        \
      {                                             \
        printf (__VA_ARGS__);                       \
        putchar ('\n');                             \
      }                                             \
    if (TARGET & LOG_FILE)                          \
      {                                             \
        fprintf (current_log_file, __VA_ARGS__);    \
        fputc ('\n', current_log_file);             \
      }                                             \
  } while (0)

int open_log_file (const char *env_filename);
void log_map (void);
void close_log_file (void);

extern FILE *current_log_file;

#endif /* ! defined(__LOG_H__) */

