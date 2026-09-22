#ifndef STCXX_SD_CLASS_H
#define STCXX_SD_CLASS_H

#include <Arduino.h>
#include <SPI.h>
#include "SD_file.h"

#if !defined(STC_XDATA_BYTES) || (STC_XDATA_BYTES < 1024UL)
# error "The STC SD library requires at least 1024 bytes of XDATA"
#endif

#ifndef FILE_READ
# define FILE_READ 0x01u
#endif
#ifndef FILE_WRITE
# define FILE_WRITE 0x17u
#endif

#define SD_CARD_NONE 0u
#define SD_CARD_SD1  1u
#define SD_CARD_SD2  2u
#define SD_CARD_SDHC 3u

#define SD_FAT_NONE 0u
#define SD_FAT16    16u
#define SD_FAT32    32u

#define SD_ERROR_NONE               0u
#define SD_ERROR_INVALID_PIN        1u
#define SD_ERROR_PIN_CONFLICT       2u
#define SD_ERROR_CARD_TIMEOUT       3u
#define SD_ERROR_CARD_RESPONSE      4u
#define SD_ERROR_CARD_UNSUPPORTED   5u
#define SD_ERROR_ADDRESS_OVERFLOW   6u
#define SD_ERROR_READ_TOKEN         7u
#define SD_ERROR_WRITE_REJECTED     8u
#define SD_ERROR_WRITE_STATUS       9u
#define SD_ERROR_INVALID_ARGUMENT  10u
#define SD_ERROR_NOT_INITIALIZED   11u
#define SD_ERROR_BAD_VOLUME        12u
#define SD_ERROR_FAT12_UNSUPPORTED 13u
#define SD_ERROR_NOT_FOUND         14u
#define SD_ERROR_NOT_A_FILE        15u
#define SD_ERROR_READ_ONLY         16u
#define SD_ERROR_BAD_CLUSTER       17u
#define SD_ERROR_NO_SPACE          18u
#define SD_ERROR_DIRECTORY_FULL    19u
#define SD_ERROR_UNSUPPORTED       20u

#ifdef __cplusplus
extern "C" {
#endif

uint8_t SD_setPins(uint8_t mosi, uint8_t miso, uint8_t clock,
                   uint8_t select);
uint8_t SD_begin(uint8_t select);
uint8_t SD_beginClock(unsigned long clock, uint8_t select);
uint8_t SD_beginDefault(void);
void SD_end(void);
uint8_t SD_cardType(void);
uint8_t SD_fatType(void);
uint8_t SD_error(void);
uint8_t SD_readBlock(unsigned long sector, uint8_t *buffer);
uint8_t SD_writeBlock(unsigned long sector, const uint8_t *buffer);
uint8_t SD_exists(const char *name);
uint8_t SD_open(const char *name, uint8_t mode);
size_t SD_write(uint8_t value);
size_t SD_writeBytes(const uint8_t *buffer, size_t length);
int SD_read(void);
size_t SD_readBytes(uint8_t *buffer, size_t length);
int SD_peek(void);
unsigned long SD_available(void);
uint8_t SD_seek(unsigned long position);
unsigned long SD_position(void);
unsigned long SD_size(void);
uint8_t SD_flush(void);
void SD_close(void);
uint8_t SD_remove(const char *name);
uint8_t SD_mkdir(const char *name);
uint8_t SD_rmdir(const char *name);

#if defined(STC_SD_HOST_TEST) && STC_SD_HOST_TEST
uint8_t SD_testMount(void);
void SD_testReset(void);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

struct STCSDFileState;

class File : public Stream
{
public:
    File();
    File(const File &other);
    File(File &&other);
    ~File();

    File &operator=(const File &other);
    File &operator=(File &&other);

    size_t write(uint8_t value);
    size_t write(const uint8_t *buffer, size_t size);
    using Print::write;
    int availableForWrite();

    int available();
    int read();
    int read(void *buffer, uint16_t length);
    int peek();
    void flush();

    bool seek(uint32_t position);
    uint32_t position() const;
    uint32_t size() const;
    void close();
    operator bool() const;
    const char *name() const;
    bool isDirectory() const;
    File openNextFile(uint8_t mode = FILE_READ);
    void rewindDirectory();

private:
    explicit File(STCSDFileState *state);
    bool valid() const;
    void retain(const File &other);
    void release();
    bool select() const;
    static bool closeAll();
    static void invalidateHandles();

    // Shared storage keeps its address when File is returned by value. Stale
    // owners keep it allocated, preventing identity reuse across later opens.
    STCSDFileState *_state;

    friend class SDClass;
};

class SDClass
{
public:
    bool begin();
    bool begin(uint8_t select);
    bool begin(uint32_t clock, uint8_t select);
    void end();
    bool setPins(uint8_t mosi, uint8_t miso, uint8_t clock, uint8_t select);
    File open(const char *name, uint8_t mode = FILE_READ);
    File open(const String &name, uint8_t mode = FILE_READ)
    {
        return open(name.c_str(), mode);
    }
    bool exists(const char *name);
    bool exists(const String &name) { return exists(name.c_str()); }

    bool remove(const char *name);
    bool remove(const String &name) { return remove(name.c_str()); }
    bool mkdir(const char *name);
    bool mkdir(const String &name) { return mkdir(name.c_str()); }
    bool rmdir(const char *name);
    bool rmdir(const String &name) { return rmdir(name.c_str()); }

    uint8_t cardType() const { return SD_cardType(); }
    uint8_t fatType() const { return SD_fatType(); }
    uint8_t error() const { return SD_error(); }
};

extern SDClass SD;

#endif
