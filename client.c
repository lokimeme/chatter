#include "util.h"

static int connect_to_host(const char *host, const char *port) {
  struct addrinfo hints, *res, *p;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  int rv = getaddrinfo(host, port, &hints, &res);
  if (rv != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return -1;
  }
  int fd = -1;
  for (p = res; p; p = p->ai_next) {
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0)
      continue;
    if (connect(fd, p->ai_addr, p->ai_addrlen) == 0)
      break;
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  return fd;
}

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: %s <host> <port> <username>\n", argv[0]);
    return 1;
  }
  const char *host = argv[1];
  const char *port = argv[2];
  const char *username = argv[3];
  int fd = connect_to_host(host, port);
  if (fd < 0)
    die("connect");
  char buf[MAX_LINE];
  ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
  if (n > 0) {
    buf[n] = '\0';
    fputs(buf, stdout);
  }
  sendf(fd, "%s\n", username);
  printf("Connected as '%s'. Type messages or commands (/who, /msg, /nick, /quit).\n ", username);
      for (;;) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    FD_SET(STDIN_FILENO, &rfds);
    int mx = fd > STDIN_FILENO ? fd : STDIN_FILENO;
    int r = select(mx + 1, &rfds, NULL, NULL, NULL);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      die("select");
    }
    if (FD_ISSET(fd, &rfds)) {
      char line[MAX_LINE];
      ssize_t k = recv(fd, line, sizeof(line) - 1, 0);
      if (k <= 0) {
        printf("Disconnected from server.\n");
        break;
      }
      line[k] = '\0';
      fputs(line, stdout);
      fflush(stdout);
    }
    if (FD_ISSET(STDIN_FILENO, &rfds)) {
      char line[MAX_LINE];
      if (!fgets(line, sizeof(line), stdin)) {
        // EOF => quit
        sendf(fd, "/quit\n");
        break;
      }
      if (strncmp(line, "/quit", 5) == 0) {
        sendf(fd, "/quit\n");
        break;
      }
      send_all(fd, line, strlen(line));
    }
  }
  close(fd);
  return 0;
}
