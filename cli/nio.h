#ifndef nio_h 
#define nio_h

#include <stdio.h>

#define SIO_TTY

typedef struct
{
        const char *host;
 	const char *port;
        int timeout;
} netinfo_s;

typedef struct
{
	int fd;
	netinfo_s info;
} nio_s;

int nio_init(nio_s *sio);
void nio_cleanup(nio_s *sio);

int nio_open(nio_s *sio);
void nio_close(nio_s *sio);
int nio_read(nio_s *sio, void *buf, size_t count);
int nio_read_line(nio_s *sio, void *buf, size_t count);
int nio_write(nio_s *sio, const void *buf, size_t count);
int nio_isopen(nio_s *sio);
int nio_setinfo(nio_s *sio, netinfo_s *info);
void nio_debug(nio_s *sio, FILE *f);

#endif	/* nio_h */
