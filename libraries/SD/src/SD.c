/*
 * SPDX-License-Identifier: MIT
 *
 * Clean-room SPI SD-card and FAT16/FAT32 implementation for the
 * arduino-mcs251 C++ core. No code from Arduino's C++ SD/SdFat stack is used.
 */
#include "SD_backend.h"

/* The C hardware backend calls the native SPI_* entry points. */

#if defined(__SDCC)
# define STC_SD_XDATA __xdata
#else
# define STC_SD_XDATA
#endif

#define SD_CMD0_GO_IDLE_STATE         0u
#define SD_CMD8_SEND_IF_COND          8u
#define SD_CMD13_SEND_STATUS         13u
#define SD_CMD16_SET_BLOCKLEN        16u
#define SD_CMD17_READ_SINGLE_BLOCK   17u
#define SD_CMD24_WRITE_BLOCK         24u
#define SD_CMD41_SD_SEND_OP_COND     41u
#define SD_CMD55_APP_CMD             55u
#define SD_CMD58_READ_OCR            58u

#define SD_R1_IDLE_STATE       0x01u
#define SD_R1_ILLEGAL_COMMAND  0x04u
#define SD_DATA_START_TOKEN    0xfeu
#define SD_WRITE_DATA_ACCEPTED 0x05u

#define SD_SECTOR_SIZE 512u
#define SD_INIT_CLOCK_HZ 100000UL
#if STC_CORE_BUS_LAYOUT == 1
#define SD_DATA_CLOCK_HZ 4000000UL
#else
#define SD_DATA_CLOCK_HZ 400000UL
#endif
static unsigned long sd_data_clock_hz = SD_DATA_CLOCK_HZ;
static uint8_t sd_transaction_active;

#define SD_READY_ATTEMPT_LIMIT 60000u
#define SD_READY_TIMEOUT_MS      600UL
#define SD_R1_ATTEMPT_LIMIT       32u
#define SD_R1_TIMEOUT_MS         100UL
#define SD_TOKEN_ATTEMPT_LIMIT 60000u
#define SD_TOKEN_TIMEOUT_MS      600UL
#define SD_INIT_ATTEMPT_LIMIT   1000u
#define SD_INIT_TIMEOUT_MS      2000UL
#define SD_ROOT_SCAN_SECTOR_LIMIT 4096UL
#define SD_UINT32_MAX     0xffffffffUL

#define SD_DIR_ENTRY_SIZE        32u
#define SD_DIR_ENTRIES_PER_SECTOR 16u
#define SD_DIR_ATTRIBUTE          11u
#define SD_DIR_CLUSTER_HIGH       20u
#define SD_DIR_CLUSTER_LOW        26u
#define SD_DIR_FILE_SIZE          28u
#define SD_ATTRIBUTE_DIRECTORY  0x10u
#define SD_ATTRIBUTE_VOLUME_ID  0x08u
#define SD_ATTRIBUTE_LONG_NAME  0x0fu
#define SD_ATTRIBUTE_ARCHIVE    0x20u
#define SD_ATTRIBUTE_READ_ONLY  0x01u

#define SD_MODE_WRITE           0x02u
#define SD_MODE_CREATE          0x04u
#define SD_MODE_APPEND          0x10u

typedef struct {
    uint8_t mosi_pin;
    uint8_t miso_pin;
    uint8_t sck_pin;
    uint8_t cs_pin;
    uint8_t pins_loaded;

    uint8_t spi_active;
    uint8_t card_ready;
    uint8_t mounted;
    uint8_t card_type;
    uint8_t fat_type;
    uint8_t last_error;

    uint8_t sectors_per_cluster;
    uint8_t cluster_shift;
    uint8_t fat_count;
    unsigned int root_entry_count;
    unsigned long volume_start;
    unsigned long volume_sectors;
    unsigned long fat_base_start;
    unsigned long fat_start;
    unsigned long fat_sectors;
    unsigned long root_dir_start;
    unsigned long root_dir_sectors;
    unsigned long data_start;
    unsigned long cluster_count;
    unsigned long root_cluster;
    unsigned long allocation_hint;
    uint8_t active_fat;
    uint8_t fat_mirroring;

    uint8_t cache_valid;
    uint8_t cache_dirty;
    unsigned long cache_sector;

} STCSDState;

typedef struct {
    unsigned long sector;
    unsigned long cluster;
    unsigned long size;
    unsigned int offset;
    uint8_t attribute;
} STCSDDirEntry;

/* One shared sector cache; file contexts remain in XDATA. The optional hot
 * volume state uses 68 bytes of DATA, never IDATA. */
static STC_SD_XDATA uint8_t sd_sector[SD_SECTOR_SIZE];
#if defined(__SDCC) && STC_SD_STATE_IN_DATA
static __data STCSDState sd_state;
typedef char stc_sd_data_budget[(sizeof(STCSDState) <= 68u) ? 1 : -1];
#else
static STC_SD_XDATA STCSDState sd_state;
#endif
static STC_SD_XDATA STCSDFileContext sd_legacy_file;
static STCSDFileContext *sd_file = &sd_legacy_file;
/* Fixed-address working state keeps 24-bit pointer arithmetic out of every
 * byte operation. Contexts are copied only at file switches/open/close. */
static STC_SD_XDATA STCSDFileContext sd_active_file;
static void sd_save_file(void) { *sd_file = sd_active_file; }
static unsigned long sd_generation;
void SD_forgetFile(STCSDFileContext *file) STC_SD_REENTRANT
{
    if (sd_file == file) {
        sd_save_file(); sd_file = &sd_legacy_file; sd_active_file = sd_legacy_file;
    }
}
unsigned long SD_generation(void) STC_SD_REENTRANT { return sd_generation; }
void SD_setError(uint8_t error) STC_SD_REENTRANT { sd_state.last_error = error; }
uint8_t SD_selectFile(STCSDFileContext *file) STC_SD_REENTRANT
{
    if (file == NULL) file = &sd_legacy_file;
    if (file == sd_file) return 1u;
    if (sd_active_file.open && !SD_flush()) return 0u;
    sd_save_file();
    sd_file = file; sd_active_file = *file;
    return 1u;
}

#if defined(STC_SD_HOST_TEST) && STC_SD_HOST_TEST
extern uint8_t STC_SD_testReadBlock(unsigned long sector, uint8_t *buffer);
extern void STC_SD_testPrepareRead(void);
extern uint8_t STC_SD_testWriteBlock(unsigned long sector,
                                     const uint8_t *buffer);
#endif

static void sd_set_error(uint8_t error)
{
    sd_state.last_error = error;
}

static void sd_load_default_pins(void)
{
    if (sd_state.pins_loaded == 0u) {
        sd_state.mosi_pin = SD_DEFAULT_MOSI_PIN;
        sd_state.miso_pin = SD_DEFAULT_MISO_PIN;
        sd_state.sck_pin = SD_DEFAULT_SCK_PIN;
        sd_state.cs_pin = SD_DEFAULT_CS_PIN;
        sd_state.pins_loaded = 1u;
    }
}

static void sd_clear_file(void)
{
    sd_active_file.is_directory = 0u;
    sd_active_file.open = 0u;
    sd_active_file.writable = 0u;
    sd_active_file.metadata_dirty = 0u;
    sd_active_file.dir_sector = 0UL;
    sd_active_file.dir_offset = 0u;
    sd_active_file.first_cluster = 0UL;
    sd_active_file.size = 0UL;
    sd_active_file.position = 0UL;
    sd_active_file.cluster = 0UL;
    sd_active_file.cluster_index = 0UL;
    sd_save_file();
}

static void sd_clear_volume(void)
{
    ++sd_generation;
    sd_legacy_file.open = 0u;
    sd_state.mounted = 0u;
    sd_state.fat_type = SD_FAT_NONE;
    sd_state.sectors_per_cluster = 0u;
    sd_state.fat_count = 0u;
    sd_state.root_entry_count = 0u;
    sd_state.volume_start = 0UL;
    sd_state.volume_sectors = 0UL;
    sd_state.fat_base_start = 0UL;
    sd_state.fat_start = 0UL;
    sd_state.fat_sectors = 0UL;
    sd_state.root_dir_start = 0UL;
    sd_state.root_dir_sectors = 0UL;
    sd_state.data_start = 0UL;
    sd_state.cluster_count = 0UL;
    sd_state.root_cluster = 0UL;
    sd_state.allocation_hint = 2UL;
    sd_state.active_fat = 0u;
    sd_state.fat_mirroring = 1u;
    sd_state.cache_valid = 0u;
    sd_state.cache_dirty = 0u;
    sd_state.cache_sector = 0UL;
    sd_clear_file();
}

static uint8_t sd_pins_conflict(uint8_t left, uint8_t right)
{
    if (left == right) {
        return 1u;
    }
    return (digitalPinsSharePhysicalPad(left, right) != 0) ? 1u : 0u;
}

static uint8_t sd_validate_pins(uint8_t mosi_pin, uint8_t miso_pin,
                                uint8_t sck_pin, uint8_t cs_pin)
{
    if ((digitalPinIsValid(mosi_pin) == 0u) ||
        (digitalPinIsValid(miso_pin) == 0u) ||
        (digitalPinIsValid(sck_pin) == 0u) ||
        (digitalPinIsValid(cs_pin) == 0u)) {
        return SD_ERROR_INVALID_PIN;
    }

    if ((sd_pins_conflict(mosi_pin, miso_pin) != 0u) ||
        (sd_pins_conflict(mosi_pin, sck_pin) != 0u) ||
        (sd_pins_conflict(mosi_pin, cs_pin) != 0u) ||
        (sd_pins_conflict(miso_pin, sck_pin) != 0u) ||
        (sd_pins_conflict(miso_pin, cs_pin) != 0u) ||
        (sd_pins_conflict(sck_pin, cs_pin) != 0u)) {
        return SD_ERROR_PIN_CONFLICT;
    }
    return SD_ERROR_NONE;
}

static uint8_t sd_time_expired(unsigned long start, unsigned long timeout)
{
    return ((unsigned long)(millis() - start) >= timeout) ? 1u : 0u;
}

static void sd_deselect(void)
{
    if (sd_transaction_active != 0u) {
        digitalWrite(sd_state.cs_pin, HIGH);
        (void)SPI_transfer(0xffu);
        SPI_endTransaction();
        sd_transaction_active = 0u;
    }
}

static uint8_t sd_wait_ready(void)
{
    unsigned int attempts = 0u;
    unsigned long started = millis();

    while ((attempts < SD_READY_ATTEMPT_LIMIT) &&
           (sd_time_expired(started, SD_READY_TIMEOUT_MS) == 0u)) {
        if (SPI_transfer(0xffu) == 0xffu) {
            return 1u;
        }
        ++attempts;
    }
    return 0u;
}

static uint8_t sd_wait_r1(void)
{
    unsigned int attempts = 0u;
    unsigned long started = millis();
    uint8_t response;

    while ((attempts < SD_R1_ATTEMPT_LIMIT) &&
           (sd_time_expired(started, SD_R1_TIMEOUT_MS) == 0u)) {
        response = SPI_transfer(0xffu);
        if ((response & 0x80u) == 0u) {
            return response;
        }
        ++attempts;
    }
    return 0xffu;
}

#if !defined(STC_SD_HOST_TEST) || !STC_SD_HOST_TEST
static uint8_t sd_wait_data_token(void)
{
    unsigned int attempts = 0u;
    unsigned long started = millis();
    uint8_t token = 0xffu;

    while ((attempts < SD_TOKEN_ATTEMPT_LIMIT) &&
           (sd_time_expired(started, SD_TOKEN_TIMEOUT_MS) == 0u)) {
        token = SPI_transfer(0xffu);
        if (token != 0xffu) {
            break;
        }
        ++attempts;
    }
    return token;
}
#endif

/* Leaves CS asserted only after receiving an R1 response. Any timeout path
 * releases CS before returning. */
static uint8_t sd_send_command(uint8_t command, unsigned long argument)
{
    uint8_t crc = 0x01u;
    uint8_t response;

    /* Another SPI client may have changed clock, bit order, or mode since the
     * preceding SD operation. Reassert the card transaction on every command. */
    if (SPI_beginTransactionChecked((sd_state.card_ready != 0u) ?
                         sd_data_clock_hz : SD_INIT_CLOCK_HZ,
                         MSBFIRST, SPI_MODE0) != STC_SPI_OK) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        return 0xffu;
    }
    sd_transaction_active = 1u;
    digitalWrite(sd_state.cs_pin, LOW);
    if (sd_wait_ready() == 0u) {
        sd_set_error(SD_ERROR_CARD_TIMEOUT);
        sd_deselect();
        return 0xffu;
    }

    if (command == SD_CMD0_GO_IDLE_STATE) {
        crc = 0x95u;
    } else if (command == SD_CMD8_SEND_IF_COND) {
        crc = 0x87u;
    }

    (void)SPI_transfer((uint8_t)(0x40u | command));
    (void)SPI_transfer((uint8_t)(argument >> 24));
    (void)SPI_transfer((uint8_t)(argument >> 16));
    (void)SPI_transfer((uint8_t)(argument >> 8));
    (void)SPI_transfer((uint8_t)argument);
    (void)SPI_transfer(crc);

    response = sd_wait_r1();
    if (response == 0xffu) {
        sd_set_error(SD_ERROR_CARD_TIMEOUT);
        sd_deselect();
    }
    return response;
}

static uint8_t sd_initialize_card(void)
{
    unsigned int attempts;
    unsigned long started;
    uint8_t response = 0xffu;
    uint8_t card_is_v2 = 0u;
    uint8_t index;
    uint8_t r7[4];

    digitalWrite(sd_state.cs_pin, HIGH);
    delay(1UL);
    for (index = 0u; index < 10u; ++index) {
        (void)SPI_transfer(0xffu);
    }
    sd_deselect();

    attempts = 0u;
    started = millis();
    while ((attempts < SD_INIT_ATTEMPT_LIMIT) &&
           (sd_time_expired(started, SD_INIT_TIMEOUT_MS) == 0u)) {
        response = sd_send_command(SD_CMD0_GO_IDLE_STATE, 0UL);
        sd_deselect();
        if (response == SD_R1_IDLE_STATE) {
            break;
        }
        ++attempts;
    }
    if (response != SD_R1_IDLE_STATE) {
        sd_set_error(SD_ERROR_CARD_TIMEOUT);
        return 0u;
    }

    response = sd_send_command(SD_CMD8_SEND_IF_COND, 0x000001aaUL);
    if (response == SD_R1_IDLE_STATE) {
        for (index = 0u; index < 4u; ++index) {
            r7[index] = SPI_transfer(0xffu);
        }
        sd_deselect();
        if ((r7[2] != 0x01u) || (r7[3] != 0xaau)) {
            sd_set_error(SD_ERROR_CARD_UNSUPPORTED);
            return 0u;
        }
        card_is_v2 = 1u;
    } else {
        sd_deselect();
        if (response != (SD_R1_IDLE_STATE | SD_R1_ILLEGAL_COMMAND)) {
            if (response != 0xffu) {
                sd_set_error(SD_ERROR_CARD_RESPONSE);
            }
            return 0u;
        }
    }

    attempts = 0u;
    started = millis();
    response = 0xffu;
    while ((attempts < SD_INIT_ATTEMPT_LIMIT) &&
           (sd_time_expired(started, SD_INIT_TIMEOUT_MS) == 0u)) {
        response = sd_send_command(SD_CMD55_APP_CMD, 0UL);
        sd_deselect();
        if (response > SD_R1_IDLE_STATE) {
            sd_set_error(SD_ERROR_CARD_RESPONSE);
            return 0u;
        }

        response = sd_send_command(SD_CMD41_SD_SEND_OP_COND,
                                   (card_is_v2 != 0u) ?
                                   0x40000000UL : 0UL);
        sd_deselect();
        if (response == 0u) {
            break;
        }
        if (response != SD_R1_IDLE_STATE) {
            sd_set_error(SD_ERROR_CARD_RESPONSE);
            return 0u;
        }
        ++attempts;
    }
    if (response != 0u) {
        sd_set_error(SD_ERROR_CARD_TIMEOUT);
        return 0u;
    }

    if (card_is_v2 != 0u) {
        response = sd_send_command(SD_CMD58_READ_OCR, 0UL);
        if (response != 0u) {
            sd_deselect();
            if (response != 0xffu) {
                sd_set_error(SD_ERROR_CARD_RESPONSE);
            }
            return 0u;
        }
        for (index = 0u; index < 4u; ++index) {
            r7[index] = SPI_transfer(0xffu);
        }
        sd_deselect();
        if ((r7[0] & 0x80u) == 0u) {
            sd_set_error(SD_ERROR_CARD_RESPONSE);
            return 0u;
        }
        sd_state.card_type = ((r7[0] & 0x40u) != 0u) ?
            SD_CARD_SDHC : SD_CARD_SD2;
    } else {
        sd_state.card_type = SD_CARD_SD1;
    }

    if (sd_state.card_type != SD_CARD_SDHC) {
        response = sd_send_command(SD_CMD16_SET_BLOCKLEN,
                                   (unsigned long)SD_SECTOR_SIZE);
        sd_deselect();
        if (response != 0u) {
            if (response != 0xffu) {
                sd_set_error(SD_ERROR_CARD_RESPONSE);
            }
            return 0u;
        }
    }

    sd_state.card_ready = 1u;
    sd_set_error(SD_ERROR_NONE);
    return 1u;
}

#if !defined(STC_SD_HOST_TEST) || !STC_SD_HOST_TEST
static uint8_t sd_sector_argument(unsigned long sector,
                                  unsigned long *argument)
{
    if (sd_state.card_type == SD_CARD_SDHC) {
        *argument = sector;
        return 1u;
    }
    if (sector > 0x007fffffUL) {
        sd_set_error(SD_ERROR_ADDRESS_OVERFLOW);
        return 0u;
    }
    *argument = sector << 9;
    return 1u;
}
#endif

static uint8_t sd_read_block_internal(unsigned long sector, uint8_t *buffer)
{
#if defined(STC_SD_HOST_TEST) && STC_SD_HOST_TEST
    if (sd_state.card_ready == 0u) {
        sd_set_error(SD_ERROR_NOT_INITIALIZED);
        return 0u;
    }
    if (buffer == NULL) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        return 0u;
    }
    if (STC_SD_testReadBlock(sector, buffer) == 0u) {
        sd_set_error(SD_ERROR_READ_TOKEN);
        return 0u;
    }
    sd_set_error(SD_ERROR_NONE);
    return 1u;
#else
    unsigned long argument;
    unsigned int index;
    uint8_t response;
    uint8_t token;

    if (sd_state.card_ready == 0u) {
        sd_set_error(SD_ERROR_NOT_INITIALIZED);
        return 0u;
    }
    if (buffer == NULL) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        return 0u;
    }
    if (sd_sector_argument(sector, &argument) == 0u) {
        return 0u;
    }

    response = sd_send_command(SD_CMD17_READ_SINGLE_BLOCK, argument);
    if (response != 0u) {
        sd_deselect();
        if (response != 0xffu) {
            sd_set_error(SD_ERROR_CARD_RESPONSE);
        }
        return 0u;
    }

    token = sd_wait_data_token();
    if (token != SD_DATA_START_TOKEN) {
        sd_deselect();
        sd_set_error((token == 0xffu) ? SD_ERROR_CARD_TIMEOUT :
                     SD_ERROR_READ_TOKEN);
        return 0u;
    }

    for (index = 0u; index < SD_SECTOR_SIZE; ++index) {
        buffer[index] = SPI_transfer(0xffu);
    }
    (void)SPI_transfer(0xffu);
    (void)SPI_transfer(0xffu);
    sd_deselect();
    sd_set_error(SD_ERROR_NONE);
    return 1u;
#endif
}

static uint8_t sd_write_block_internal(unsigned long sector,
                                       const uint8_t *buffer)
{
#if defined(STC_SD_HOST_TEST) && STC_SD_HOST_TEST
    if (sd_state.card_ready == 0u) {
        sd_set_error(SD_ERROR_NOT_INITIALIZED);
        return 0u;
    }
    if (buffer == NULL) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        return 0u;
    }
    if (STC_SD_testWriteBlock(sector, buffer) == 0u) {
        sd_set_error(SD_ERROR_WRITE_REJECTED);
        return 0u;
    }
    sd_set_error(SD_ERROR_NONE);
    return 1u;
#else
    unsigned long argument;
    unsigned int index;
    uint8_t response;
    uint8_t data_response;
    uint8_t card_status;

    if (sd_state.card_ready == 0u) {
        sd_set_error(SD_ERROR_NOT_INITIALIZED);
        return 0u;
    }
    if (buffer == NULL) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        return 0u;
    }
    if (sd_sector_argument(sector, &argument) == 0u) {
        return 0u;
    }

    response = sd_send_command(SD_CMD24_WRITE_BLOCK, argument);
    if (response != 0u) {
        sd_deselect();
        if (response != 0xffu) {
            sd_set_error(SD_ERROR_CARD_RESPONSE);
        }
        return 0u;
    }

    (void)SPI_transfer(0xffu);
    (void)SPI_transfer(SD_DATA_START_TOKEN);
    for (index = 0u; index < SD_SECTOR_SIZE; ++index) {
        (void)SPI_transfer(buffer[index]);
    }
    (void)SPI_transfer(0xffu);
    (void)SPI_transfer(0xffu);

    data_response = (uint8_t)(SPI_transfer(0xffu) & 0x1fu);
    if (data_response != SD_WRITE_DATA_ACCEPTED) {
        sd_deselect();
        sd_set_error(SD_ERROR_WRITE_REJECTED);
        return 0u;
    }
    if (sd_wait_ready() == 0u) {
        sd_deselect();
        sd_set_error(SD_ERROR_CARD_TIMEOUT);
        return 0u;
    }
    sd_deselect();

    response = sd_send_command(SD_CMD13_SEND_STATUS, 0UL);
    if (response != 0u) {
        sd_deselect();
        if (response != 0xffu) {
            sd_set_error(SD_ERROR_CARD_RESPONSE);
        }
        return 0u;
    }
    card_status = SPI_transfer(0xffu);
    sd_deselect();
    if (card_status != 0u) {
        sd_set_error(SD_ERROR_WRITE_STATUS);
        return 0u;
    }

    sd_set_error(SD_ERROR_NONE);
    return 1u;
#endif
}

/* FAT fields are decoded byte by byte. This is required for the big-endian
 * MCS251 backend as well as for unaligned BPB and directory fields. */
static unsigned int sd_load_le16(unsigned int offset)
{
    return (unsigned int)sd_sector[offset + 0u] |
        ((unsigned int)sd_sector[offset + 1u] << 8);
}

static unsigned long sd_load_le32(unsigned int offset)
{
    return (unsigned long)sd_sector[offset + 0u] |
        ((unsigned long)sd_sector[offset + 1u] << 8) |
        ((unsigned long)sd_sector[offset + 2u] << 16) |
        ((unsigned long)sd_sector[offset + 3u] << 24);
}

static uint8_t sd_add_u32(unsigned long left, unsigned long right,
                          unsigned long *result)
{
    if (left > (SD_UINT32_MAX - right)) {
        return 0u;
    }
    *result = left + right;
    return 1u;
}

static uint8_t sd_boot_sector_plausible(void)
{
    uint8_t sectors_per_cluster = sd_sector[13];
    unsigned int reserved = sd_load_le16(14);
    uint8_t fats = sd_sector[16];
    unsigned long total = (unsigned long)sd_load_le16(19);
    unsigned long fat_size = (unsigned long)sd_load_le16(22);

    if (total == 0UL) {
        total = sd_load_le32(32);
    }
    if (fat_size == 0UL) {
        fat_size = sd_load_le32(36);
    }

    return ((sd_sector[510] == 0x55u) &&
            (sd_sector[511] == 0xaau) &&
            (sd_load_le16(11) == SD_SECTOR_SIZE) &&
            (sectors_per_cluster != 0u) &&
            ((sectors_per_cluster &
              (uint8_t)(sectors_per_cluster - 1u)) == 0u) &&
            (sectors_per_cluster <= 128u) &&
            (reserved != 0u) &&
            ((fats == 1u) || (fats == 2u)) &&
            (total != 0UL) &&
            (fat_size != 0UL)) ? 1u : 0u;
}

static uint8_t sd_mbr_partition_type_supported(uint8_t type)
{
    switch (type) {
    case 0x01u: /* FAT12; parser will return FAT12_UNSUPPORTED explicitly. */
    case 0x04u: /* FAT16, less than 32 MiB */
    case 0x06u: /* FAT16 */
    case 0x0bu: /* FAT32 CHS */
    case 0x0cu: /* FAT32 LBA */
    case 0x0eu: /* FAT16 LBA */
    case 0x11u: /* Hidden FAT12 */
    case 0x14u: /* Hidden FAT16, less than 32 MiB */
    case 0x16u: /* Hidden FAT16 */
    case 0x1bu: /* Hidden FAT32 CHS */
    case 0x1cu: /* Hidden FAT32 LBA */
    case 0x1eu: /* Hidden FAT16 LBA */
        return 1u;
    default:
        return 0u;
    }
}

static uint8_t sd_flush_cache(void)
{
    if ((sd_state.cache_valid == 0u) ||
        (sd_state.cache_dirty == 0u)) {
        return 1u;
    }
    if (sd_write_block_internal(sd_state.cache_sector, sd_sector) == 0u) {
        return 0u;
    }
    sd_state.cache_dirty = 0u;
    return 1u;
}

static uint8_t sd_prepare_reconfiguration(void) STC_SD_REENTRANT
{
    if (sd_active_file.open != 0u) {
        SD_close();
        if (sd_state.last_error != SD_ERROR_NONE) {
            return 0u;
        }
    }
    return sd_flush_cache();
}

static uint8_t sd_store_loaded_sector(void)
{
    if (sd_state.cache_valid == 0u) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        return 0u;
    }
    sd_state.cache_dirty = 1u;
    return sd_flush_cache();
}

static uint8_t sd_load_sector(unsigned long sector)
{
    if ((sd_state.cache_valid != 0u) &&
        (sd_state.cache_sector == sector)) {
        return 1u;
    }
    if (sd_flush_cache() == 0u) {
        return 0u;
    }
    if (sd_read_block_internal(sector, sd_sector) == 0u) {
        sd_state.cache_valid = 0u;
        sd_state.cache_dirty = 0u;
        return 0u;
    }
    sd_state.cache_sector = sector;
    sd_state.cache_valid = 1u;
    sd_state.cache_dirty = 0u;
    return 1u;
}

static uint8_t sd_parse_volume(unsigned long volume_start,
                               unsigned long partition_sectors)
{
    uint8_t sectors_per_cluster;
    uint8_t fat_count;
    unsigned int reserved;
    unsigned int root_entries;
    unsigned long total_sectors;
    unsigned long fat_sectors;
    unsigned long root_dir_sectors;
    unsigned long fat_area_sectors;
    unsigned long root_relative;
    unsigned long data_relative;
    unsigned long data_sectors;
    unsigned long cluster_count;
    unsigned long required_fat_sectors;
    unsigned long fat_relative;
    unsigned long value;
    unsigned long volume_end;
    unsigned int fat32_flags = 0u;
    uint8_t active_fat = 0u;
    uint8_t fat_type;

    if (sd_boot_sector_plausible() == 0u) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }

    sectors_per_cluster = sd_sector[13];
    reserved = sd_load_le16(14);
    fat_count = sd_sector[16];
    root_entries = sd_load_le16(17);
    total_sectors = (unsigned long)sd_load_le16(19);
    if (total_sectors == 0UL) {
        total_sectors = sd_load_le32(32);
    }
    fat_sectors = (unsigned long)sd_load_le16(22);
    if (fat_sectors == 0UL) {
        fat_sectors = sd_load_le32(36);
    }

    if ((partition_sectors != 0UL) &&
        (total_sectors > partition_sectors)) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }
    if (sd_add_u32(volume_start, total_sectors, &volume_end) == 0u) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }
    (void)volume_end;

    root_dir_sectors =
        (((unsigned long)root_entries * SD_DIR_ENTRY_SIZE) +
         (SD_SECTOR_SIZE - 1u)) / SD_SECTOR_SIZE;
    if (fat_sectors > (fat_count == 2u ? 0x7fffffffUL : SD_UINT32_MAX)) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }
    fat_area_sectors = fat_count == 2u ? fat_sectors << 1 : fat_sectors;
    if (sd_add_u32((unsigned long)reserved, fat_area_sectors,
                   &root_relative) == 0u ||
        sd_add_u32(root_relative, root_dir_sectors,
                   &data_relative) == 0u ||
        (data_relative >= total_sectors)) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }

    data_sectors = total_sectors - data_relative;
    sd_state.cluster_shift = 0u;
    value = sectors_per_cluster;
    while (value > 1UL) {
        value >>= 1;
        ++sd_state.cluster_shift;
    }
    cluster_count = data_sectors >> sd_state.cluster_shift;
    if (cluster_count < 4085UL) {
        sd_set_error(SD_ERROR_FAT12_UNSUPPORTED);
        return 0u;
    }
    fat_type = (cluster_count < 65525UL) ? SD_FAT16 : SD_FAT32;

    if (fat_type == SD_FAT16) {
        if ((root_entries == 0u) ||
            (sd_load_le16(22) == 0u)) {
            sd_set_error(SD_ERROR_BAD_VOLUME);
            return 0u;
        }
        required_fat_sectors =
            (cluster_count + 2UL + 255UL) / 256UL;
    } else {
        if ((root_entries != 0u) ||
            (sd_load_le16(22) != 0u) ||
            (sd_load_le16(42) != 0u) ||
            (cluster_count > 0x0ffffff5UL)) {
            sd_set_error(SD_ERROR_BAD_VOLUME);
            return 0u;
        }
        fat32_flags = sd_load_le16(40);
        if ((fat32_flags & 0x0080u) != 0u) {
            active_fat = (uint8_t)(fat32_flags & 0x000fu);
            if (active_fat >= fat_count) {
                sd_set_error(SD_ERROR_BAD_VOLUME);
                return 0u;
            }
        }
        required_fat_sectors =
            (cluster_count + 2UL + 127UL) / 128UL;
    }
    if (fat_sectors < required_fat_sectors) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }

    if (sd_add_u32(volume_start, (unsigned long)reserved,
                   &sd_state.fat_base_start) == 0u) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }
    fat_relative = active_fat ? fat_sectors : 0UL;
    if (sd_add_u32(sd_state.fat_base_start, fat_relative, &value) == 0u) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }
    sd_state.fat_start = value;
    if (sd_add_u32(volume_start, root_relative, &value) == 0u) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }
    sd_state.root_dir_start = value;
    if (sd_add_u32(volume_start, data_relative, &value) == 0u) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }
    sd_state.data_start = value;

    sd_state.root_cluster = 0UL;
    if (fat_type == SD_FAT32) {
        sd_state.root_cluster = sd_load_le32(44) & 0x0fffffffUL;
        if ((sd_state.root_cluster < 2UL) ||
            (sd_state.root_cluster > (cluster_count + 1UL))) {
            sd_set_error(SD_ERROR_BAD_VOLUME);
            return 0u;
        }
    }

    sd_state.volume_start = volume_start;
    sd_state.volume_sectors = total_sectors;
    sd_state.fat_sectors = fat_sectors;
    sd_state.root_dir_sectors = root_dir_sectors;
    sd_state.sectors_per_cluster = sectors_per_cluster;
    sd_state.fat_count = fat_count;
    sd_state.active_fat = active_fat;
    sd_state.fat_mirroring = ((fat_type == SD_FAT32) &&
                              ((fat32_flags & 0x0080u) != 0u)) ? 0u : 1u;
    sd_state.root_entry_count = root_entries;
    sd_state.cluster_count = cluster_count;
    sd_state.fat_type = fat_type;
    sd_state.allocation_hint = 2UL;
    sd_state.cache_valid = 0u;
    sd_state.cache_dirty = 0u;
    sd_state.mounted = 1u;
    sd_set_error(SD_ERROR_NONE);
    return 1u;
}

static uint8_t sd_mount(void)
{
    unsigned long volume_start;
    unsigned long partition_sectors;
    uint8_t first_error;
    uint8_t mbr_candidate;

    sd_clear_volume();
    if (sd_read_block_internal(0UL, sd_sector) == 0u) {
        return 0u;
    }
    sd_state.cache_sector = 0UL;
    sd_state.cache_valid = 1u;

    /* Match the conventional SD layout probe order: MBR partition 1 first,
     * then sector-zero superfloppy. Re-read LBA0 before fallback because the
     * one cache buffer was overwritten by the partition boot-sector attempt. */
    volume_start = sd_load_le32(454);
    partition_sectors = sd_load_le32(458);
    mbr_candidate = ((sd_sector[510] == 0x55u) &&
                     (sd_sector[511] == 0xaau) &&
                     ((sd_sector[446] == 0x00u) ||
                      (sd_sector[446] == 0x80u)) &&
                     (sd_mbr_partition_type_supported(sd_sector[450]) != 0u) &&
                     (volume_start != 0UL) &&
                     (partition_sectors != 0UL) &&
                     (volume_start <=
                      (SD_UINT32_MAX - partition_sectors))) ? 1u : 0u;
    first_error = SD_ERROR_NONE;

    if (mbr_candidate != 0u) {
        if (sd_read_block_internal(volume_start, sd_sector) != 0u) {
            sd_state.cache_sector = volume_start;
            sd_state.cache_valid = 1u;
            if (sd_parse_volume(volume_start, partition_sectors) != 0u) {
                return 1u;
            }
        }
        first_error = sd_state.last_error;

        if (sd_read_block_internal(0UL, sd_sector) == 0u) {
            if (first_error != SD_ERROR_NONE) {
                sd_set_error(first_error);
            }
            return 0u;
        }
        sd_state.cache_sector = 0UL;
        sd_state.cache_valid = 1u;
    }

    if ((sd_boot_sector_plausible() != 0u) &&
        (sd_parse_volume(0UL, 0UL) != 0u)) {
        return 1u;
    }
    if (first_error != SD_ERROR_NONE) {
        sd_set_error(first_error);
    } else if (sd_state.last_error == SD_ERROR_NONE) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
    }
    return 0u;
}

static uint8_t sd_cluster_to_sector(unsigned long cluster,
                                    unsigned long *sector)
{
    unsigned long relative;

    if ((cluster < 2UL) ||
        (cluster > (sd_state.cluster_count + 1UL))) {
        sd_set_error(SD_ERROR_BAD_CLUSTER);
        return 0u;
    }
    relative = cluster - 2UL;
    if (relative >
        (SD_UINT32_MAX >> sd_state.cluster_shift)) {
        sd_set_error(SD_ERROR_BAD_CLUSTER);
        return 0u;
    }
    relative <<= sd_state.cluster_shift;
    if (sd_add_u32(sd_state.data_start, relative, sector) == 0u) {
        sd_set_error(SD_ERROR_BAD_CLUSTER);
        return 0u;
    }
    return 1u;
}

static uint8_t sd_read_fat_entry(unsigned long cluster,
    unsigned long *value) STC_SD_REENTRANT;
static uint8_t sd_fat_value_is_end(unsigned long value);
static uint8_t sd_next_cluster(unsigned long cluster,
    unsigned long *next_cluster, uint8_t *is_end)
{
    unsigned long value;
    if (!sd_read_fat_entry(cluster, &value)) {
        return 0u;
    }
    if (sd_fat_value_is_end(value)) {
        *is_end = 1u;
        *next_cluster = 0UL;
        return 1u;
    }
    if (value < 2UL || value > sd_state.cluster_count + 1UL ||
        value == (sd_state.fat_type == SD_FAT16 ? 0xfff7UL : 0x0ffffff7UL)) {
        sd_set_error(SD_ERROR_BAD_CLUSTER);
        return 0u;
    }
    *is_end = 0u;
    *next_cluster = value;
    return 1u;
}

static void sd_store_le16(unsigned int offset, unsigned int value)
{
    sd_sector[offset + 0u] = (uint8_t)(value & 0xffu);
    sd_sector[offset + 1u] = (uint8_t)((value >> 8) & 0xffu);
}

static void sd_store_le32(unsigned int offset, unsigned long value)
{
    sd_sector[offset + 0u] = (uint8_t)(value & 0xffUL);
    sd_sector[offset + 1u] = (uint8_t)((value >> 8) & 0xffUL);
    sd_sector[offset + 2u] = (uint8_t)((value >> 16) & 0xffUL);
    sd_sector[offset + 3u] = (uint8_t)((value >> 24) & 0xffUL);
}

static uint8_t sd_fat_entry_location(unsigned long fat_start,
                                     unsigned long cluster,
                                     unsigned long *sector,
                                     unsigned int *offset) STC_SD_REENTRANT
{
    unsigned long byte_offset;

    if ((cluster < 2UL) ||
        (cluster > (sd_state.cluster_count + 1UL))) {
        sd_set_error(SD_ERROR_BAD_CLUSTER);
        return 0u;
    }
    if (sd_state.fat_type == SD_FAT16) {
        byte_offset = cluster << 1;
    } else {
        if (cluster > 0x3fffffffUL) {
            sd_set_error(SD_ERROR_BAD_CLUSTER);
            return 0u;
        }
        byte_offset = cluster << 2;
    }
    if ((byte_offset / SD_SECTOR_SIZE) >= sd_state.fat_sectors ||
        sd_add_u32(fat_start, byte_offset / SD_SECTOR_SIZE,
                   sector) == 0u) {
        sd_set_error(SD_ERROR_BAD_CLUSTER);
        return 0u;
    }
    *offset = (unsigned int)(byte_offset & 0x01ffUL);
    return 1u;
}

static uint8_t sd_read_fat_entry(unsigned long cluster,
                                 unsigned long *value) STC_SD_REENTRANT
{
    unsigned long sector;
    unsigned int offset;

    if ((value == NULL) ||
        (sd_fat_entry_location(sd_state.fat_start, cluster,
                               &sector, &offset) == 0u) ||
        (sd_load_sector(sector) == 0u)) {
        if (value == NULL) {
            sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        }
        return 0u;
    }
    if (sd_state.fat_type == SD_FAT16) {
        *value = (unsigned long)sd_load_le16(offset);
    } else {
        *value = sd_load_le32(offset) & 0x0fffffffUL;
    }
    return 1u;
}

static uint8_t sd_fat_value_is_end(unsigned long value)
{
    return (sd_state.fat_type == SD_FAT16) ?
        ((value >= 0xfff8UL) ? 1u : 0u) :
        ((value >= 0x0ffffff8UL) ? 1u : 0u);
}

static unsigned long sd_fat_end_marker(void)
{
    return (sd_state.fat_type == SD_FAT16) ?
        0xffffUL : 0x0fffffffUL;
}

static uint8_t sd_write_fat_entry(unsigned long cluster,
                                  unsigned long value) STC_SD_REENTRANT
{
    unsigned long sectors[2];
    unsigned long originals[2];
    unsigned long raw;
    unsigned long start;
    unsigned int offsets[2];
    uint8_t copies;
    uint8_t copy;
    uint8_t actual_copy;
    uint8_t written = 0u;
    uint8_t saved_error = SD_ERROR_BAD_CLUSTER;

    copies = (sd_state.fat_mirroring != 0u) ? sd_state.fat_count : 1u;
    if ((copies == 0u) || (copies > 2u)) {
        sd_set_error(SD_ERROR_BAD_VOLUME);
        return 0u;
    }

    /* Capture each exact on-disk value first. FAT32 reserves its high nibble,
     * which is preserved independently in every mirror. */
    for (copy = 0u; copy < copies; ++copy) {
        actual_copy = (sd_state.fat_mirroring != 0u) ?
            copy : sd_state.active_fat;
        start = sd_state.fat_base_start +
            ((unsigned long)actual_copy * sd_state.fat_sectors);
        if ((sd_fat_entry_location(start, cluster, &sectors[copy],
                                   &offsets[copy]) == 0u) ||
            (sd_load_sector(sectors[copy]) == 0u)) {
            return 0u;
        }
        originals[copy] = (sd_state.fat_type == SD_FAT16) ?
            (unsigned long)sd_load_le16(offsets[copy]) :
            sd_load_le32(offsets[copy]);
    }

    for (copy = 0u; copy < copies; ++copy) {
        if (sd_load_sector(sectors[copy]) == 0u) {
            saved_error = sd_state.last_error;
            break;
        }
        if (sd_state.fat_type == SD_FAT16) {
            sd_store_le16(offsets[copy],
                          (unsigned int)(value & 0xffffUL));
        } else {
            raw = (originals[copy] & 0xf0000000UL) |
                (value & 0x0fffffffUL);
            sd_store_le32(offsets[copy], raw);
        }
        if (sd_store_loaded_sector() == 0u) {
            saved_error = sd_state.last_error;
            /* A rejected single-block write cannot be treated as a cache that
             * is safe to carry into a different mirror or metadata sector. */
            sd_state.cache_dirty = 0u;
            sd_state.cache_valid = 0u;
            break;
        }
        ++written;
    }
    if (written == copies) {
        return 1u;
    }

    /* Best-effort rollback keeps a failure in the second FAT copy from being
     * reported as success with knowingly divergent mirrors. */
    while (written != 0u) {
        --written;
        if (sd_load_sector(sectors[written]) != 0u) {
            if (sd_state.fat_type == SD_FAT16) {
                sd_store_le16(offsets[written],
                              (unsigned int)originals[written]);
            } else {
                sd_store_le32(offsets[written],
                              originals[written]);
            }
            if (sd_store_loaded_sector() == 0u) {
                sd_state.cache_dirty = 0u;
                sd_state.cache_valid = 0u;
            }
        }
    }
    sd_set_error(saved_error);
    return 0u;
}

static uint8_t sd_zero_cluster(unsigned long cluster) STC_SD_REENTRANT
{
    unsigned long first_sector;
    unsigned int index;
    uint8_t sector_index;

    if ((sd_cluster_to_sector(cluster, &first_sector) == 0u) ||
        (sd_flush_cache() == 0u)) {
        return 0u;
    }
    for (index = 0u; index < SD_SECTOR_SIZE; ++index) {
        sd_sector[index] = 0u;
    }
    sd_state.cache_valid = 0u;
    sd_state.cache_dirty = 0u;
    for (sector_index = 0u;
         sector_index < sd_state.sectors_per_cluster;
         ++sector_index) {
        if (sd_write_block_internal(first_sector +
                                    (unsigned long)sector_index,
                                    sd_sector) == 0u) {
            return 0u;
        }
    }
    return 1u;
}

static uint8_t sd_allocate_cluster(unsigned long *allocated) STC_SD_REENTRANT
{
    unsigned long candidate;
    unsigned long maximum = sd_state.cluster_count + 1UL;
    unsigned long scanned;
    unsigned long value;
    uint8_t saved_error;

    if (allocated == NULL) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        return 0u;
    }
    candidate = sd_state.allocation_hint;
    if ((candidate < 2UL) || (candidate > maximum)) {
        candidate = 2UL;
    }
    for (scanned = 0UL; scanned < sd_state.cluster_count; ++scanned) {
        if (sd_read_fat_entry(candidate, &value) == 0u) {
            return 0u;
        }
        if (value == 0UL) {
            if (sd_write_fat_entry(candidate, sd_fat_end_marker()) == 0u) {
                return 0u;
            }
            if (sd_zero_cluster(candidate) == 0u) {
                saved_error = sd_state.last_error;
                (void)sd_write_fat_entry(candidate, 0UL);
                sd_set_error(saved_error);
                return 0u;
            }
            *allocated = candidate;
            sd_state.allocation_hint = (candidate == maximum) ?
                2UL : candidate + 1UL;
            return 1u;
        }
        candidate = (candidate == maximum) ? 2UL : candidate + 1UL;
    }
    sd_set_error(SD_ERROR_NO_SPACE);
    return 0u;
}

static uint8_t sd_extend_cluster(unsigned long tail,
                                 unsigned long *allocated) STC_SD_REENTRANT
{
    unsigned long fresh;
    uint8_t saved_error;

    if (sd_allocate_cluster(&fresh) == 0u) {
        return 0u;
    }
    if (sd_write_fat_entry(tail, fresh) == 0u) {
        saved_error = sd_state.last_error;
        (void)sd_write_fat_entry(fresh, 0UL);
        sd_set_error(saved_error);
        return 0u;
    }
    *allocated = fresh;
    return 1u;
}

static uint8_t sd_release_cluster_chain(unsigned long first) STC_SD_REENTRANT
{
    unsigned long current = first;
    unsigned long next;
    unsigned long hops = 0UL;
    unsigned long value;
    uint8_t is_end;

    if (first == 0UL) {
        return 1u;
    }
    while (hops < sd_state.cluster_count) {
        if (sd_read_fat_entry(current, &value) == 0u) {
            return 0u;
        }
        is_end = sd_fat_value_is_end(value);
        next = value;
        if ((is_end == 0u) &&
            ((next < 2UL) ||
             (next > (sd_state.cluster_count + 1UL)))) {
            sd_set_error(SD_ERROR_BAD_CLUSTER);
            return 0u;
        }
        if (sd_write_fat_entry(current, 0UL) == 0u) {
            return 0u;
        }
        if (current < sd_state.allocation_hint) {
            sd_state.allocation_hint = current;
        }
        if (is_end != 0u) {
            return 1u;
        }
        current = next;
        ++hops;
    }
    sd_set_error(SD_ERROR_BAD_CLUSTER);
    return 0u;
}

static uint8_t sd_short_name_character_valid(uint8_t value)
{
    if ((value <= 0x20u) || (value >= 0x7fu)) {
        return 0u;
    }
    switch (value) {
    case '"':
    case '*':
    case '+':
    case ',':
    case '/':
    case ':':
    case ';':
    case '<':
    case '=':
    case '>':
    case '?':
    case '[':
    case '\\':
    case ']':
    case '|':
        return 0u;
    default:
        return 1u;
    }
}

static uint8_t sd_format_short_name(const char *name, uint8_t *formatted)
{
    uint8_t index;
    uint8_t base_length = 0u;
    uint8_t extension_length = 0u;
    uint8_t in_extension = 0u;
    uint8_t value;

    if ((name == NULL) || (formatted == NULL)) {
        return 0u;
    }
    if (*name == '/') {
        ++name;
    }
    if (*name == '\0') {
        return 0u;
    }
    for (index = 0u; index < 11u; ++index) {
        formatted[index] = (uint8_t)' ';
    }

    while (*name != '\0') {
        value = (uint8_t)*name++;
        if (value == (uint8_t)'.') {
            if ((in_extension != 0u) || (base_length == 0u)) {
                return 0u;
            }
            in_extension = 1u;
            continue;
        }
        if (sd_short_name_character_valid(value) == 0u) {
            return 0u;
        }
        if ((value >= (uint8_t)'a') && (value <= (uint8_t)'z')) {
            value = (uint8_t)(value - ((uint8_t)'a' - (uint8_t)'A'));
        }
        if (in_extension == 0u) {
            if (base_length >= 8u) {
                return 0u;
            }
            formatted[base_length++] = value;
        } else {
            if (extension_length >= 3u) {
                return 0u;
            }
            formatted[8u + extension_length++] = value;
        }
    }
    if ((base_length == 0u) ||
        ((in_extension != 0u) && (extension_length == 0u))) {
        return 0u;
    }
    return 1u;
}

/* Private FAT directory operations. Included after the FAT allocation code.
 * Directory 0 denotes the volume root on both FAT16 and FAT32. */
static uint8_t sd_directory_location(unsigned long directory, unsigned long index,
    uint8_t mode, unsigned long *sector, unsigned int *offset) STC_SD_REENTRANT
{
    unsigned long cluster, next, hops, sector_index = index / 16UL;
    uint8_t end, extend = mode & 1u;
    if (sector_index >= SD_ROOT_SCAN_SECTOR_LIMIT) {
        sd_set_error(SD_ERROR_DIRECTORY_FULL); return 0u;
    }
    *offset = (unsigned int)(index % 16UL) * 32u;
    if (!directory && sd_state.fat_type == SD_FAT16) {
        if (index >= sd_state.root_entry_count) {
            sd_set_error(extend ? SD_ERROR_DIRECTORY_FULL : SD_ERROR_NOT_FOUND); return 0u;
        }
        *sector = sd_state.root_dir_start + sector_index;
        return 1u;
    }
    cluster = directory ? directory : sd_state.root_cluster;
    hops = sector_index >> sd_state.cluster_shift;
    if (hops >= sd_state.cluster_count) {
        sd_set_error(SD_ERROR_BAD_CLUSTER); return 0u;
    }
    /* Mode 2 is directory-handle iteration. Reuse the file context's cluster
     * cursor without increasing its ABI size or adding a second cache. */
    if ((mode & 2u) && sd_active_file.cluster >= 2UL &&
        hops >= sd_active_file.cluster_index) {
        cluster = sd_active_file.cluster;
        hops -= sd_active_file.cluster_index;
    }
    while (hops--) {
        if (!sd_next_cluster(cluster, &next, &end)) return 0u;
        if (end) {
            if (!extend) { sd_set_error(SD_ERROR_NOT_FOUND); return 0u; }
            if (!sd_extend_cluster(cluster, &next)) return 0u;
        }
        cluster = next;
    }
    if (!sd_cluster_to_sector(cluster, sector)) return 0u;
    if (mode & 2u) {
        sd_active_file.cluster = cluster;
        sd_active_file.cluster_index = sector_index >> sd_state.cluster_shift;
    }
    *sector += sector_index & (unsigned long)(sd_state.sectors_per_cluster - 1u);
    return 1u;
}
static void sd_decode_entry(unsigned long sector, unsigned int offset,
    STCSDDirEntry *entry) STC_SD_REENTRANT
{
    entry->sector = sector; entry->offset = offset;
    entry->attribute = sd_sector[offset + SD_DIR_ATTRIBUTE];
    entry->cluster = sd_load_le16(offset + SD_DIR_CLUSTER_LOW);
    if (sd_state.fat_type == SD_FAT32)
        entry->cluster |= ((unsigned long)sd_load_le16(offset + SD_DIR_CLUSTER_HIGH) & 0x0fffUL) << 16;
    entry->size = sd_load_le32(offset + SD_DIR_FILE_SIZE);
}
static uint8_t sd_find_entry(unsigned long directory, const uint8_t *target,
    STCSDDirEntry *entry) STC_SD_REENTRANT
{
    unsigned long index = 0UL, sector;
    unsigned int offset;
    uint8_t i, match;
    for (;;) {
        if (!sd_directory_location(directory, index++, 0u, &sector, &offset) ||
            !sd_load_sector(sector)) return 0u;
        if (!sd_sector[offset]) { sd_set_error(SD_ERROR_NOT_FOUND); return 0u; }
        if (sd_sector[offset] == 0xe5u || (sd_sector[offset + 11u] & SD_ATTRIBUTE_VOLUME_ID)) continue;
        match = 1u;
        for (i = 0u; i < 11u; ++i) if (target[i] != sd_sector[offset + i]) match = 0u;
        if (match) { sd_decode_entry(sector, offset, entry); return 1u; }
    }
}
static uint8_t sd_create_entry(unsigned long directory, const uint8_t *target,
    uint8_t attribute, unsigned long cluster, STCSDDirEntry *entry) STC_SD_REENTRANT
{
    unsigned long index = 0UL, sector;
    unsigned int offset;
    uint8_t i;
    for (;;) {
        if (!sd_directory_location(directory, index++, 1u, &sector, &offset) ||
            !sd_load_sector(sector)) return 0u;
        if (!sd_sector[offset] || sd_sector[offset] == 0xe5u) break;
    }
    for (i = 0u; i < 32u; ++i) sd_sector[offset + i] = 0u;
    for (i = 0u; i < 11u; ++i) sd_sector[offset + i] = target[i];
    sd_sector[offset + 11u] = attribute;
    sd_store_le16(offset + SD_DIR_CLUSTER_LOW, (unsigned int)cluster);
    if (sd_state.fat_type == SD_FAT32)
        sd_store_le16(offset + SD_DIR_CLUSTER_HIGH, (unsigned int)(cluster >> 16));
    sd_decode_entry(sector, offset, entry);
    return sd_store_loaded_sector();
}
static uint8_t sd_make_directory(unsigned long parent, const uint8_t *target,
    STCSDDirEntry *entry) STC_SD_REENTRANT
{
    unsigned long cluster, sector;
    unsigned int offset;
    uint8_t i, error;
    if (!sd_allocate_cluster(&cluster)) return 0u;
    if (!sd_cluster_to_sector(cluster, &sector) || !sd_load_sector(sector)) goto failed;
    for (offset = 0u; offset <= 32u; offset += 32u) {
        for (i = 0u; i < 11u; ++i) sd_sector[offset + i] = ' ';
        sd_sector[offset] = '.';
        if (offset) sd_sector[offset + 1u] = '.';
        sd_sector[offset + 11u] = SD_ATTRIBUTE_DIRECTORY;
        sd_store_le16(offset + SD_DIR_CLUSTER_LOW, (unsigned int)(offset ? parent : cluster));
        if (sd_state.fat_type == SD_FAT32)
            sd_store_le16(offset + SD_DIR_CLUSTER_HIGH, (unsigned int)((offset ? parent : cluster) >> 16));
    }
    if (!sd_store_loaded_sector()) goto failed;
    if (!sd_create_entry(parent, target, SD_ATTRIBUTE_DIRECTORY, cluster, entry)) goto failed;
    return 1u;
failed:
    error = sd_state.last_error;
    /* The parent entry was not committed; reclaim the unpublished cluster. */
    (void)sd_release_cluster_chain(cluster);
    sd_set_error(error); return 0u;
}
static uint8_t sd_component(const char *text, uint8_t *target) STC_SD_REENTRANT
{
    uint8_t i;
    if (text[0] == '.' && (!text[1] || (text[1] == '.' && !text[2]))) {
        for (i = 0u; i < 11u; ++i) target[i] = ' ';
        target[0] = '.';
        if (text[1]) target[1] = '.';
        return 1u;
    }
    return sd_format_short_name(text, target);
}
static uint8_t sd_is_root(const char *name)
{
    if (!name || !*name) return 0u;
    for (;;) {
        while (*name == '/') ++name;
        if (!*name) return 1u;
        if (*name++ != '.') return 0u;
        if (*name == '.') ++name;
        if (*name && *name != '/') return 0u;
    }
}
static uint8_t sd_parent(const char *path, unsigned long *parent,
    uint8_t *target, uint8_t create_parents) STC_SD_REENTRANT
{
    char component[13];
    uint8_t count;
    STCSDDirEntry entry;
    *parent = 0UL;
    if (!path || !*path) { sd_set_error(SD_ERROR_INVALID_ARGUMENT); return 0u; }
    while (*path == '/') ++path;
    for (;;) {
        count = 0u;
        while (*path && *path != '/') {
            if (count == 12u) { sd_set_error(SD_ERROR_INVALID_ARGUMENT); return 0u; }
            component[count++] = *path++;
        }
        component[count] = '\0';
        if (!sd_component(component, target)) { sd_set_error(SD_ERROR_INVALID_ARGUMENT); return 0u; }
        while (*path == '/') ++path;
        if (!*path) return 1u;
        if (component[0] == '.' && !component[1]) continue;
        if (!*parent && component[0] == '.' && component[1] == '.' && !component[2]) continue;
        if (!sd_find_entry(*parent, target, &entry)) {
            if (!create_parents || sd_state.last_error != SD_ERROR_NOT_FOUND ||
                !sd_make_directory(*parent, target, &entry)) return 0u;
        }
        if (!(entry.attribute & SD_ATTRIBUTE_DIRECTORY)) {
            sd_set_error(SD_ERROR_NOT_A_DIRECTORY); return 0u;
        }
        *parent = entry.cluster;
    }
}
static uint8_t sd_open_entry(const STCSDDirEntry *entry, const uint8_t *target,
    uint8_t mode) STC_SD_REENTRANT
{
    uint8_t i, writable = (mode & SD_MODE_WRITE) != 0u;
    if ((entry->attribute & SD_ATTRIBUTE_DIRECTORY) && writable) {
        sd_set_error(SD_ERROR_NOT_A_FILE); return 0u;
    }
    if (writable && (entry->attribute & SD_ATTRIBUTE_READ_ONLY)) {
        sd_set_error(SD_ERROR_READ_ONLY); return 0u;
    }
    if (entry->size && (entry->cluster < 2UL || entry->cluster > sd_state.cluster_count + 1UL)) {
        sd_set_error(SD_ERROR_BAD_CLUSTER); return 0u;
    }
    sd_clear_file();
    sd_active_file.open = 1u; sd_active_file.writable = writable;
    sd_active_file.is_directory = (entry->attribute & SD_ATTRIBUTE_DIRECTORY) != 0u;
    sd_active_file.dir_sector = entry->sector; sd_active_file.dir_offset = entry->offset;
    for (i = 0u; i < 11u; ++i) sd_active_file.name[i] = target[i];
    sd_active_file.first_cluster = sd_active_file.cluster = entry->cluster;
    sd_active_file.size = entry->size;
    sd_active_file.position = writable && (mode & SD_MODE_APPEND) ? entry->size : 0UL;
    sd_save_file();
    sd_set_error(SD_ERROR_NONE); return 1u;
}


static uint8_t sd_update_open_file_metadata(void) STC_SD_REENTRANT
{
    unsigned int index;
    unsigned int offset;

    if (sd_active_file.metadata_dirty == 0u) {
        return 1u;
    }
    if (sd_load_sector(sd_active_file.dir_sector) == 0u) {
        return 0u;
    }
    offset = sd_active_file.dir_offset;
    if ((offset > (SD_SECTOR_SIZE - SD_DIR_ENTRY_SIZE)) ||
        (sd_sector[offset] == 0x00u) ||
        (sd_sector[offset] == 0xe5u)) {
        sd_set_error(SD_ERROR_NOT_FOUND);
        return 0u;
    }
    for (index = 0u; index < 11u; ++index) {
        if (sd_sector[offset + index] != sd_active_file.name[index]) {
            sd_set_error(SD_ERROR_NOT_FOUND);
            return 0u;
        }
    }
    sd_store_le16(offset + SD_DIR_CLUSTER_LOW,
                  (unsigned int)(sd_active_file.first_cluster & 0xffffUL));
    if (sd_state.fat_type == SD_FAT32) {
        sd_store_le16(offset + SD_DIR_CLUSTER_HIGH,
                      (unsigned int)((sd_active_file.first_cluster >> 16) &
                                     0x0fffUL));
    }
    sd_store_le32(offset + SD_DIR_FILE_SIZE, sd_active_file.size);
    if (sd_store_loaded_sector() == 0u) {
        return 0u;
    }
    sd_active_file.metadata_dirty = 0u;
    return 1u;
}

static uint8_t sd_get_file_cluster(unsigned long target_index,
                                   uint8_t allocate,
                                   unsigned long *cluster) STC_SD_REENTRANT
{
    unsigned long next_cluster;
    uint8_t is_end;

    if ((cluster == NULL) || (target_index >= sd_state.cluster_count)) {
        sd_set_error((cluster == NULL) ? SD_ERROR_INVALID_ARGUMENT :
                     SD_ERROR_NO_SPACE);
        return 0u;
    }
    if (sd_active_file.first_cluster < 2UL) {
        if ((allocate == 0u) ||
            (sd_allocate_cluster(&sd_active_file.first_cluster) == 0u)) {
            if (allocate == 0u) {
                sd_set_error(SD_ERROR_BAD_CLUSTER);
            }
            return 0u;
        }
        sd_active_file.cluster = sd_active_file.first_cluster;
        sd_active_file.cluster_index = 0UL;
        sd_active_file.metadata_dirty = 1u;
    }
    if ((sd_active_file.cluster < 2UL) ||
        (target_index < sd_active_file.cluster_index)) {
        sd_active_file.cluster = sd_active_file.first_cluster;
        sd_active_file.cluster_index = 0UL;
    }
    while (sd_active_file.cluster_index < target_index) {
        if (sd_next_cluster(sd_active_file.cluster, &next_cluster,
                            &is_end) == 0u) {
            return 0u;
        }
        if (is_end != 0u) {
            if ((allocate == 0u) ||
                (sd_extend_cluster(sd_active_file.cluster,
                                   &next_cluster) == 0u)) {
                if (allocate == 0u) {
                    sd_set_error(SD_ERROR_BAD_CLUSTER);
                }
                return 0u;
            }
        }
        sd_active_file.cluster = next_cluster;
        ++sd_active_file.cluster_index;
    }
    *cluster = sd_active_file.cluster;
    return 1u;
}

static uint8_t sd_validate_cluster_chain(unsigned long first)
    STC_SD_REENTRANT
{
    unsigned long current = first;
    unsigned long next;
    unsigned long hops = 0UL;
    uint8_t is_end;

    if (first == 0UL) {
        return 1u;
    }
    while (hops < sd_state.cluster_count) {
        if (sd_next_cluster(current, &next, &is_end) == 0u) {
            return 0u;
        }
        if (is_end != 0u) {
            return 1u;
        }
        current = next;
        ++hops;
    }
    sd_set_error(SD_ERROR_BAD_CLUSTER);
    return 0u;
}

static uint8_t sd_prepare_file_sector(void) STC_SD_REENTRANT
{
    unsigned long target_cluster_index;
    unsigned long cluster;
    unsigned long sector;

#if defined(STC_SD_HOST_TEST) && STC_SD_HOST_TEST
    STC_SD_testPrepareRead();
#endif

    if ((sd_active_file.open == 0u) ||
        (sd_active_file.position >= sd_active_file.size)) {
        return 0u;
    }

    target_cluster_index = sd_active_file.position >> (sd_state.cluster_shift + 9u);
    if (target_cluster_index >= sd_state.cluster_count) {
        sd_set_error(SD_ERROR_BAD_CLUSTER);
        return 0u;
    }

    if ((sd_get_file_cluster(target_cluster_index, 0u, &cluster) == 0u) ||
        (sd_cluster_to_sector(cluster, &sector) == 0u)) {
        return 0u;
    }
    sector += (sd_active_file.position >> 9) & (unsigned long)(sd_state.sectors_per_cluster - 1u);
    return sd_load_sector(sector);
}

static uint8_t sd_prepare_write_sector(void) STC_SD_REENTRANT
{
    unsigned long target_cluster_index;
    unsigned long cluster;
    unsigned long sector;

    if ((sd_active_file.open == 0u) ||
        (sd_active_file.writable == 0u)) {
        sd_set_error((sd_active_file.open == 0u) ?
                     SD_ERROR_NOT_INITIALIZED : SD_ERROR_READ_ONLY);
        return 0u;
    }
    target_cluster_index = sd_active_file.position >> (sd_state.cluster_shift + 9u);
    if ((sd_get_file_cluster(target_cluster_index, 1u, &cluster) == 0u) ||
        (sd_cluster_to_sector(cluster, &sector) == 0u)) {
        return 0u;
    }
    sector += (sd_active_file.position >> 9) & (unsigned long)(sd_state.sectors_per_cluster - 1u);
    return sd_load_sector(sector);
}

uint8_t SD_setPins(uint8_t mosi_pin, uint8_t miso_pin, uint8_t sck_pin,
                   uint8_t cs_pin) STC_SD_REENTRANT
{
    uint8_t validation = sd_validate_pins(mosi_pin, miso_pin, sck_pin, cs_pin);

    if (validation != SD_ERROR_NONE) {
        sd_set_error(validation);
        return 0u;
    }
    if ((sd_state.spi_active != 0u) &&
        (sd_prepare_reconfiguration() == 0u)) {
        return 0u;
    }
    if (sd_state.spi_active != 0u) {
        SD_end();
    }
    sd_state.mosi_pin = mosi_pin;
    sd_state.miso_pin = miso_pin;
    sd_state.sck_pin = sck_pin;
    sd_state.cs_pin = cs_pin;
    sd_state.pins_loaded = 1u;
    sd_set_error(SD_ERROR_NONE);
    return 1u;
}

uint8_t SD_beginClock(unsigned long clock_hz, uint8_t cs_pin) STC_SD_REENTRANT
{
    uint8_t validation;
    uint8_t saved_error;

    if (!clock_hz) { sd_set_error(SD_ERROR_INVALID_ARGUMENT); return 0u; }
    sd_load_default_pins();
    validation = sd_validate_pins(sd_state.mosi_pin, sd_state.miso_pin,
                                  sd_state.sck_pin, cs_pin);
    if (validation != SD_ERROR_NONE) {
        sd_set_error(validation);
        return 0u;
    }
    if ((sd_state.spi_active != 0u) &&
        (sd_prepare_reconfiguration() == 0u)) {
        return 0u;
    }
    if (sd_state.spi_active != 0u) {
        SD_end();
    }

    sd_state.cs_pin = cs_pin;
    sd_state.card_ready = 0u;
    sd_state.card_type = SD_CARD_NONE;
    sd_clear_volume();

    if (SPI_setPinsChecked(sd_state.mosi_pin, sd_state.miso_pin,
                sd_state.sck_pin, sd_state.cs_pin) != STC_SPI_OK) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT); return 0u;
    }
    sd_data_clock_hz = clock_hz;
    digitalWrite(sd_state.cs_pin, HIGH);
    SPI_begin();
    if (SPI_beginTransactionChecked(SD_INIT_CLOCK_HZ, MSBFIRST, SPI_MODE0) != STC_SPI_OK) {
        SPI_end(); sd_set_error(SD_ERROR_INVALID_ARGUMENT); return 0u;
    }
    sd_transaction_active = 1u;
    sd_state.spi_active = 1u;

    if (sd_initialize_card() == 0u) {
        saved_error = sd_state.last_error;
        sd_deselect();
        SPI_end();
        sd_state.spi_active = 0u;
        sd_state.card_ready = 0u;
        sd_state.card_type = SD_CARD_NONE;
        sd_clear_volume();
        sd_set_error(saved_error);
        return 0u;
    }

    if (sd_mount() == 0u) {
        /* The card remains available to readBlock/writeBlock so a caller can
         * inspect a non-FAT or damaged layout after begin() reports failure. */
        return 0u;
    }
    sd_set_error(SD_ERROR_NONE);
    return 1u;
}

uint8_t SD_begin(uint8_t cs_pin) STC_SD_REENTRANT
{
    return SD_beginClock(SD_DATA_CLOCK_HZ, cs_pin);
}

uint8_t SD_beginDefault(void) STC_SD_REENTRANT
{
    sd_load_default_pins();
    return SD_begin(sd_state.cs_pin);
}

void SD_end(void) STC_SD_REENTRANT
{
    uint8_t saved_error = SD_ERROR_NONE;

    if (sd_active_file.open != 0u) {
        SD_close();
        saved_error = sd_state.last_error;
    } else if (sd_flush_cache() == 0u) {
        saved_error = sd_state.last_error;
    }
#if !defined(STC_SD_HOST_TEST) || !STC_SD_HOST_TEST
    if (sd_state.spi_active != 0u) {
        sd_deselect();
        SPI_end();
    }
#endif
    sd_state.spi_active = 0u;
    sd_state.card_ready = 0u;
    sd_state.card_type = SD_CARD_NONE;
    sd_clear_volume();
    sd_set_error(saved_error);
}

#if defined(STC_SD_HOST_TEST) && STC_SD_HOST_TEST
uint8_t SD_testMount(void) STC_SD_REENTRANT
{
    sd_clear_volume();
    sd_state.spi_active = 1u;
    sd_state.card_ready = 1u;
    sd_state.card_type = SD_CARD_SDHC;
    return sd_mount();
}

void SD_testReset(void) STC_SD_REENTRANT
{
    unsigned int index;
    uint8_t *state = (uint8_t *)&sd_state;

    for (index = 0u; index < (unsigned int)sizeof(sd_state); ++index) {
        state[index] = 0u;
    }
    for (index = 0u; index < SD_SECTOR_SIZE; ++index) {
        sd_sector[index] = 0u;
    }
}
#endif

uint8_t SD_cardType(void) STC_SD_REENTRANT
{
    return sd_state.card_type;
}

uint8_t SD_fatType(void) STC_SD_REENTRANT
{
    return sd_state.fat_type;
}

uint8_t SD_error(void) STC_SD_REENTRANT
{
    return sd_state.last_error;
}

uint8_t SD_readBlock(unsigned long sector, uint8_t *buffer)
                     STC_SD_REENTRANT
{
    uint8_t result;

    if (sd_flush_cache() == 0u) {
        return 0u;
    }
    result = sd_read_block_internal(sector, buffer);
    sd_state.cache_valid = 0u;
    sd_state.cache_dirty = 0u;
    return result;
}

uint8_t SD_writeBlock(unsigned long sector, const uint8_t *buffer)
                      STC_SD_REENTRANT
{
    uint8_t result;

    if (sd_flush_cache() == 0u) {
        return 0u;
    }
    result = sd_write_block_internal(sector, buffer);
    sd_state.cache_valid = 0u;
    sd_state.cache_dirty = 0u;
    return result;
}

uint8_t SD_exists(const char *name) STC_SD_REENTRANT
{
    uint8_t target[11]; unsigned long parent; STCSDDirEntry entry;
    if (!sd_state.mounted) { sd_set_error(SD_ERROR_NOT_INITIALIZED); return 0u; }
    if (sd_is_root(name)) { sd_set_error(SD_ERROR_NONE); return 1u; }
    if (!sd_parent(name, &parent, target, 0u) || !sd_find_entry(parent, target, &entry)) return 0u;
    sd_set_error(SD_ERROR_NONE); return 1u;
}
uint8_t SD_open(const char *name, uint8_t mode) STC_SD_REENTRANT
{
    uint8_t target[11] = {0}; unsigned long parent; STCSDDirEntry entry;
    if (!sd_state.mounted) { sd_set_error(SD_ERROR_NOT_INITIALIZED); return 0u; }
    if (mode != FILE_READ && (!(mode & SD_MODE_WRITE) || (mode & 0xe8u))) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT); return 0u;
    }
    if (sd_active_file.open) { SD_close(); if (sd_state.last_error) return 0u; }
    if (sd_is_root(name)) {
        entry.sector = 0UL; entry.offset = 0u; entry.cluster = 0UL; entry.size = 0UL;
        entry.attribute = SD_ATTRIBUTE_DIRECTORY; target[0] = '/';
        return sd_open_entry(&entry, target, mode);
    }
    if (!sd_parent(name, &parent, target, 0u)) return 0u;
    if (!sd_find_entry(parent, target, &entry)) {
        if (!(mode & SD_MODE_CREATE) || sd_state.last_error != SD_ERROR_NOT_FOUND ||
            !sd_create_entry(parent, target, SD_ATTRIBUTE_ARCHIVE, 0UL, &entry)) return 0u;
    }
    return sd_open_entry(&entry, target, mode);
}

size_t SD_write(uint8_t value) STC_SD_REENTRANT
{
    return SD_writeBytes(&value, 1u);
}

size_t SD_writeBytes(const uint8_t *buffer, size_t length)
                     STC_SD_REENTRANT
{
    size_t count = 0u;
    size_t chunk;
    unsigned int offset;
    /* copy uses the shared libc routine */

    if (sd_active_file.open == 0u) {
        sd_set_error(SD_ERROR_NOT_INITIALIZED);
        return 0u;
    }
    if (sd_active_file.writable == 0u) {
        sd_set_error(SD_ERROR_READ_ONLY);
        return 0u;
    }
    if ((buffer == NULL) && (length != 0u)) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        return 0u;
    }
    while (count < length) {
        if (sd_active_file.position >= SD_UINT32_MAX) {
            sd_set_error(SD_ERROR_NO_SPACE);
            break;
        }
        if (sd_prepare_write_sector() == 0u) {
            break;
        }
        offset = (unsigned int)(sd_active_file.position & 0x01ffUL);
        chunk = (size_t)(SD_SECTOR_SIZE - offset);
        if (chunk > (length - count)) {
            chunk = length - count;
        }
        if ((unsigned long)chunk >
            (SD_UINT32_MAX - sd_active_file.position)) {
            chunk = (size_t)(SD_UINT32_MAX - sd_active_file.position);
        }
        if (chunk == 0u) {
            sd_set_error(SD_ERROR_NO_SPACE);
            break;
        }
        memcpy(sd_sector + offset, buffer + count, chunk);
        sd_state.cache_dirty = 1u;
        sd_active_file.position += (unsigned long)chunk;
        count += chunk;
        if (sd_active_file.position > sd_active_file.size) {
            sd_active_file.size = sd_active_file.position;
            sd_active_file.metadata_dirty = 1u;
        }
    }
    if (count == length) {
        sd_set_error(SD_ERROR_NONE);
    }
    return count;
}

int SD_peek(void) STC_SD_REENTRANT
{
    if (sd_active_file.open == 0u) {
        sd_set_error(SD_ERROR_NOT_INITIALIZED);
        return -1;
    }
    if (sd_active_file.position >= sd_active_file.size) {
        sd_set_error(SD_ERROR_NONE);
        return -1;
    }
    if (sd_prepare_file_sector() == 0u) {
        return -1;
    }
    sd_set_error(SD_ERROR_NONE);
    return (int)sd_sector[(unsigned int)
        (sd_active_file.position & 0x01ffUL)];
}

int SD_read(void) STC_SD_REENTRANT
{
    int value = SD_peek();

    if (value >= 0) {
        ++sd_active_file.position;
    }
    return value;
}

size_t SD_readBytes(uint8_t *buffer, size_t length) STC_SD_REENTRANT
{
    size_t count = 0u;
    unsigned int offset, chunk;
    unsigned long remaining;

    if (sd_active_file.open == 0u) {
        sd_set_error(SD_ERROR_NOT_INITIALIZED);
        return 0u;
    }
    if ((buffer == NULL) && (length != 0u)) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        return 0u;
    }
    if (length == 0u) {
        sd_set_error(SD_ERROR_NONE);
        return 0u;
    }
    /* Bound the request once; every chunk below then fits both the caller's
     * buffer and the file. This avoids repeated 32-bit EOF arithmetic. */
    remaining = sd_active_file.position < sd_active_file.size ?
        sd_active_file.size - sd_active_file.position : 0UL;
    if (length > remaining) {
        length = (size_t)remaining;
    }
    while (count < length) {
        if (!sd_prepare_file_sector()) {
            return count;
        }
        offset = (unsigned int)(sd_active_file.position & 0x01ffUL);
        chunk = SD_SECTOR_SIZE - offset;
        if ((size_t)chunk > length - count) {
            chunk = (unsigned int)(length - count);
        }
        /* Use the shared cache, including any bytes not flushed yet. */
        memcpy(buffer + count, sd_sector + offset, chunk);
        count += chunk;
        sd_active_file.position += (unsigned long)chunk;
    }
    sd_set_error(SD_ERROR_NONE);
    return count;
}

unsigned long SD_available(void) STC_SD_REENTRANT
{
    if (sd_active_file.open == 0u) {
        sd_set_error(SD_ERROR_NOT_INITIALIZED);
        return 0UL;
    }
    sd_set_error(SD_ERROR_NONE);
    return sd_active_file.is_directory ? 0UL : sd_active_file.size - sd_active_file.position;
}

uint8_t SD_seek(unsigned long position) STC_SD_REENTRANT
{
    if (sd_active_file.open == 0u) {
        sd_set_error(SD_ERROR_NOT_INITIALIZED);
        return 0u;
    }
    if (sd_active_file.is_directory || position > sd_active_file.size) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT);
        return 0u;
    }
    sd_active_file.position = position;
    sd_active_file.cluster = sd_active_file.first_cluster;
    sd_active_file.cluster_index = 0UL;
    sd_set_error(SD_ERROR_NONE);
    return 1u;
}

unsigned long SD_position(void) STC_SD_REENTRANT
{
    return (sd_active_file.open != 0u) ? sd_active_file.position : 0UL;
}

unsigned long SD_size(void) STC_SD_REENTRANT
{
    return (sd_active_file.open != 0u) ? sd_active_file.size : 0UL;
}

uint8_t SD_flush(void) STC_SD_REENTRANT
{
    if (sd_active_file.open == 0u) {
        sd_set_error(SD_ERROR_NOT_INITIALIZED);
        return 0u;
    }
    if ((sd_flush_cache() == 0u) ||
        (sd_update_open_file_metadata() == 0u)) {
        return 0u;
    }
    sd_set_error(SD_ERROR_NONE);
    return 1u;
}

void SD_close(void) STC_SD_REENTRANT
{
    if ((sd_active_file.open != 0u) && (SD_flush() == 0u)) {
        /* Keep the backend state live so an explicit second close/flush, or
         * the next open/remove operation, can retry without orphaning a newly
         * allocated chain whose directory metadata is still pending. */
        return;
    }
    sd_clear_file();
    sd_set_error(SD_ERROR_NONE);
}

static uint8_t sd_delete(const char *name, uint8_t directory) STC_SD_REENTRANT
{
    uint8_t target[11], attr; unsigned long parent, index = 0UL, sector;
    unsigned int offset; STCSDDirEntry entry;
    if (!sd_state.mounted) { sd_set_error(SD_ERROR_NOT_INITIALIZED); return 0u; }
    if (!sd_parent(name, &parent, target, 0u) || !sd_find_entry(parent, target, &entry)) return 0u;
    if (target[0] == '.' || target[0] == '/') { sd_set_error(SD_ERROR_INVALID_ARGUMENT); return 0u; }
    if (!!(entry.attribute & SD_ATTRIBUTE_DIRECTORY) != !!directory) {
        sd_set_error(directory ? SD_ERROR_NOT_A_DIRECTORY : SD_ERROR_NOT_A_FILE); return 0u;
    }
    if (entry.attribute & SD_ATTRIBUTE_READ_ONLY) { sd_set_error(SD_ERROR_READ_ONLY); return 0u; }
    if (sd_active_file.open && entry.sector == sd_active_file.dir_sector && entry.offset == sd_active_file.dir_offset) {
        sd_set_error(SD_ERROR_FILE_BUSY); return 0u;
    }
    if (entry.cluster && !sd_validate_cluster_chain(entry.cluster)) return 0u;
    if (directory) for (;;) {
        if (!sd_directory_location(entry.cluster, index++, 0u, &sector, &offset)) {
            if (sd_state.last_error == SD_ERROR_NOT_FOUND) break;
            return 0u;
        }
        if (!sd_load_sector(sector)) return 0u;
        if (!sd_sector[offset]) break;
        attr = sd_sector[offset + 11u];
        if (sd_sector[offset] != 0xe5u && sd_sector[offset] != '.' && !(attr & SD_ATTRIBUTE_VOLUME_ID)) {
            sd_set_error(SD_ERROR_DIRECTORY_NOT_EMPTY); return 0u;
        }
    }
    if (!sd_load_sector(entry.sector)) return 0u;
    sd_sector[entry.offset] = 0xe5u;
    if (!sd_store_loaded_sector()) return 0u;
    if (entry.cluster && !sd_release_cluster_chain(entry.cluster)) return 0u;
    sd_set_error(SD_ERROR_NONE); return 1u;
}
uint8_t SD_remove(const char *name) STC_SD_REENTRANT { return sd_delete(name, 0u); }
uint8_t SD_rmdir(const char *name) STC_SD_REENTRANT { return sd_delete(name, 1u); }
uint8_t SD_mkdir(const char *name) STC_SD_REENTRANT
{
    uint8_t target[11]; unsigned long parent; STCSDDirEntry entry;
    if (!sd_state.mounted) { sd_set_error(SD_ERROR_NOT_INITIALIZED); return 0u; }
    if (sd_is_root(name)) { sd_set_error(SD_ERROR_NONE); return 1u; }
    if (!sd_parent(name, &parent, target, 1u)) return 0u;
    if (sd_find_entry(parent, target, &entry)) {
        if (!(entry.attribute & SD_ATTRIBUTE_DIRECTORY)) {
            sd_set_error(SD_ERROR_NOT_A_DIRECTORY); return 0u;
        }
    } else if (sd_state.last_error != SD_ERROR_NOT_FOUND || !sd_make_directory(parent, target, &entry)) return 0u;
    sd_set_error(SD_ERROR_NONE); return 1u;
}
uint8_t SD_openNext(STCSDFileContext *file, uint8_t mode) STC_SD_REENTRANT
{
    uint8_t target[11], i, first, attr; unsigned long sector; unsigned int offset;
    STCSDDirEntry entry; STCSDFileContext *directory = sd_file;
    if (!sd_active_file.open || !sd_active_file.is_directory) {
        sd_set_error(SD_ERROR_NOT_A_DIRECTORY); return 0u;
    }
    if (!file || file == directory || file->open ||
        (mode != FILE_READ && (!(mode & SD_MODE_WRITE) || (mode & 0xe8u)))) {
        sd_set_error(SD_ERROR_INVALID_ARGUMENT); return 0u;
    }
    for (;;) {
        if (!sd_directory_location(sd_active_file.first_cluster, sd_active_file.position, 2u, &sector, &offset)) {
            if (sd_state.last_error == SD_ERROR_NOT_FOUND) sd_set_error(SD_ERROR_NONE);
            return 0u;
        }
        if (!sd_load_sector(sector)) return 0u;
        first = sd_sector[offset]; attr = sd_sector[offset + 11u];
        if (!first) { sd_set_error(SD_ERROR_NONE); return 0u; }
        ++sd_active_file.position;
        if (first == 0xe5u || first == '.' || (attr & SD_ATTRIBUTE_VOLUME_ID)) continue;
        sd_decode_entry(sector, offset, &entry);
        for (i = 0u; i < 11u; ++i) target[i] = sd_sector[offset + i];
        if (!SD_selectFile(file)) return 0u;
        if (!sd_open_entry(&entry, target, mode)) { sd_file = directory; sd_active_file = *directory; return 0u; }
        return 1u;
    }
}
void SD_rewindDirectory(void) STC_SD_REENTRANT
{
    if (sd_active_file.open && sd_active_file.is_directory) {
        sd_active_file.position = 0UL;
        sd_active_file.cluster = sd_active_file.first_cluster;
        sd_active_file.cluster_index = 0UL;
    }
}
void SD_fileName(char *name) STC_SD_REENTRANT
{
    uint8_t i, length = 0u;
    if (!name) return;
    if (sd_active_file.name[0] == '/') { name[0] = '/'; name[1] = 0; return; }
    for (i = 0u; i < 8u && sd_active_file.name[i] != ' '; ++i) name[length++] = (char)sd_active_file.name[i];
    if (sd_active_file.name[8] != ' ') {
        name[length++] = '.';
        for (i = 8u; i < 11u && sd_active_file.name[i] != ' '; ++i) name[length++] = (char)sd_active_file.name[i];
    }
    name[length] = 0;
}
