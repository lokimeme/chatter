#pragma once
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE 1024
#define USERNAME_MAX 32
#define BACKLOG 64

static inline void die(const char *msg) {
  perror(msg);
  exit(1);
}

static inline void trim_newline(char *s) {
  if (!s)
    return;
  size_t n = strlen(s);
  while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
    s[--n] = '\0';
  }
}

static inline void rstrip(char *s) {
  if (!s)
    return;
  size_t n = strlen(s);
  while (n && isspace((unsigned char)s[n - 1])) {
    s[--n] = '\0';
  }
}

static inline void lstrip(char *s) {
  if (!s)
    return;
  size_t i = 0, n = strlen(s);
  while (i < n && isspace((unsigned char)s[i]))
    i++;
  if (i)
    memmove(s, s + i, n - i + 1);
}

static inline void strip(char *s) {
  lstrip(s);
  rstrip(s);
}

static inline int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// send_all: write all bytes; return 0 on success, -1 on error
static inline int send_all(int fd, const char *buf, size_t len) {
  size_t off = 0;
  while (off < len) {
    ssize_t n = send(fd, buf + off, len - off, 0);
      if (n < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
      return -1;
    }
    if (n == 0)
      return -1;
    off += (size_t)n;
  }
  return 0;
}

// sendf: printf-style send
static inline int sendf(int fd, const char *fmt, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n < 0)
    return -1;
  size_t len = (size_t)n;
  if (len >= sizeof(buf))
    len = sizeof(buf) - 1;
  return send_all(fd, buf, len);
}

// recv_line: read until '\n' (inclusive) into buf (cap bytes). Return bytesread
// or -1 on error, 0 on EOF. Non-blocking friendly: may return -1 with EAGAIN;
// caller should check errno.
static inline ssize_t recv_line(int fd, char *buf, size_t cap) {
  static char tmp[4096];
  static size_t tlen = 0;
  size_t i = 0;
  while (1) {
    for (; i < tlen; ++i)
      if (tmp[i] == '\n')
        break;
    if (i < tlen) {
      size_t n = i + 1;
      if (n > cap - 1)
        n = cap - 1;
      memcpy(buf, tmp, n);
      buf[n] = '\0';
      memmove(tmp, tmp + i + 1, tlen - (i + 1));
      tlen -= (i + 1);
      return (ssize_t)n;
    }
    ssize_t r = recv(fd, tmp + tlen, sizeof(tmp) - tlen, 0);
    if (r < 0) {
      if (errno == EINTR)
        continue;
        return -1;
    }
    if (r == 0) {
      if (tlen) {
        size_t n = tlen > cap - 1 ? cap - 1 : tlen;
        memcpy(buf, tmp, n);
        buf[n] = '\0';
        tlen = 0;
        return (ssize_t)n;
      }
      return 0;
    }
    tlen += (size_t)r;
    if (tlen >= sizeof(tmp)) {
      size_t n = sizeof(tmp) < cap - 1 ? sizeof(tmp) : cap - 1;
      memcpy(buf, tmp, n);
      buf[n] = '\0';
      tlen = 0;
      return (ssize_t)n;
    }
  }
}
