/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_vfs.h Libretro VFS wrapper header for OpenTTD file operations. */

#ifndef VIDEO_LIBRETRO_VFS_H
#define VIDEO_LIBRETRO_VFS_H

#include <cstdint>

/* File access modes */
#define LIBRETRO_VFS_MODE_READ   (1 << 0)
#define LIBRETRO_VFS_MODE_WRITE  (1 << 1)
#define LIBRETRO_VFS_MODE_UPDATE (1 << 2)

/**
 * Namespace for libretro VFS operations.
 * Provides a portable file I/O interface that uses the libretro VFS when available,
 * falling back to standard library functions when not.
 */
namespace LibretroVFS {

/* Opaque file handle */
struct FileHandle;

/* Opaque directory handle */
struct DirHandle;

/**
 * Open a file.
 * @param path Path to the file.
 * @param mode Access mode (LIBRETRO_VFS_MODE_READ, WRITE, UPDATE flags).
 * @return File handle or nullptr on failure.
 */
FileHandle *Open(const char *path, unsigned mode);

/**
 * Close a file.
 * @param handle File handle to close.
 * @return 0 on success, -1 on failure.
 */
int Close(FileHandle *handle);

/**
 * Get the size of a file.
 * @param handle File handle.
 * @return File size in bytes, or -1 on failure.
 */
int64_t Size(FileHandle *handle);

/**
 * Get the current position in a file.
 * @param handle File handle.
 * @return Current position, or -1 on failure.
 */
int64_t Tell(FileHandle *handle);

/**
 * Seek to a position in a file.
 * @param handle File handle.
 * @param offset Offset to seek to.
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END.
 * @return New position, or -1 on failure.
 */
int64_t Seek(FileHandle *handle, int64_t offset, int whence);

/**
 * Read from a file.
 * @param handle File handle.
 * @param buffer Buffer to read into.
 * @param len Number of bytes to read.
 * @return Number of bytes read, or -1 on failure.
 */
int64_t Read(FileHandle *handle, void *buffer, uint64_t len);

/**
 * Write to a file.
 * @param handle File handle.
 * @param buffer Buffer to write from.
 * @param len Number of bytes to write.
 * @return Number of bytes written, or -1 on failure.
 */
int64_t Write(FileHandle *handle, const void *buffer, uint64_t len);

/**
 * Flush a file's buffers.
 * @param handle File handle.
 * @return 0 on success, -1 on failure.
 */
int Flush(FileHandle *handle);

/**
 * Get file status.
 * @param path Path to the file.
 * @param size Output: file size (optional).
 * @return Status flags (RETRO_VFS_STAT_*), or 0 if not found.
 */
int Stat(const char *path, int32_t *size);

/**
 * Create a directory.
 * @param path Path to create.
 * @return 0 on success, -1 on failure.
 */
int MakeDir(const char *path);

/**
 * Remove a file.
 * @param path Path to the file.
 * @return 0 on success, -1 on failure.
 */
int Remove(const char *path);

/**
 * Rename a file.
 * @param old_path Current path.
 * @param new_path New path.
 * @return 0 on success, -1 on failure.
 */
int Rename(const char *old_path, const char *new_path);

/**
 * Open a directory for iteration.
 * @param path Path to the directory.
 * @return Directory handle or nullptr on failure.
 */
DirHandle *OpenDir(const char *path);

/**
 * Read the next entry from a directory.
 * @param handle Directory handle.
 * @return true if an entry was read, false if no more entries.
 */
bool ReadDir(DirHandle *handle);

/**
 * Get the name of the current directory entry.
 * @param handle Directory handle.
 * @return Entry name, or nullptr on failure.
 */
const char *GetDirEntryName(DirHandle *handle);

/**
 * Check if the current directory entry is a directory.
 * @param handle Directory handle.
 * @return true if directory, false otherwise.
 */
bool IsDirEntryDir(DirHandle *handle);

/**
 * Close a directory.
 * @param handle Directory handle to close.
 * @return 0 on success, -1 on failure.
 */
int CloseDir(DirHandle *handle);

/**
 * Check if the VFS interface is available.
 * @return true if VFS is available.
 */
bool IsAvailable();

} /* namespace LibretroVFS */

#endif /* VIDEO_LIBRETRO_VFS_H */
