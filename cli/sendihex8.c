#include <stdlib.h>
#include <stdio.h> 
#include <string.h> 
#include "sio.h"
#include "nio.h"
#include "../ihex8.h"

#define PORT "/dev/cu.usbmodem14101"


IHex8Record* load_ihex_data(FILE* fp);

IHex8 *open_controller(const int argc, const char* argv[]);
IHex8 *open_controller_sio(const char* port, int speed);
IHex8 *open_controller_nio(const char* host, const char* port);

int await_controller_ready(IHex8* ih);
int await_controller_done(IHex8* ih);
int close_controller(IHex8* ih);
void echo_output(IHex8* ih);

int file_readc(void* fp);
int file_writec(void* fp, char c);
int file_writeln(void* fp, const char* s);

int sio_writec(void* sio, char c);
int sio_writeln(void *sio, const char* s);
int sio_readln(void* sio, char* buf, int buflen);

int nio_writec(void* sio, char c);
int nio_writeln(void *sio, const char* s);
int nio_readln(void* sio, char* buf, int buflen);

int main(const int argc, const char* argv[]) {
  int rc = 1;
  IHex8* ctrlr = open_controller(argc, argv);
  if (ctrlr == NULL) goto error;

  IHex8Record* rex = load_ihex_data(stdin);
 
  if (await_controller_ready(ctrlr) != 0) goto error;
  puts("Sending programming data");
  if (ihex8Send(rex, ctrlr) != 0) goto error;
  if (await_controller_done(ctrlr) != 0) goto error;
  rc = 0;
  echo_output(ctrlr);

error:
  return rc;
}

IHex8* open_controller(const int argc, const char* argv[]) {
  // TODO use args to set parameters and choose I/O method
  return open_controller_sio(PORT, 115200);
//  return open_controller_nio("localhost", "5331");
}

IHex8* open_controller_sio(const char* port, int speed) {
  puts("open sio");
  sio_s* sio = (sio_s*) malloc(sizeof(sio_s));;
  sio_init(sio);
  sio->info.port = port;
  sio->info.baud = speed;

  if (sio_open(sio) != 0) {
    free(sio);
    return NULL;
  }

  IHex8* ih = (IHex8*) malloc(sizeof(IHex8));
  ih->writec = sio_writec;
  ih->writeln = sio_writeln;
  ih->readln = sio_readln;
  ih->ctx = sio;

  return ih;
}

IHex8* open_controller_nio(const char* host, const char* port) {
  nio_s* nio = (nio_s*) malloc(sizeof(nio_s));;
  nio_init(nio);
  nio->info.host = host;
  nio->info.port = port;

  if (nio_open(nio) != 0) {
    free(nio);
    return NULL;
  }

  IHex8* ih = (IHex8*) malloc(sizeof(IHex8));
  ih->writec = nio_writec;
  ih->writeln = nio_writeln;
  ih->readln = nio_readln;
  ih->ctx = nio;

  return ih;
}

int await_controller_ready(IHex8* ih) {
  char buf[256];
  fputs("Syncing controller\n", stdout);
  int max_tries = 30;
  int ok = 5;
  while (max_tries > 0) { 
    ih->writec(ih->ctx, '\n');
    if (ih->readln(ih->ctx, buf, sizeof(buf)) == -1) {
      fputs("error in await_controller_ready\n", stderr);
      return -1;
    }
    if (strncmp(buf, MSG_INFO, strlen(MSG_INFO)) == 0) {
      fprintf(stdout, "%s\n", buf+strlen(MSG_INFO) + 1);
      continue;
    }
    if (strncmp(buf, MSG_OK, strlen(buf)) == 0) {
      ok--;
      if (ok == 0) {
        fputs("Controller ready\n", stdout);
        return 0;
      }
    }
    max_tries--;
  }

  fputs("timeout in await_controller_ready\n", stderr);
  return -1;
}

int await_controller_done(IHex8* ih) {
  char buf[256];
  int max_tries = 60;
  while (max_tries) {
    int n = ih->readln(ih->ctx, buf, sizeof(buf));
    if (n == -1) {
      fputs("error in await_controller_done\n", stderr);
      return -1;
    }
    if (strncmp(buf, MSG_INFO, strlen(MSG_INFO)) == 0) {
      fprintf(stdout, "%s\n", buf+strlen(MSG_INFO) + 1);
      continue;
    }
    if (strncmp(buf, MSG_OK, strlen(buf)) == 0) {
      return 0;
    }
    if (strncmp(buf, MSG_ERROR, strlen(MSG_ERROR)) == 0) {
      return -1;
    }
    if (n > 0) {
      fputs(buf, stdout);
      fputc('\n', stdout);
    }
    max_tries--;
  }
  fputs("timeout in await_controller_done\n", stderr);
  return -1;
}

void echo_output(IHex8* ih) {
  char buf[256];
  while (1) {
    int len = ih->readln(ih->ctx, buf, sizeof(buf));
    if (len < 0) {
      fputs("error reading output\n", stderr);
      break;
    }
    if (len == 0) continue;
    if (strncmp(buf, MSG_OK, strlen(buf)) == 0) {
      break;
    }
    fputs(buf, stdout);
    fputc('\n', stdout); 
  }
}

IHex8Record* load_ihex_data(FILE* fp) {
  IHex8 ih;
  ih.readc = file_readc;
  ih.writec = file_writec;
  ih.writeln = file_writeln;
  ih.ctx = fp;
  IHex8Record* rex = ihex8Load(&ih);
  ih.ctx = stdout;
  return rex;
}

int file_readc(void* fp) {
  return fgetc((FILE*) fp);
}

int file_writec(void* fp, char c) {
  return fputc(c, stdout);
}

int file_writeln(void* fp, const char* s) {
  fputs(s, stdout);
  return fputc('\n', stdout);
}

int sio_writec(void* ctx, char c) {
  char buf[1];
  buf[0] = c;
  return sio_write((sio_s*) ctx, buf, 1);
}

int sio_writeln(void* ctx, const char* s) {
  int rc = sio_write((sio_s*) ctx, s, strlen(s));
  if (rc == -1) return -1;
  if (sio_writec(ctx, '\n') == -1) return -1;
  return rc + 1; 
}
 
int sio_readln(void* ctx, char* buf, int buflen) {
  return sio_read_line((sio_s*) ctx, buf, buflen);   
}

int nio_writec(void* ctx, char c) {
  char buf[1];
  buf[0] = c;
  return nio_write((nio_s*) ctx, buf, 1);
}

int nio_writeln(void* ctx, const char* s) {
  int rc = nio_write((nio_s*) ctx, s, strlen(s));
  if (rc == -1) return -1;
  if (nio_writec(ctx, '\n') == -1) return -1;
  return rc + 1; 
}
 
int nio_readln(void* ctx, char* buf, int buflen) {
  return nio_read_line((nio_s*) ctx, buf, buflen);   
}
