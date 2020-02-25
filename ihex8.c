#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "ihex8.h"

typedef enum {
  OK,
  END,
  ERR_START,
  ERR_LENGTH,
  ERR_ADDRESS,
  ERR_TYPE,
  ERR_UNSUPPORTED,
  ERR_DATA,
  ERR_CHECKSUM,
  ERR_MISMATCH,
  ERR_END
} ReadStatus;

static ReadStatus readRecord(IHex8* ih, IHex8Record** rec);
static int readByte(IHex8* ih);
static int readNibble(IHex8* ih);

static void writeRecord(IHex8* ih, IHex8Record* rec);
static void writeByte(IHex8* ih, uint8_t b);
static void writeNibble(IHex8* ih, uint8_t b);

static int findStartOfRecord(IHex8* ih);
static int isEndOfRecord(IHex8* ih);

static const char* getResponse(ReadStatus status);
static const char* getError(ReadStatus status);

IHex8Record* ihex8Load(IHex8* ih) {
  IHex8Record* head;
  IHex8Record* tail;

  head = (IHex8Record*) malloc(sizeof(IHex8Record));
  tail = head;
  head->next = NULL;
  head->address = 0;
  head->data = NULL;

  ReadStatus status = OK;
  while (status == OK) {
    IHex8Record* record = NULL;
    status = readRecord(ih, &record);
    if (status == ERR_START) continue;
    if (status == OK) {
      tail->next = record;
      tail = record;
    }
  }

  if (status == END) {
    tail = head->next;
    free(head); 
  }
  else {
    ihex8Free(head);
    tail = NULL;
    fputs("error: ", stderr);
    fputs(getError(status), stderr);
    fputc('\n', stderr);
  }
  
  return tail;
}

int ihex8LoadAndStore(IHex8* ih, void* ctx, void(*store)(IHex8Record*, void*)) {
  ReadStatus status = OK;
  while (status == OK) {
    IHex8Record* record = NULL;
    status = readRecord(ih, &record);
    if (status == ERR_START) continue;
    if (status == OK) {
      store(record, ctx);
    }
    ihex8Free(record);
  }

  if (status != END) {
    fputs("error: ", stderr);
    fputs(getError(status), stderr);
    fputc('\n', stderr);
  }
  
  return status != END;   
}

IHex8Record* ihex8Receive(IHex8* ih) {
  IHex8Record* head;
  IHex8Record* tail;

  head = (IHex8Record*) malloc(sizeof(IHex8Record));
  tail = head;
  head->next = NULL;
  head->address = 0;
  head->data = NULL;

  ReadStatus status = OK;
  while (status == OK) {
    IHex8Record* record = NULL;
    status = readRecord(ih, &record);
    if (status == ERR_START) continue;
    ih->writeln(ih->ctx, getResponse(status));
    if (status == OK) {
      tail->next = record;
      tail = record;
    }
  }

  if (status == END) {
    tail = head->next;
    free(head); 
  }
  else {
    ihex8Free(head);
    tail = NULL;
  }
  return tail;
}

int ihex8ReceiveAndStore(IHex8* ih, void* ctx, void (*store)(IHex8Record*, void*)) {
  
  ReadStatus status = OK;
  while (status == OK) {
    IHex8Record* record = NULL;
    status = readRecord(ih, &record);
    if (status == ERR_START) continue;
    ih->writeln(ih->ctx, getResponse(status));
    if (status == OK) {
      store(record, ctx);
    }
    ihex8Free(record);
  }

  return status == END;
}

static ReadStatus readRecord(IHex8* ih, IHex8Record** rec) {
  *rec = NULL;
 
  if (!findStartOfRecord(ih)) {
    return ERR_START;
  }
  
  int length = readByte(ih);
  if (length == -1) return ERR_LENGTH;
  
  int msb = readByte(ih);
  if (msb == -1) return ERR_ADDRESS;
  
  int lsb = readByte(ih);
  if (lsb == -1) return ERR_ADDRESS;
  
  int type = readByte(ih);
  if (type == -1) return ERR_TYPE;
  
  if (type != 0 && type != 1) return ERR_UNSUPPORTED;
  
  if (type == 0 && length > 0) {
    IHex8Record* record = (IHex8Record*) malloc(sizeof(IHex8Record));
    record->address = (msb<<8) | lsb;
    record->length = length;
    record->data = (uint8_t*) malloc(length*sizeof(uint8_t));
    record->next = NULL;
  
    int sum = length;
    sum += msb;
    sum += lsb;
  
    for (int i = 0; i < length; i++) {
      int b = readByte(ih);
      if (b == -1) {
        ihex8Free(record);
        return ERR_DATA;
      }
      record->data[i] = (uint8_t) b;
      sum += b;
    }
  
    int checksum = readByte(ih);
    if (checksum == -1) {
      ihex8Free(record);
      return ERR_CHECKSUM;
    }
  
    sum += checksum;
    if ((sum & 0xff) != 0) {
      ihex8Free(record);
      return ERR_MISMATCH;
    }

    *rec = record;
  }
  
  if (type == 1) {
    int checksum = readByte(ih);
    if (checksum != 0xff) {
      return ERR_MISMATCH;
    }
  }

  if (!isEndOfRecord(ih)) {
    if (*rec != NULL) {
      ihex8Free(*rec);
    }
    return ERR_END;
  }

  return type == 0 ? OK : END;
}

static int findStartOfRecord(IHex8* ih) {
  int c = ih->readc(ih->ctx);
  while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
    if (c == '\r' || c == '\n') {
      ih->writeln(ih->ctx, MSG_OK);
    }
    c = ih->readc(ih->ctx);
  }
  return c == ':';
}

static int isEndOfRecord(IHex8* ih) {
  int c = ih->readc(ih->ctx);
  if (c == '\r') {
    c = ih->readc(ih->ctx);
  }
  return c == '\n';
}

static int readByte(IHex8* ih) {
  int b = readNibble(ih);
  if (b == -1) return -1;
  int c = readNibble(ih);
  if (c == -1) return -1;
  return (b<<4) | c;
}

static int readNibble(IHex8* ih) {
  int c = ih->readc(ih->ctx);
  if (c == -1) return -1;
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

void ihex8Dump(IHex8Record* top, IHex8* ih) {
  while (top != NULL) {
    writeRecord(ih, top);
    top = top->next;
  }

  ih->writeln(ih->ctx, ":00000001FF");
}

int ihex8Send(IHex8Record* top, IHex8* ih) {
  char buf[256];
  int n;
  
  while (top != NULL) {
    writeRecord(ih, top);
    while (1) {
      n = ih->readln(ih->ctx, buf, sizeof(buf));
      if (strncmp(buf, MSG_INFO, strlen(MSG_INFO)) == 0) {
        fprintf(stdout, "%s\n", buf+strlen(MSG_INFO) + 1);
        continue;
      }
      break;
    }
    if (strncmp(buf, MSG_OK, strlen(buf)) != 0) {
      fprintf(stderr, "unexpected response: %s\n", buf);
      return -1;
    }
    top = top->next;
  }
  
  ih->writeln(ih->ctx, ":00000001FF");
  while (1) {
    n = ih->readln(ih->ctx, buf, sizeof(buf));
    if (strncmp(buf, MSG_INFO, strlen(MSG_INFO)) == 0) {
      fprintf(stdout, "%s\n", buf+strlen(MSG_INFO) + 1);
      continue;
    }
    break;
  }
  if (strncmp(buf, MSG_END, strlen(buf)) != 0) {
    fprintf(stderr , "unexpected response: %s\n", buf);
    return -1;
  }

  return 0;
}

static void writeRecord(IHex8* ih, IHex8Record* rec) {
  ih->writec(ih->ctx, ':');

  int sum = rec->length;
  writeByte(ih, rec->length);

  uint8_t msb = rec->address >> 8;
  sum += msb;
  writeByte(ih, msb);

  uint8_t lsb = rec->address & 0xff;
  sum += lsb;
  writeByte(ih, lsb);

  writeByte(ih, 0);
  for (int i = 0; i < rec->length; i++) {
    uint8_t b = rec->data[i];
    sum += b;
    writeByte(ih, b);
  }

  writeByte(ih, ~(sum & 0xff) + 1);
  ih->writec(ih->ctx, '\n');  
}

static void writeByte(IHex8* ih, uint8_t b) {
  writeNibble(ih, b >> 4);
  writeNibble(ih, b & 0xf);
}

static void writeNibble(IHex8* ih, uint8_t c) {
  if (c >= 0 && c <= 9) {
    ih->writec(ih->ctx, c + '0');
  }
  else {
    ih->writec(ih->ctx, c - 10 + 'A');
  }
}

void ihex8Free(IHex8Record* top) {
  IHex8Record* temp;
  while (top != NULL) {
    if (top->data != NULL) free(top->data);
    temp = top;
    top = top->next;
    free(temp);
  }
}

static const char* getResponse(ReadStatus status) {
  static char message[256];
  
  if (status == OK) {
    return MSG_OK;
  }
  else if (status == END) {
    return MSG_END;
  }

  strncpy(message, MSG_ERROR, sizeof(message));
  strncpy(message, ": ", sizeof(message) - strlen(message) - 1);
  strncat(message, getError(status), sizeof(message) - strlen(message) - 1);

  return message;
}

static const char *getError(ReadStatus status) {
  switch (status) {
    case ERR_START:
      return "expected start of record";
    case ERR_LENGTH:
      return "expected record length";
    case ERR_ADDRESS:
      return "expected address";
    case ERR_TYPE:
      return "expected record type";
    case ERR_UNSUPPORTED:
      return "unsupported record type";
    case ERR_DATA:
      return "expected data byte";
    case ERR_CHECKSUM:
      return "expected checksum";
    case ERR_MISMATCH:
      return "checksum mismatch";
    case ERR_END:
      return "expected end of record";
    default:
      return NULL;
  }
}
