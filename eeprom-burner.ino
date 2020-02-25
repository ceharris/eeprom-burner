
#include <stdio.h>
#include <ctype.h>
#include <SPI.h>
#include "ihex8.h"

#define TIMEOUT 30000
#define DUMP_FORMAT "%04x  %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x  %c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c"

#define dch(b) (isprint(b) ? b : '.')

#define SCK 13
#define MISO 12
#define MOSI 11
#define DATA_IN_LATCH 10
#define DATA_OUT_LATCH 9
#define DATA_OUT_ENABLE 8
#define ADDR_LOW_LATCH 7
#define ADDR_HIGH_LATCH 6
#define EEPROM_OUT_ENABLE 5
#define EEPROM_WRITE_ENABLE 4

#define PAGE_BITS 6
#define PAGE_SIZE (1<<PAGE_BITS)
#define PAGE_MASK (PAGE_SIZE-1)

typedef struct {
  uint8_t data[PAGE_SIZE];      /* page data */
  uint16_t address;             /* k-bit page address */
  uint8_t offsets[PAGE_SIZE];   /* (16-k)-bit page offsets */
  uint16_t count;               /* number of offsets */
} Page;

int programEEPROM(IHex8* ih);
void dumpEEPROM();
void storeRecord(IHex8Record* record, void* ctx);
void storeInPage(uint16_t address, uint8_t data, Page* page);
void writePage(Page* page);
void writeByte(uint8_t data, uint16_t addr);
void sendByte(uint8_t data, uint16_t addr);
uint8_t recvByte(uint16_t addr);

int readc(void* ctx);
int writec(void* ctx, char c);
int writeln(void* ctx, const char* c);


IHex8 ihex8 = { 
  .readc = readc,
  .readln = NULL,
  .writec = writec,
  .writeln = writeln,
  .ctx = NULL
};

boolean done;
uint16_t address;

void setup() {
  pinMode(SCK, OUTPUT);
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(DATA_IN_LATCH, OUTPUT);
  pinMode(DATA_OUT_LATCH, OUTPUT);
  pinMode(ADDR_LOW_LATCH, OUTPUT);
  pinMode(ADDR_HIGH_LATCH, OUTPUT);
  pinMode(EEPROM_OUT_ENABLE, OUTPUT);
  pinMode(EEPROM_WRITE_ENABLE, OUTPUT);
  pinMode(DATA_OUT_ENABLE, OUTPUT);

  digitalWrite(DATA_IN_LATCH, HIGH);
  digitalWrite(DATA_OUT_LATCH, LOW);
  digitalWrite(ADDR_LOW_LATCH, LOW);
  digitalWrite(ADDR_HIGH_LATCH, LOW);

  digitalWrite(EEPROM_WRITE_ENABLE, HIGH);
  digitalWrite(EEPROM_OUT_ENABLE, HIGH);
  digitalWrite(DATA_OUT_ENABLE, HIGH);

  Serial.begin(115200);
}

void loop() {
  while (!done) {
    done = programEEPROM(&ihex8);
    if (!done) {
      for (int i = 0; i < 8; i++) {
        Serial.println("EEPROM programming failed");
      }
    }
    else {
      Serial.println("EEPROM programming completed");
      Serial.println("OK");      
      dumpEEPROM();
      Serial.println("OK");
    }
  }
  if (done) {
    digitalWrite(SCK, HIGH);
    delay(125);
    digitalWrite(SCK, LOW);
    sendByte(0, address++);
    delay(125);
  }
}

int programEEPROM(IHex8* ih) {
  Page page;
  memset(&page, 0, sizeof(Page));
  digitalWrite(EEPROM_OUT_ENABLE, HIGH);
  digitalWrite(DATA_OUT_ENABLE, LOW);
  int rc = ihex8ReceiveAndStore(ih, &page, storeRecord);
  if (page.count != 0) {
    writePage(&page);
  }
  digitalWrite(DATA_OUT_ENABLE, HIGH);
  digitalWrite(EEPROM_OUT_ENABLE, LOW);
  return rc;   
}

void dumpEEPROM() {
  char cbuf[80];
  uint8_t dbuf[16];
  for (uint16_t i = 0; i < (1024 / 256); i++) {
    for (uint8_t j = 0; j < (256 / 16); j++) {
      uint16_t addr = 256*i + 16*j;
      for (uint8_t k = 0; k < 16; k++) {
        uint8_t data = recvByte(addr + k);
        dbuf[k] = data;
      }
      
      snprintf(cbuf, sizeof(cbuf), DUMP_FORMAT, addr, 
          dbuf[0], dbuf[1], dbuf[2], dbuf[3], 
          dbuf[4], dbuf[5], dbuf[6], dbuf[7],
          dbuf[8], dbuf[9], dbuf[10], dbuf[11], 
          dbuf[12], dbuf[13], dbuf[14], dbuf[15],
          dch(dbuf[0]), dch(dbuf[1]), dch(dbuf[2]), dch(dbuf[3]), 
          dch(dbuf[4]), dch(dbuf[5]), dch(dbuf[6]), dch(dbuf[7]),
          dch(dbuf[8]), dch(dbuf[9]), dch(dbuf[10]), dch(dbuf[11]), 
          dch(dbuf[12]), dch(dbuf[13]), dch(dbuf[14]), dch(dbuf[15]));
      Serial.println(cbuf);
    }
    Serial.println(" ");
  }
}

void storeRecord(IHex8Record* record, void* ctx) {
  for (uint8_t i = 0; i < record->length; i++) {
    storeInPage(record->address + i, record->data[i], (Page*) ctx);
  }
}

void storeInPage(uint16_t address, uint8_t data, Page* page) {
  uint16_t pageAddress = address >> PAGE_BITS;
  if (pageAddress != page->address) {
    writePage(page);
    page->address = pageAddress;
  }

  uint8_t offset = address & PAGE_MASK;
  page->data[offset] = data;

  uint16_t i = 0;
  while (i < page->count && page->offsets[i] != offset) {
    i++;
  }

  if (i == page->count) {
    if (page->count == PAGE_SIZE) {
      writePage(page);
    }
    page->offsets[i] = offset;
    page->count++;
  }
}

void writePage(Page* page) {
  for (uint16_t i = 0; i < page->count; i++) {
    uint8_t offset = page->offsets[i];
    uint16_t address = (page->address<<PAGE_BITS) | offset;
    writeByte(page->data[offset], address);
  }
  delay(15);
}

void writeByte(uint8_t data, uint16_t addr) {
  sendByte(data, addr);
  digitalWrite(EEPROM_WRITE_ENABLE, LOW);
  delayMicroseconds(1);
  digitalWrite(EEPROM_WRITE_ENABLE, HIGH);
}

void sendByte(uint8_t data, uint16_t addr) {
  SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
  uint8_t addr_low = addr & 0xff;
  uint8_t addr_high = addr >> 8;
  SPI.transfer(data);  
  digitalWrite(DATA_OUT_LATCH, HIGH);
  SPI.transfer(addr_low);
  digitalWrite(ADDR_LOW_LATCH, HIGH);
  SPI.transfer(addr_high);
  digitalWrite(ADDR_HIGH_LATCH, HIGH);
  SPI.endTransaction();
  digitalWrite(DATA_OUT_LATCH, LOW);
  digitalWrite(ADDR_LOW_LATCH, LOW);
  digitalWrite(ADDR_HIGH_LATCH, LOW);
}

uint8_t recvByte(uint16_t addr) {
  SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
  uint8_t addr_low = addr & 0xff;
  uint8_t addr_high = addr >> 8;
  SPI.transfer(addr_low);
  digitalWrite(ADDR_LOW_LATCH, HIGH);
  SPI.transfer(addr_high);
  digitalWrite(ADDR_HIGH_LATCH, HIGH);
  digitalWrite(DATA_IN_LATCH, LOW);
  delayMicroseconds(1);
  digitalWrite(ADDR_LOW_LATCH, LOW);
  digitalWrite(ADDR_HIGH_LATCH, LOW);
  digitalWrite(DATA_IN_LATCH, HIGH);
  return SPI.transfer(0);  
}

int readc(void* ctx) {
  unsigned long start = millis();
  while (millis() - start <= TIMEOUT) {
    int c = Serial.read();
    if (c != -1) return c;
  }
  return -1;
}

int writec(void* ctx, char c) {
  Serial.write(c);
}

int writeln(void* ctx, const char* s) {
  Serial.println(s);
}
