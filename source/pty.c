#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "pty.h"

#ifdef SP_LINUX

#include <stdio.h>
#include <pty.h>
#include <sys/wait.h>
#include <unistd.h>

void strip_ansi(char* buf, ssize_t* len) {
  char* out = buf;
  char* in = buf;
  char* end = buf + *len;

  while (in < end) {
    if (*in == '\033' && in + 1 < end && *(in + 1) == '[') {
      in += 2;
      while (in < end && !(*in >= 'A' && *in <= 'z')) {
        in++;
      }
      if (in < end) {
        in++;
      }
    } else {
      *out++ = *in++;
    }
  }

  *len = out - buf;
}

s32 spn_pty_wrap(s32 num_args, const c8** args) {
  if (num_args < 3) {
    fprintf(stderr, "usage: spn --pty-wrap <command> [args...]\n");
    return 1;
  }

  int master;
  pid_t pid = forkpty(&master, NULL, NULL, NULL);

  if (pid < 0) {
    perror("forkpty");
    return 1;
  }

  if (pid == 0) {
    execvp(args[2], (char* const*)&args[2]);
    _exit(127);
  }

  char buf[4096];
  ssize_t n;
  while ((n = read(master, buf, sizeof(buf))) > 0) {
    strip_ansi(buf, &n);
    write(STDOUT_FILENO, buf, n);
  }

  int status;
  waitpid(pid, &status, 0);
  close(master);

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }

  return 1;
}

#endif
