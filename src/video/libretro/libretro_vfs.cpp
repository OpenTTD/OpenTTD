/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_vfs.cpp Libretro VFS wrapper for OpenTTD file operations. */

#ifdef WITH_LIBRETRO

#include "libretro_vfs.h"
#include "libretro_core.h"
#include "libretro.h"

#include <cstring>
#include <cstdio>
#include <string>

/* Debug logging macros */
#define LR_VFS_DEBUG(...) do { \
    LibretroCore::Log(RETRO_LOG_DEBUG, "[libretro_vfs] " __VA_ARGS__); \
} while(0)

#define LR_VFS_INFO(...) do { \
    LibretroCore::Log(RETRO_LOG_INFO, "[libretro_vfs] " __VA_ARGS__); \
} while(0)

#define LR_VFS_WARN(...) do { \
    LibretroCore::Log(RETRO_LOG_WARN, "[libretro_vfs] " __VA_ARGS__); \
} while(0)

#define LR_VFS_ERROR(...) do { \
    LibretroCore::Log(RETRO_LOG_ERROR, "[libretro_vfs] " __VA_ARGS__); \
} while(0)

namespace LibretroVFS {

/* Internal file handle wrapper */
struct FileHandle {
    struct retro_vfs_file_handle *vfs_handle;
    FILE *stdio_handle;
    bool using_vfs;
    std::string path;
};

/* Internal directory handle wrapper */
struct DirHandle {
    struct retro_vfs_dir_handle *vfs_handle;
    void *stdio_handle; /* Platform-specific DIR* or HANDLE */
    bool using_vfs;
    std::string path;
};

FileHandle *Open(const char *path, unsigned mode)
{
    LR_VFS_DEBUG("Open: path=%s, mode=%u\n", path, mode);

    if (!path) {
        LR_VFS_ERROR("Open: null path\n");
        return nullptr;
    }

    FileHandle *handle = new FileHandle();
    handle->path = path;
    handle->vfs_handle = nullptr;
    handle->stdio_handle = nullptr;
    handle->using_vfs = false;

    /* Try VFS first if available */
    struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
    if (vfs && vfs->open) {
        LR_VFS_DEBUG("Open: Attempting VFS open\n");

        unsigned vfs_mode = 0;
        unsigned vfs_hints = RETRO_VFS_FILE_ACCESS_HINT_NONE;

        if (mode & LIBRETRO_VFS_MODE_READ) {
            vfs_mode |= RETRO_VFS_FILE_ACCESS_READ;
        }
        if (mode & LIBRETRO_VFS_MODE_WRITE) {
            vfs_mode |= RETRO_VFS_FILE_ACCESS_WRITE;
        }
        if (mode & LIBRETRO_VFS_MODE_UPDATE) {
            vfs_mode |= RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
        }

        handle->vfs_handle = vfs->open(path, vfs_mode, vfs_hints);
        if (handle->vfs_handle) {
            handle->using_vfs = true;
            LR_VFS_DEBUG("Open: VFS open successful\n");
            return handle;
        }
        LR_VFS_DEBUG("Open: VFS open failed, falling back to stdio\n");
    }

    /* Fall back to stdio */
    const char *stdio_mode = "rb";
    if ((mode & LIBRETRO_VFS_MODE_READ) && (mode & LIBRETRO_VFS_MODE_WRITE)) {
        stdio_mode = (mode & LIBRETRO_VFS_MODE_UPDATE) ? "r+b" : "w+b";
    } else if (mode & LIBRETRO_VFS_MODE_WRITE) {
        stdio_mode = (mode & LIBRETRO_VFS_MODE_UPDATE) ? "r+b" : "wb";
    }

    handle->stdio_handle = fopen(path, stdio_mode);
    if (handle->stdio_handle) {
        LR_VFS_DEBUG("Open: stdio open successful\n");
        return handle;
    }

    LR_VFS_ERROR("Open: Failed to open file\n");
    delete handle;
    return nullptr;
}

int Close(FileHandle *handle)
{
    LR_VFS_DEBUG("Close: handle=%p\n", (void*)handle);

    if (!handle) {
        return -1;
    }

    int result = 0;

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->close && handle->vfs_handle) {
            result = vfs->close(handle->vfs_handle);
        }
    } else if (handle->stdio_handle) {
        result = fclose(handle->stdio_handle);
    }

    delete handle;
    LR_VFS_DEBUG("Close: result=%d\n", result);
    return result;
}

int64_t Size(FileHandle *handle)
{
    LR_VFS_DEBUG("Size: handle=%p\n", (void*)handle);

    if (!handle) {
        return -1;
    }

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->size && handle->vfs_handle) {
            return vfs->size(handle->vfs_handle);
        }
    } else if (handle->stdio_handle) {
        long current = ftell(handle->stdio_handle);
        fseek(handle->stdio_handle, 0, SEEK_END);
        long size = ftell(handle->stdio_handle);
        fseek(handle->stdio_handle, current, SEEK_SET);
        return size;
    }

    return -1;
}

int64_t Tell(FileHandle *handle)
{
    LR_VFS_DEBUG("Tell: handle=%p\n", (void*)handle);

    if (!handle) {
        return -1;
    }

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->tell && handle->vfs_handle) {
            return vfs->tell(handle->vfs_handle);
        }
    } else if (handle->stdio_handle) {
        return ftell(handle->stdio_handle);
    }

    return -1;
}

int64_t Seek(FileHandle *handle, int64_t offset, int whence)
{
    LR_VFS_DEBUG("Seek: handle=%p, offset=%lld, whence=%d\n",
        (void*)handle, (long long)offset, whence);

    if (!handle) {
        return -1;
    }

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->seek && handle->vfs_handle) {
            int vfs_whence = RETRO_VFS_SEEK_POSITION_START;
            switch (whence) {
                case SEEK_SET: vfs_whence = RETRO_VFS_SEEK_POSITION_START; break;
                case SEEK_CUR: vfs_whence = RETRO_VFS_SEEK_POSITION_CURRENT; break;
                case SEEK_END: vfs_whence = RETRO_VFS_SEEK_POSITION_END; break;
            }
            return vfs->seek(handle->vfs_handle, offset, vfs_whence);
        }
    } else if (handle->stdio_handle) {
        return fseek(handle->stdio_handle, (long)offset, whence);
    }

    return -1;
}

int64_t Read(FileHandle *handle, void *buffer, uint64_t len)
{
    LR_VFS_DEBUG("Read: handle=%p, len=%llu\n", (void*)handle, (unsigned long long)len);

    if (!handle || !buffer) {
        return -1;
    }

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->read && handle->vfs_handle) {
            return vfs->read(handle->vfs_handle, buffer, len);
        }
    } else if (handle->stdio_handle) {
        return fread(buffer, 1, len, handle->stdio_handle);
    }

    return -1;
}

int64_t Write(FileHandle *handle, const void *buffer, uint64_t len)
{
    LR_VFS_DEBUG("Write: handle=%p, len=%llu\n", (void*)handle, (unsigned long long)len);

    if (!handle || !buffer) {
        return -1;
    }

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->write && handle->vfs_handle) {
            return vfs->write(handle->vfs_handle, buffer, len);
        }
    } else if (handle->stdio_handle) {
        return fwrite(buffer, 1, len, handle->stdio_handle);
    }

    return -1;
}

int Flush(FileHandle *handle)
{
    LR_VFS_DEBUG("Flush: handle=%p\n", (void*)handle);

    if (!handle) {
        return -1;
    }

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->flush && handle->vfs_handle) {
            return vfs->flush(handle->vfs_handle);
        }
    } else if (handle->stdio_handle) {
        return fflush(handle->stdio_handle);
    }

    return -1;
}

int Stat(const char *path, int32_t *size)
{
    LR_VFS_DEBUG("Stat: path=%s\n", path);

    if (!path) {
        return 0;
    }

    struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
    if (vfs && vfs->stat) {
        return vfs->stat(path, size);
    }

    /* Fall back to stdio */
    FILE *f = fopen(path, "rb");
    if (f) {
        if (size) {
            fseek(f, 0, SEEK_END);
            *size = (int32_t)ftell(f);
        }
        fclose(f);
        return RETRO_VFS_STAT_IS_VALID;
    }

    return 0;
}

int MakeDir(const char *path)
{
    LR_VFS_DEBUG("MakeDir: path=%s\n", path);

    if (!path) {
        return -1;
    }

    struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
    if (vfs && vfs->mkdir) {
        return vfs->mkdir(path);
    }

    /* Fall back to platform-specific mkdir */
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

int Remove(const char *path)
{
    LR_VFS_DEBUG("Remove: path=%s\n", path);

    if (!path) {
        return -1;
    }

    struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
    if (vfs && vfs->remove) {
        return vfs->remove(path);
    }

    return remove(path);
}

int Rename(const char *old_path, const char *new_path)
{
    LR_VFS_DEBUG("Rename: old=%s, new=%s\n", old_path, new_path);

    if (!old_path || !new_path) {
        return -1;
    }

    struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
    if (vfs && vfs->rename) {
        return vfs->rename(old_path, new_path);
    }

    return rename(old_path, new_path);
}

DirHandle *OpenDir(const char *path)
{
    LR_VFS_DEBUG("OpenDir: path=%s\n", path);

    if (!path) {
        return nullptr;
    }

    DirHandle *handle = new DirHandle();
    handle->path = path;
    handle->vfs_handle = nullptr;
    handle->stdio_handle = nullptr;
    handle->using_vfs = false;

    struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
    if (vfs && vfs->opendir) {
        handle->vfs_handle = vfs->opendir(path, true);
        if (handle->vfs_handle) {
            handle->using_vfs = true;
            LR_VFS_DEBUG("OpenDir: VFS open successful\n");
            return handle;
        }
    }

    /* Fall back to platform-specific directory iteration would go here */
    LR_VFS_WARN("OpenDir: Directory iteration not implemented for stdio fallback\n");
    delete handle;
    return nullptr;
}

bool ReadDir(DirHandle *handle)
{
    LR_VFS_DEBUG("ReadDir: handle=%p\n", (void*)handle);

    if (!handle) {
        return false;
    }

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->readdir && handle->vfs_handle) {
            return vfs->readdir(handle->vfs_handle);
        }
    }

    return false;
}

const char *GetDirEntryName(DirHandle *handle)
{
    LR_VFS_DEBUG("GetDirEntryName: handle=%p\n", (void*)handle);

    if (!handle) {
        return nullptr;
    }

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->dirent_get_name && handle->vfs_handle) {
            return vfs->dirent_get_name(handle->vfs_handle);
        }
    }

    return nullptr;
}

bool IsDirEntryDir(DirHandle *handle)
{
    LR_VFS_DEBUG("IsDirEntryDir: handle=%p\n", (void*)handle);

    if (!handle) {
        return false;
    }

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->dirent_is_dir && handle->vfs_handle) {
            return vfs->dirent_is_dir(handle->vfs_handle);
        }
    }

    return false;
}

int CloseDir(DirHandle *handle)
{
    LR_VFS_DEBUG("CloseDir: handle=%p\n", (void*)handle);

    if (!handle) {
        return -1;
    }

    int result = 0;

    if (handle->using_vfs) {
        struct retro_vfs_interface *vfs = LibretroCore::GetVFS();
        if (vfs && vfs->closedir && handle->vfs_handle) {
            result = vfs->closedir(handle->vfs_handle);
        }
    }

    delete handle;
    return result;
}

bool IsAvailable()
{
    return LibretroCore::HasVFS();
}

} /* namespace LibretroVFS */

#endif /* WITH_LIBRETRO */
