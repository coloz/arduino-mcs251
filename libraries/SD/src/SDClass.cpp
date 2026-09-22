#include "SDClass.h"
#include <limits.h>
#include <stcxx_allocator.h>

struct STCSDFileState {
    STCSDFileContext file;
    uint32_t references;
    unsigned long generation;
    char name[13];
    STCSDFileState *next;
};
namespace {
STCSDFileState *handles;
bool live(const STCSDFileState *state)
{
    return state && state->generation == SD_generation() && state->file.open;
}
void discard(STCSDFileState *state)
{
    SD_forgetFile(&state->file);
    STCSDFileState **link = &handles;
    while (*link && *link != state) link = &(*link)->next;
    if (*link) *link = state->next;
    stcxx_free(state);
}
STCSDFileState *allocate()
{
    // Retry writeback for handles abandoned by destructors after an I/O error.
    STCSDFileState *state = handles;
    while (state) {
        STCSDFileState *next = state->next;
        if (!state->references) {
            if (live(state)) {
                if (!SD_selectFile(&state->file)) return 0;
                SD_close();
                if (SD_error()) return 0;
            }
            discard(state);
        }
        state = next;
    }
    state = static_cast<STCSDFileState *>(stcxx_malloc(sizeof(STCSDFileState)));
    if (!state) { SD_setError(SD_ERROR_NO_SPACE); return 0; }
    memset(state, 0, sizeof(*state));
    state->generation = SD_generation();
    state->references = 1;
    return state;
}
bool sameEntry(const STCSDFileContext &a, const STCSDFileContext &b)
{
    if (a.is_directory && b.is_directory) return a.first_cluster == b.first_cluster;
    return a.dir_sector == b.dir_sector && a.dir_offset == b.dir_offset;
}
bool attach(STCSDFileState *state)
{
    for (STCSDFileState *other = handles; other; other = other->next) {
        if (live(other) && sameEntry(state->file, other->file) &&
            (state->file.writable || other->file.writable)) {
            // Independent positions are supported; competing mutable views of
            // one directory entry are explicitly rejected to avoid stale size.
            SD_close(); SD_forgetFile(&state->file);
            SD_setError(SD_ERROR_FILE_BUSY); return false;
        }
    }
    SD_fileName(state->name);
    state->next = handles; handles = state;
    return true;
}
bool canDelete(const char *name)
{
    if (!name) { SD_setError(SD_ERROR_INVALID_ARGUMENT); return false; }
    STCSDFileContext probe;
    memset(&probe, 0, sizeof(probe));
    if (!SD_selectFile(&probe)) return false;
    bool result = SD_open(name, FILE_READ) != 0;
    uint8_t error = SD_error();
    if (result) {
        for (STCSDFileState *state = handles; state; state = state->next) {
            if (live(state) && sameEntry(probe, state->file)) {
                result = false; error = SD_ERROR_FILE_BUSY; break;
            }
        }
        SD_close();
    }
    SD_forgetFile(&probe);
    if (!result) SD_setError(error);
    return result;
}
}

SDClass SD;
File::File() : _state(0) {}
File::File(STCSDFileState *state) : _state(state) {}
File::File(const File &other) : _state(0) { retain(other); }
File::File(File &&other) : _state(other._state) { other._state = 0; }
File::~File() { release(); }
File &File::operator=(const File &other)
{
    if (this != &other) { release(); retain(other); }
    return *this;
}
File &File::operator=(File &&other)
{
    if (this != &other) { release(); _state = other._state; other._state = 0; }
    return *this;
}
bool File::valid() const { return live(_state); }
bool File::select() const { return valid() && SD_selectFile(&_state->file); }
void File::retain(const File &other)
{
    if (other.valid() && other._state->references != UINT32_MAX) {
        _state = other._state; ++_state->references;
    }
}
void File::release()
{
    if (!_state) return;
    STCSDFileState *state = _state;
    _state = 0;
    if (--state->references) return;
    if (live(state)) {
        if (!SD_selectFile(&state->file)) return;
        SD_close();
        if (SD_error()) return; // Keep a retryable orphan, never a dangling backend pointer.
    }
    discard(state);
}
void File::invalidateHandles()
{
    for (STCSDFileState *state = handles; state; state = state->next) {
        SD_forgetFile(&state->file);
        state->file.open = 0u;
    }
}
bool File::closeAll()
{
    for (STCSDFileState *state = handles; state; state = state->next) {
        if (live(state) && (!SD_selectFile(&state->file) || !SD_flush())) return false;
    }
    invalidateHandles();
    STCSDFileState *state = handles;
    while (state) {
        STCSDFileState *next = state->next;
        if (!state->references) discard(state);
        state = next;
    }
    return true;
}
size_t File::write(uint8_t value) { return write(&value, 1u); }
size_t File::write(const uint8_t *buffer, size_t length)
{
    if (!valid()) { setWriteError(SD_ERROR_NOT_INITIALIZED); return 0; }
    if (!select()) { setWriteError(SD_error()); return 0; }
    if (!_state->file.writable) { setWriteError(SD_ERROR_READ_ONLY); return 0; }
    if (!buffer && length) { setWriteError(SD_ERROR_INVALID_ARGUMENT); return 0; }
    size_t count = SD_writeBytes(buffer, length);
    if (count != length) setWriteError(SD_error());
    return count;
}
int File::availableForWrite() { return valid() && _state->file.writable ? INT_MAX : 0; }
int File::available()
{
    if (!select()) return 0;
    unsigned long count = SD_available();
    return count > (unsigned long)INT_MAX ? INT_MAX : (int)count;
}
int File::read() { return select() ? SD_read() : -1; }
int File::read(void *buffer, uint16_t length)
{
    if (!select() || (!buffer && length)) return 0;
    size_t count = length;
    if (count > (size_t)INT_MAX) count = INT_MAX;
    return (int)SD_readBytes(static_cast<uint8_t *>(buffer), count);
}
int File::peek() { return select() ? SD_peek() : -1; }
void File::flush() { if (valid() && (!select() || !SD_flush())) setWriteError(SD_error()); }
bool File::seek(uint32_t position) { return select() && SD_seek(position); }
uint32_t File::position() const { return select() ? (uint32_t)SD_position() : 0u; }
uint32_t File::size() const { return select() ? (uint32_t)SD_size() : 0u; }
void File::close()
{
    if (valid() && _state->references == 1) {
        if (!select()) { setWriteError(SD_error()); return; }
        SD_close();
        if (SD_error()) { setWriteError(SD_error()); return; }
    }
    release();
}
File::operator bool() const { return valid(); }
const char *File::name() const { return valid() ? _state->name : ""; }
bool File::isDirectory() const { return valid() && _state->file.is_directory; }
void File::rewindDirectory() { if (select()) SD_rewindDirectory(); }
File File::openNextFile(uint8_t mode)
{
    if (!isDirectory()) return File();
    STCSDFileState *state = allocate();
    if (!state) return File();
    if (!select() || !SD_openNext(&state->file, mode) || !attach(state)) {
        SD_forgetFile(&state->file); stcxx_free(state); return File();
    }
    return File(state);
}
bool SDClass::begin() { return File::closeAll() && SD_beginDefault(); }
bool SDClass::begin(uint8_t select) { return File::closeAll() && SD_begin(select); }
bool SDClass::begin(uint32_t clock, uint8_t select) { return File::closeAll() && SD_beginClock(clock, select); }
void SDClass::end() { if (File::closeAll()) SD_end(); }
bool SDClass::setPins(uint8_t mosi, uint8_t miso, uint8_t clock, uint8_t select)
{
    // Let the backend validate pins before changing the volume generation.
    for (STCSDFileState *state = handles; state; state = state->next)
        if (live(state) && (!SD_selectFile(&state->file) || !SD_flush())) return false;
    if (!SD_setPins(mosi, miso, clock, select)) return false;
    File::invalidateHandles(); return true;
}
File SDClass::open(const char *name, uint8_t mode)
{
    STCSDFileState *state = allocate();
    if (!state) return File();
    if (!SD_selectFile(&state->file) || !SD_open(name, mode) || !attach(state)) {
        SD_forgetFile(&state->file); stcxx_free(state); return File();
    }
    return File(state);
}
bool SDClass::exists(const char *name) { return name && SD_exists(name); }
bool SDClass::remove(const char *name) { return canDelete(name) && SD_remove(name); }
bool SDClass::mkdir(const char *name) { return name && SD_mkdir(name); }
bool SDClass::rmdir(const char *name) { return canDelete(name) && SD_rmdir(name); }
