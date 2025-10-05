#ifndef GENERAL_H
#define GENERAL_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK_ERROR(condition, message)                                        \
  if (condition) {                                                             \
    perror(message);                                                           \
    exit(EXIT_FAILURE);                                                        \
  }

#endif