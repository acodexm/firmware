/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "InternalFileSystem.h"

#include <cerrno>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>

using namespace Adafruit_LittleFS_Namespace;

extern "C" __attribute__((weak)) bool runaAllowFilesystemGrowth(const char *, size_t, size_t)
{
    return true;
}

namespace
{

constexpr char mountPoint[] = "/lfs";
constexpr size_t pathLength = 256;

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(meshtastic_lfs_data);

fs_mount_t mount = {
    .type = FS_LITTLEFS,
    .mnt_point = mountPoint,
    .fs_data = &meshtastic_lfs_data,
    .storage_dev = reinterpret_cast<void *>(FIXED_PARTITION_ID(storage_partition)),
    .flags = 0,
};

void relativePath(const char *absolute, char *relative, size_t size)
{
    const size_t prefixLength = sizeof(mountPoint) - 1U;
    const char *source = strncmp(absolute, mountPoint, prefixLength) == 0 ? absolute + prefixLength : absolute;
    if (*source == '\0')
        source = "/";
    snprintf(relative, size, "%s", source);
}

void reportFsFailure(const char *operation, int error)
{
    printk("[zephyr-fs] %s failed: %d\n", operation, error);
}

bool joinPath(char *destination, size_t size, const char *parent, const char *name)
{
    const int length = snprintf(destination, size, "%s/%s", parent, name);
    return length >= 0 && static_cast<size_t>(length) < size;
}

bool storagePartitionIsErased()
{
    const flash_area *area = nullptr;
    if (flash_area_open(FIXED_PARTITION_ID(storage_partition), &area) != 0)
        return false;

    constexpr size_t readSize = 128;
    uint8_t bytes[readSize];
    const uint8_t erasedValue = flash_area_erased_val(area);
    bool erased = true;
    for (size_t offset = 0; offset < area->fa_size; offset += readSize) {
        const size_t remaining = area->fa_size - offset;
        const size_t length = remaining < readSize ? remaining : readSize;
        if (flash_area_read(area, static_cast<off_t>(offset), bytes, length) != 0) {
            erased = false;
            break;
        }
        for (size_t index = 0; index < length; ++index) {
            if (bytes[index] != erasedValue) {
                erased = false;
                break;
            }
        }
        if (!erased)
            break;
    }
    flash_area_close(area);
    return erased;
}

} // namespace

Adafruit_LittleFS_Namespace::InternalFileSystem InternalFS;

void InternalFileSystem::toabs(const char *relative, char *absolute, size_t size)
{
    if (relative == nullptr || absolute == nullptr || size == 0)
        return;
    if (strcmp(relative, "/") == 0)
        snprintf(absolute, size, "%s", mountPoint);
    else if (relative[0] == '/')
        snprintf(absolute, size, "%s%s", mountPoint, relative);
    else
        snprintf(absolute, size, "%s/%s", mountPoint, relative);
}

bool InternalFileSystem::begin()
{
    if (_mounted)
        return true;

    int error = fs_mount(&mount);
    if (error == 0) {
        _mounted = true;
        return true;
    }

    if (!storagePartitionIsErased()) {
        printk("[zephyr-fs] mount failed (%d); preserving nonblank storage partition\n", error);
        return false;
    }

    printk("[zephyr-fs] mount failed (%d); formatting erased first-boot storage partition\n", error);
    error = fs_mkfs(FS_LITTLEFS, FIXED_PARTITION_ID(storage_partition), nullptr, 0);
    if (error != 0) {
        reportFsFailure("format", error);
        return false;
    }
    error = fs_mount(&mount);
    if (error != 0) {
        reportFsFailure("mount after format", error);
        return false;
    }
    _mounted = true;
    return true;
}

File InternalFileSystem::open(const char *path, const char *mode)
{
    if (!_mounted || path == nullptr || mode == nullptr)
        return {};

    char absolute[pathLength];
    toabs(path, absolute, sizeof(absolute));
    auto state = std::make_shared<ZephyrFileState>();
    if (!state)
        return {};
    snprintf(state->fullpath, sizeof(state->fullpath), "%s", absolute);
    relativePath(absolute, state->relpath, sizeof(state->relpath));

    fs_dirent entry{};
    const int statError = fs_stat(absolute, &entry);
    if (statError == 0 && entry.type == FS_DIR_ENTRY_DIR) {
        state->is_dir = true;
        if (fs_opendir(&state->dir, absolute) != 0)
            return {};
        state->valid = true;
        return File(state);
    }

    fs_mode_t flags = FS_O_READ;
    if (strcmp(mode, FILE_O_WRITE) == 0) {
        flags = FS_O_WRITE | FS_O_CREATE | FS_O_TRUNC;
    }
    const int error = fs_open(&state->file, absolute, flags);
    if (error != 0)
        return {};
    state->is_dir = false;
    state->file_size = strcmp(mode, FILE_O_WRITE) == 0 || statError != 0 ? 0 : static_cast<size_t>(entry.size);
    state->valid = true;
    return File(state);
}

bool InternalFileSystem::exists(const char *path)
{
    if (!_mounted || path == nullptr)
        return false;
    char absolute[pathLength];
    toabs(path, absolute, sizeof(absolute));
    fs_dirent entry{};
    return fs_stat(absolute, &entry) == 0;
}

bool InternalFileSystem::remove(const char *path)
{
    if (!_mounted || path == nullptr)
        return false;
    char absolute[pathLength];
    toabs(path, absolute, sizeof(absolute));
    return fs_unlink(absolute) == 0;
}

bool InternalFileSystem::rename(const char *from, const char *to)
{
    if (!_mounted || from == nullptr || to == nullptr)
        return false;
    char absoluteFrom[pathLength];
    char absoluteTo[pathLength];
    toabs(from, absoluteFrom, sizeof(absoluteFrom));
    toabs(to, absoluteTo, sizeof(absoluteTo));
    return fs_rename(absoluteFrom, absoluteTo) == 0;
}

bool InternalFileSystem::mkdir(const char *path)
{
    if (!_mounted || path == nullptr)
        return false;
    char absolute[pathLength];
    toabs(path, absolute, sizeof(absolute));
    const int error = fs_mkdir(absolute);
    return error == 0 || error == -EEXIST;
}

bool InternalFileSystem::rmdir(const char *path)
{
    return remove(path);
}

bool InternalFileSystem::rmdir_r(const char *path)
{
    if (!_mounted || path == nullptr)
        return false;
    char absolute[pathLength];
    toabs(path, absolute, sizeof(absolute));

    fs_dir_t directory;
    fs_dir_t_init(&directory);
    if (fs_opendir(&directory, absolute) != 0)
        return fs_unlink(absolute) == 0;

    bool success = true;
    fs_dirent entry{};
    while (fs_readdir(&directory, &entry) == 0 && entry.name[0] != '\0') {
        char child[pathLength];
        if (!joinPath(child, sizeof(child), absolute, entry.name)) {
            success = false;
            continue;
        }
        if (entry.type == FS_DIR_ENTRY_DIR) {
            char relative[pathLength];
            relativePath(child, relative, sizeof(relative));
            success = rmdir_r(relative) && success;
        } else {
            success = (fs_unlink(child) == 0) && success;
        }
    }
    success = (fs_closedir(&directory) == 0) && success;
    return (fs_unlink(absolute) == 0) && success;
}

bool InternalFileSystem::format()
{
    if (_mounted) {
        const int error = fs_unmount(&mount);
        if (error != 0) {
            reportFsFailure("unmount", error);
            return false;
        }
        _mounted = false;
    }
    const int error = fs_mkfs(FS_LITTLEFS, FIXED_PARTITION_ID(storage_partition), nullptr, 0);
    if (error != 0) {
        reportFsFailure("format", error);
        return false;
    }
    return begin();
}

File File::openNextFile()
{
    if (!_s || !_s->valid || !_s->is_dir)
        return {};
    fs_dirent entry{};
    if (fs_readdir(&_s->dir, &entry) != 0 || entry.name[0] == '\0')
        return {};

    char absolute[pathLength];
    if (!joinPath(absolute, sizeof(absolute), _s->fullpath, entry.name))
        return {};
    auto state = std::make_shared<ZephyrFileState>();
    if (!state)
        return {};
    snprintf(state->fullpath, sizeof(state->fullpath), "%s", absolute);
    relativePath(absolute, state->relpath, sizeof(state->relpath));

    state->is_dir = entry.type == FS_DIR_ENTRY_DIR;
    const int error = state->is_dir ? fs_opendir(&state->dir, absolute) : fs_open(&state->file, absolute, FS_O_READ);
    if (error != 0)
        return {};
    if (!state->is_dir)
        state->file_size = static_cast<size_t>(entry.size);
    state->valid = true;
    return File(state);
}
