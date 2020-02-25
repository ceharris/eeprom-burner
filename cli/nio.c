/*
 * nio.c *
 * network port routines
 *
 */ 
#include <stdio.h>

#include "nio.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

int nio_init(nio_s *nio)
{
  nio->fd = 0;
  nio->info.host = "localhost";
  nio->info.port = "5331";
  nio->info.timeout = 1;
  return 0;
}

void nio_cleanup(nio_s *nio)
{
  if (nio_isopen(nio))
    nio_close(nio);
}

int nio_open(nio_s *nio)
{
  int fd;
  struct addrinfo hints;
  struct addrinfo* addrs = NULL;

  if (nio_isopen(nio))
    return -1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  
  int rc = getaddrinfo(nio->info.host, nio->info.port, &hints, &addrs);
  if (rc != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
    return -1;
  }

  struct addrinfo* addr = addrs;
  const char* cause = NULL;
  for (struct addrinfo* addr = addrs; addr != NULL; addr = addr->ai_next) {
    fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (fd == -1) {
      cause = "socket";
      continue;
    }
    if (connect(fd, addr->ai_addr, addr->ai_addrlen) < 0) {
      close(fd);
      cause = "connect";
      continue;
    }
    break;    
  }
  freeaddrinfo(addrs);
           
  if (fd == -1) {
    perror(cause);
    return -1;
  }

  struct timeval tv;
  tv.tv_sec = nio->info.timeout;
  tv.tv_usec = 0;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
    perror("setsockopt");
    return -1;
  }

  nio->fd = fd;
  return 0;
}

void nio_close(nio_s *nio)
{
  close(nio->fd);
  nio->fd = 0;
}

/* __inline */
int nio_read(nio_s *nio, void *buf, size_t count)
{
  int rc = read(nio->fd, buf, count);
  return rc;
}

int nio_read_line(nio_s *nio, void *buf, size_t count)
{
  char *p = buf;
  size_t length = 0;
  int n;

  while (length < count) {
    n = nio_read(nio, p, 1);
    if (n <= 0) break;
    if (*p == '\n') break;
    if (*p != '\r') {
      p++;
      length++;
    }
  }
  *p = '\0';
  if (n < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
    return n;
  }
  return length;
}

__inline
int nio_write(nio_s *nio, const void *buf, size_t count)
{
  return write(nio->fd, buf, count);
}

int nio_isopen(nio_s *nio)
{
  return (nio->fd != 0);
}

int nio_setinfo(nio_s *nio, netinfo_s *info)
{
  nio->info = *info;
  return 0;
}
  
void nio_debug(nio_s *nio, FILE *f)
{
  fprintf(f, "nio {\n");
  fprintf(f, "\tfd = %d\n", nio_isopen(nio) ? nio->fd : 0);
  fprintf(f, "\thost = %s\n", nio->info.host);  
  fprintf(f, "\tport = %s\n", nio->info.port);  
  fprintf(f, "}\n\n");
}
