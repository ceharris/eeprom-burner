#ifndef ihex8_h
#define ihex8_h

#include <stdint.h>

#define MSG_OK    "OK"
#define MSG_INFO  "INFO"
#define MSG_END   "END"
#define MSG_ERROR "ERROR"

typedef struct ihex8_record_t {
  struct ihex8_record_t* next;
  uint16_t address;
  uint8_t length;
  uint8_t* data;
} IHex8Record;


typedef struct ihex8_t {
  int (*readc)(void* ctx);
  int (*readln)(void *ctx, char *buf, int buflen);
  int (*writec)(void* ctx, char c);
  int (*writeln)(void* ctx, const char* s);
  void* ctx;
} IHex8;

#ifdef __cplusplus
extern "C" {
#endif

IHex8Record* ihex8Load(IHex8* ih);

int ihex8LoadAndStore(IHex8* ih, void* ctx, 
    void(*store)(IHex8Record*, void*));

void ihex8Dump(IHex8Record* rec, IHex8* ih);

IHex8Record* ihex8Receive(IHex8* ih);

int ihex8ReceiveAndStore(IHex8* ih, void* ctx, 
    void(*store)(IHex8Record*, void*));

int ihex8Send(IHex8Record* rec, IHex8* ih);

void ihex8Free(IHex8Record* rec);

#ifdef __cplusplus
}
#endif

#endif /* ihex8_h */
