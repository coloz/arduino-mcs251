#ifndef STC_SD_FILE_CONTEXT_H
#define STC_SD_FILE_CONTEXT_H
#include <stdint.h>

/* One small context per open handle; the volume still shares one sector
 * cache. This layout is shared verbatim by the C backend and C++ facade. */
typedef struct {
    uint8_t open, writable, metadata_dirty, is_directory;
    uint8_t name[11];
    unsigned long dir_sector;
    unsigned int dir_offset;
    unsigned long first_cluster, size, position, cluster, cluster_index;
} STCSDFileContext;
#if defined(__SDCC_mcs251) || defined(__STC_CLANG_IR_ONLY__)
typedef char stc_sd_context_abi_must_be_41_bytes[(sizeof(STCSDFileContext) == 41u) ? 1 : -1];
#endif

#define SD_HAS_DIRECTORIES 1
#define SD_HAS_MULTIPLE_FILES 1
#define SD_HAS_LONG_FILE_NAMES 0
#define SD_ERROR_FILE_BUSY 21u
#define SD_ERROR_NOT_A_DIRECTORY 22u
#define SD_ERROR_DIRECTORY_NOT_EMPTY 23u

#if defined(__SDCC)
#define STC_SD_FILE_CALL __reentrant
#else
#define STC_SD_FILE_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
uint8_t SD_selectFile(STCSDFileContext *file) STC_SD_FILE_CALL;
void SD_forgetFile(STCSDFileContext *file) STC_SD_FILE_CALL;
unsigned long SD_generation(void) STC_SD_FILE_CALL;
uint8_t SD_openNext(STCSDFileContext *file, uint8_t mode) STC_SD_FILE_CALL;
void SD_rewindDirectory(void) STC_SD_FILE_CALL;
void SD_fileName(char *name) STC_SD_FILE_CALL;
void SD_setError(uint8_t error) STC_SD_FILE_CALL;
#ifdef __cplusplus
}
#endif
#endif
