/**
 * readn, writen — adapted from W. Richard Stevens,
 * Unix Network Programming.
 *
 * These helpers perform full-length blocking I/O,
 * retrying on EINTR and handling partial reads/writes.
 */


#include "netUtils.h"

ssize_t readn(int fd, void *buf, size_t n) {
  size_t nleft = n;
  ssize_t nread;
  char *ptr = buf;

  while (nleft > 0) {
    if ((nread = read(fd, ptr, nleft)) < 0) {
      if (errno == EINTR)
        nread = 0;
      else
        return -1;
    } else if (nread == 0)
      break;
    nleft -= nread;
    ptr += nread;
  }
  return n - nleft;
}

ssize_t writen(int fd, const void *buf, size_t n) {
  size_t nleft = n;
  ssize_t nwritten;
  const char *ptr = buf;

  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0 && errno == EINTR)
        nwritten = 0;
      else
        return -1;
    }
    nleft -= nwritten;
    ptr += nwritten;
  }
  return n;
}
