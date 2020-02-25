/*
 * sio.c *
 * serial port routines
 *
 * (C)1999 Stefano Busti
 *
 */

#include <stdio.h>

#include "sio.h"

#include <unistd.h>
#include <termios.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int sio_init(sio_s *sio)
{
  sio->fd = 0;
  sio->info.port = NULL;
  sio->info.baud = 9600;
  sio->info.parity = SIO_PARITY_NONE;
  sio->info.stopbits = 1;
  sio->info.databits = 8;
  sio->info.timeout = 1;
  return 0;
}

void sio_cleanup(sio_s *sio)
{
  if (sio_isopen(sio))
    sio_close(sio);
}

int sio_open(sio_s *sio)
{
  struct termios t;
  int fd, c_stop, c_data, i_parity, c_parity;
  speed_t speed;
  
  if (sio_isopen(sio))
    return -1;
           
  fd = open(sio->info.port, O_RDWR);

  if (fd == -1)
    return -1;
  
  if (tcgetattr(fd, &t))
  {
    close(fd);
    return -1;
  }
  
  switch(sio->info.baud)
  {
  case 0: speed = B0; break;
  case 50: speed = B50; break;
  case 75: speed = B75; break;
  case 110: speed = B110; break;
  case 134: speed = B134; break;
  case 150: speed = B150; break;
  case 300: speed = B300; break;
  case 600: speed = B600; break;
  case 1200: speed = B1200; break;
  case 1800: speed = B1800; break;
  case 2400: speed = B2400; break;
  case 4800: speed = B4800; break;
  case 9600: speed = B9600; break;
  case 19200: speed = B19200; break;
  case 38400: speed = B38400; break;
  case 57600: speed = B57600; break;
  case 115200: speed = B115200; break;
  case 230400: speed = B230400; break;
  default: speed = B0; break;
  }

  if (speed == B0)
  {
    close(fd);
    return -1;
  }
  
  if (cfsetospeed(&t, speed))
  {
    close(fd);
    return -1;
  }

  if (cfsetispeed(&t, speed))
  {
    close(fd);
    return -1;
  }
  
  switch(sio->info.stopbits)
  {
  case 1: c_stop = 0; break;
  case 2: c_stop = CSTOPB; break;
  default: close(fd); return -1;
  }

  switch(sio->info.databits)
  {
  case 5: c_data = CS5; break;
  case 6: c_data = CS6; break;
  case 7: c_data = CS7; break;
  case 8: c_data = CS8; break;
  default: close(fd); return -1;
  }

  switch(sio->info.parity)
  {
  case SIO_PARITY_NONE:
    i_parity = IGNPAR;
    c_parity = 0;
    break;
    
  case SIO_PARITY_EVEN:
    i_parity = INPCK;
    c_parity = PARENB;
    break;

  case SIO_PARITY_ODD:
    i_parity = INPCK;
    c_parity = PARENB | PARODD;
    break;

  default:
    close(fd);
    return -1;
  }

  t.c_cc[VMIN]  = 0;
  t.c_cc[VTIME] = 10 * sio->info.timeout;

  if (tcsetattr(fd, TCSANOW, &t))
  {
    close(fd);
    return -1;
  }
  
  sio->fd = fd;
  
  return 0;
}

void sio_close(sio_s *sio)
{
  close(sio->fd);
  sio->fd = 0;
}

__inline
int sio_read(sio_s *sio, void *buf, size_t count)
{
  return read(sio->fd, buf, count);
}

int sio_read_line(sio_s *sio, void *buf, size_t count)
{
  char *p = buf;
  size_t length = 0;
  size_t n;

  while (length < count) {
    n = sio_read(sio, p, 1);
    if (n <= 0) break;
    if (*p == '\n') break;
    if (*p != '\r') {
      p++;
      length++;
    }
  }
  if (length == 0 && n < 0) return n;
  *p = '\0';
  return length;
}

__inline
int sio_write(sio_s *sio, const void *buf, size_t count)
{
  return write(sio->fd, buf, count);
}

int sio_isopen(sio_s *sio)
{
  return (sio->fd != 0);
}

int sio_setinfo(sio_s *sio, serialinfo_s *info)
{
  sio->info = *info;
  return 0;
}
  
void sio_flush(sio_s *sio, int dir)
{
  if (sio_isopen(sio))
  {
    switch(dir)
    {
    case SIO_IN: tcflush(sio->fd, TCIFLUSH); break;
    case SIO_OUT: tcflush(sio->fd, TCOFLUSH); break;
    case SIO_IN | SIO_OUT: tcflush(sio->fd, TCIOFLUSH); break;
    }
  }
}

void sio_drain(sio_s *sio)
{
  if (sio_isopen(sio))
  {
    tcdrain(sio->fd);
  }
}

void sio_debug(sio_s *sio, FILE *f)
{
  fprintf(f, "sio {\n");
  fprintf(f, "\tfd = %d\n", sio_isopen(sio) ? sio->fd : 0);
  fprintf(f, "\tport = %s\n", sio->info.port);  
  fprintf(f, "\tbaud = %ld\n", sio->info.baud);  
  fprintf(f, "\tparity = %d\n", sio->info.parity);  
  fprintf(f, "\tstopbits = %d\n", sio->info.stopbits);
  fprintf(f, "\tdatabits = %d\n", sio->info.databits);  
  fprintf(f, "\topen = %d\n", sio_isopen(sio));  
  fprintf(f, "}\n\n");
}
