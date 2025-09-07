#include "util.h"
#include "hashtable.h"
typedef struct {
  int fd;
  char username[USERNAME_MAX];
  int active;
} Client;
typedef struct {
  int listen_fd;
  fd_set master;
  int fdmax;
  Client *clients;
  size_t cap;
  HashTable *users; // username -> fd
} Server;
static void add_client(Server *S, int fd) {
  // expand array if needed
  for (size_t i = 0; i < S->cap; ++i) {
    if (!S->clients[i].active) {
      S->clients[i].fd = fd;
      S->clients[i].username[0] = '\0';
      S->clients[i].active = 1;
      FD_SET(fd, &S->master);
      if (fd > S->fdmax)
        S->fdmax = fd;
      return;
    }
  }
  // grow
  size_t old = S->cap;
  size_t ncap = old ? old * 2 : 64;
  S->clients = realloc(S->clients, ncap * sizeof(Client));
  for (size_t i = old; i < ncap; ++i)
    S->clients[i].active = 0;
  S->cap = ncap;
  S->clients[old].fd = fd;
  S->clients[old].username[0] = '\0';
  S->clients[old].active = 1;
  FD_SET(fd, &S->master);
  if (fd > S->fdmax)
    S->fdmax = fd;
}
static Client *find_client_by_fd(Server *S, int fd) {
  for (size_t i = 0; i < S->cap; ++i) {
    if (S->clients[i].active && S->clients[i].fd == fd)
      return &S->clients[i];
  }
  return NULL;
}
static Client *find_client_by_name(Server *S, const char *name) {
  int fd = ht_get(S->users, name);
  if (fd < 0)
    return NULL;
  return find_client_by_fd(S, fd);
}
static void remove_client(Server *S, int fd, const char *reason) {
  Client *c = find_client_by_fd(S, fd);
  if (!c)
    return;
  if (c->username[0]) {
    ht_remove(S->users, c->username);
    sendf(fd, "BYE %s\r\n", reason ? reason : "goodbye");
    char out[MAX_LINE];
    snprintf(out, sizeof(out), "* %s left: %s\n", c->username,
             reason ? reason : "disconnected");
    // broadcast to others
    for (size_t i = 0; i < S->cap; ++i) {
      if (S->clients[i].active && S->clients[i].fd != fd &&
          S->clients[i].username[0]) {
        send_all(S->clients[i].fd, out, strlen(out));
      }
    }
  }
  close(fd);
  FD_CLR(fd, &S->master);
  c->active = 0;
  c->username[0] = '\0';
  c->fd = -1;
}
static void broadcast(Server *S, int from_fd, const char *msg) {
  Client *from = find_client_by_fd(S, from_fd);
  char line[MAX_LINE];
  snprintf(line, sizeof(line), "[%s] %s\n",
           from && from->username[0] ? from->username : "?", msg);
  for (size_t i = 0; i < S->cap; ++i) {
    if (!S->clients[i].active)
      continue;
    int fd = S->clients[i].fd;
    if (fd == from_fd)
      continue;
    if (!S->clients[i].username[0])
      continue;
    send_all(fd, line, strlen(line));
  }
}
static void send_user_list(Server *S, int fd) {
  sendf(fd, "* Online users:\n");
  void cb(const char *key, int value, void *user) {
    (void)user;
    (void)value;
    sendf(fd, " - %s\n", key);
  }
  ht_each(S->users, cb, NULL);
}
static int is_valid_username(const char *u) {
  size_t n = strlen(u);
  if (n == 0 || n >= USERNAME_MAX)
    return 0;
  for (size_t i = 0; i < n; ++i) {
    unsigned char ch = (unsigned char)u[i];
    if (!(isalnum(ch) || ch == '_' || ch == '-'))
      return 0;
  }
  return 1;
}
static int change_username(Server *S, Client *c, const char *newname) {
  if (!is_valid_username(newname)) {
    sendf(c->fd, "! invalid username. use [A-Za-z0-9_-], max %d chars\n",
          USERNAME_MAX - 1);
    return -1;
  }
  if (ht_get(S->users, newname) >= 0) {
    sendf(c->fd, "! username taken\n");
    return -1;
  }
  if (c->username[0])
    ht_remove(S->users, c->username);
  strncpy(c->username, newname, USERNAME_MAX - 1);
  c->username[USERNAME_MAX - 1] = '\0';
  ht_put(S->users, c->username, c->fd);
  return 0;
}
static void handle_command(Server *S, Client *c, char *line) {
  // line without trailing newline
  if (strncmp(line, "/who", 4) == 0) {
    send_user_list(S, c->fd);
  } else if (strncmp(line, "/quit", 5) == 0) {
    remove_client(S, c->fd, "quit");
  } else if (strncmp(line, "/nick ", 6) == 0) {
    char *newname = line + 6;
    strip(newname);
    if (change_username(S, c, newname) == 0) {
      sendf(c->fd, "* you are now '%s'\n", c->username);
    }
  } else if (strncmp(line, "/msg ", 5) == 0) {
    char *p = line + 5;
    char *user = strtok(p, " \t");
    char *text = strtok(NULL, "");
    if (!user || !text) {
      sendf(c->fd, "! usage: /msg <user> <text>\n");
      return;
    }
    Client *dst = find_client_by_name(S, user);
    if (!dst) {
      sendf(c->fd, "! user not found: %s\n", user);
      return;
    }
    sendf(dst->fd, "[pm from %s] %s\n", c->username[0] ? c->username : "?",
          text);
  } else {
    sendf(c->fd, "! unknown command\n");
  }
}
static void handle_client_line(Server *S, int fd, char *line) {
  trim_newline(line);
  strip(line);
  Client *c = find_client_by_fd(S, fd);
  if (!c)
    return;
  if (c->username[0] == '\0') {
    // First line must be username
    if (change_username(S, c, line) == 0) {
      sendf(fd, "* welcome, %s!\n", c->username);
      char out[MAX_LINE];
      snprintf(out, sizeof(out), "* %s joined\n", c->username);
      for (size_t i = 0; i < S->cap; ++i) {
        if (S->clients[i].active && S->clients[i].fd != fd &&
            S->clients[i].username[0]) {
          send_all(S->clients[i].fd, out, strlen(out));
        }
      }
    } else {
      sendf(fd, "! invalid/taken username. try again:\n");
    }
    return;
  }
  if (line[0] == '/') {
    handle_command(S, c, line);
  } else if (line[0]) {
    broadcast(S, fd, line);
  }
}
static int setup_listener(const char *port) {
  struct addrinfo hints, *res, *p;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  int rv = getaddrinfo(NULL, port, &hints, &res);
  if (rv != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return -1;
  }
  int listen_fd = -1;
  for (p = res; p; p = p->ai_next) {
    listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (listen_fd < 0)
      continue;
    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (bind(listen_fd, p->ai_addr, p->ai_addrlen) < 0) {
      close(listen_fd);
      listen_fd = -1;
      continue;
    }
    if (listen(listen_fd, BACKLOG) < 0) {
      close(listen_fd);
      listen_fd = -1;
      continue;
    }
    break;
  }
  freeaddrinfo(res);
  return listen_fd;
}
int main(int argc, char **argv) {
  const char *port = (argc > 1) ? argv[1] : "5555";
  int listen_fd = setup_listener(port);
  if (listen_fd < 0)
    die("listen socket");
  Server S = {0};
  S.listen_fd = listen_fd;
  S.fdmax = listen_fd;
  S.cap = 64;
  S.clients = calloc(S.cap, sizeof(Client));
  if (!S.clients)
    die("calloc clients");
  S.users = ht_create(257);
  if (!S.users)
    die("ht_create");
  FD_ZERO(&S.master);
  FD_SET(listen_fd, &S.master);
  printf("Chat server listening on port %s\n", port);
  for (;;) {
    fd_set readfds = S.master;
    int nready = select(S.fdmax + 1, &readfds, NULL, NULL, NULL);
    if (nready < 0) {
      if (errno == EINTR)
        continue;
      die("select");
    }
    for (int fd = 0; fd <= S.fdmax; ++fd) {
      if (!FD_ISSET(fd, &readfds))
        continue;
      if (fd == S.listen_fd) {
        // accept
        struct sockaddr_storage ss;
        socklen_t slen = sizeof ss;
        int cfd = accept(S.listen_fd, (struct sockaddr *)&ss, &slen);
        if (cfd < 0)
          continue;
        set_nonblocking(cfd);
        add_client(&S, cfd);
        sendf(cfd, "Enter username: \n");
      } else {
        char buf[MAX_LINE];
        ssize_t r = recv_line(fd, buf, sizeof(buf));
        if (r <= 0) {
          remove_client(&S, fd, "disconnect");
        } else {
          handle_client_line(&S, fd, buf);
        }
      }
    }
  }
  // Unreachable normally
  close(S.listen_fd);
  ht_free(S.users);
  free(S.clients);
  return 0;
}
