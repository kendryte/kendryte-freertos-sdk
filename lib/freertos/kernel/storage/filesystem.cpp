/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "filesystem.h"
#include "FreeRTOS.h"
#include "devices.h"
#include "kernel/driver_impl.hpp"
#include <array>
#include <cstring>
#include <diskio.h>
#include <ff.h>

using namespace sys;

#define MAX_FILE_SYSTEMS 16

static void check_fatfs_error(FRESULT result)
{
    static const char *err_str[] = {
        "(0) Succeeded",
        "(1) A hard error occurred in the low level disk I/O layer",
        "(2) Assertion failed",
        "(3) The physical drive cannot work",
        "(4) Could not find the file",
        "(5) Could not find the path",
        "(6) The path name format is invalid",
        "(7) Access denied due to prohibited access or directory full",
        "(8) Access denied due to prohibited access",
        "(9) The file/directory object is invalid",
        "(10) The physical drive is write protected",
        "(11) The logical drive number is invalid",
        "(12) The volume has no work area",
        "(13) There is no valid FAT volume",
        "(14) The f_mkfs() aborted due to any problem",
        "(15) Could not get a grant to access the volume within defined period",
        "(16) The operation is rejected according to the file sharing policy",
        "(17) LFN working buffer could not be allocated",
        "(18) Number of open files > FF_FS_LOCK",
        "(19) Given parameter is invalid"
    };

    if (result != FR_OK)
        throw std::runtime_error(err_str[result]);
}

static const char *normalize_path(const char *name)
{
    auto str = std::strstr(name, "/fs/");
    if (!str)
        throw std::runtime_error("Invalid path.");
    return str + 4;
}

class k_filesystem : public virtual object, public heap_object
{
public:
    FATFS FatFS;

    k_filesystem(object_accessor<block_storage_driver> storage)
        : storage_(std::move(storage))
    {
    }

    block_storage_driver &get_storage() noexcept
    {
        return *storage_.operator->();
    }

    static object_ptr<k_filesystem> install_filesystem(object_accessor<block_storage_driver> storage)
    {
        auto obj = make_object<k_filesystem>(std::move(storage));

        for (size_t i = 0; i < filesystems_.size(); i++)
        {
            if (!filesystems_[i])
            {
                filesystems_[i] = obj;
                return obj;
            }
        }

        throw std::runtime_error("Max custom drivers exceeded.");
    }

    static object_ptr<k_filesystem> get_filesystem(size_t index)
    {
        return filesystems_.at(index);
    }

private:
    static std::array<object_ptr<k_filesystem>, MAX_FILE_SYSTEMS> filesystems_;

    object_accessor<block_storage_driver> storage_;
};

std::array<object_ptr<k_filesystem>, MAX_FILE_SYSTEMS> k_filesystem::filesystems_;

class k_filesystem_file : public filesystem_file, public heap_object, public exclusive_object_access
{
public:
    k_filesystem_file(const char *fileName, file_access_t file_access, file_mode_t file_mode)
    {
        BYTE mode = 0;
        if (file_access & FILE_ACCESS_READ)
            mode |= FA_READ;
        if (file_access & FILE_ACCESS_WRITE)
            mode |= FA_WRITE;
        if (file_mode & FILE_MODE_OPEN_EXISTING)
            mode |= FA_OPEN_EXISTING;
        else if (file_mode & FILE_MODE_OPEN_ALWAYS)
            mode |= FA_OPEN_ALWAYS;
        else if (file_mode & FILE_MODE_CREATE_NEW)
            mode |= FA_CREATE_NEW;
        else if (file_mode & FILE_MODE_CREATE_ALWAYS)
            mode |= FA_CREATE_ALWAYS;
        else if (file_mode & FILE_MODE_APPEND)
            mode |= FA_OPEN_APPEND;

        check_fatfs_error(f_open(&file_, normalize_path(fileName), mode));

        if (file_mode & FILE_MODE_TRUNCATE)
        {
            auto err = f_truncate(&file_);
            if (err != FR_OK)
                f_close(&file_);
            check_fatfs_error(err);
        }
    }

    ~k_filesystem_file()
    {
        f_close(&file_);
    }

    virtual size_t read(gsl::span<uint8_t> buffer) override
    {
        UINT read = buffer.size();
        check_fatfs_error(f_read(&file_, buffer.data(), read, &read));
        return read;
    }

    virtual size_t write(gsl::span<const uint8_t> buffer) override
    {
        UINT written = buffer.size();
        check_fatfs_error(f_write(&file_, buffer.data(), written, &written));
        if (written != buffer.size())
            throw std::runtime_error("Disk full.");
        return written;
    }

    virtual fpos_t get_position() override
    {
        return f_tell(&file_);
    }

    virtual void set_position(fpos_t position) override
    {
        check_fatfs_error(f_lseek(&file_, position));
    }

    virtual uint64_t get_size() override
    {
        return f_size(&file_);
    }

    virtual void flush() override
    {
        check_fatfs_error(f_sync(&file_));
    }

private:
    FIL file_;
};

class k_filesystem_find : public virtual object_access, public heap_object, public exclusive_object_access
{
public:
    k_filesystem_find(const char *path, const char *pattern)
    {
        check_fatfs_error(f_findfirst(&dir_, &info_, normalize_path(path), pattern));
    }

    void fill_find_data(find_find_data_t &find_data)
    {
        strcpy(find_data.filename, info_.fname);
    }

    bool move_next()
    {
        return f_findnext(&dir_, &info_) == FR_OK && info_.fname[0];
    }

private:
    DIR dir_;
    FILINFO info_;
};

int filesystem_mount(const char *name, handle_t storage_handle)
{
    try
    {
        auto fs = k_filesystem::install_filesystem(system_handle_to_object(storage_handle).move_as<block_storage_driver>());
        check_fatfs_error(f_mount(&fs->FatFS, normalize_path(name), 1));
        return 0;
    }
    catch (...)
    {
        return -1;
    }
}

#define FILE_ENTRY                             \
    auto &obj = system_handle_to_object(file); \
    configASSERT(obj.is<filesystem_file>());   \
    auto f = obj.as<filesystem_file>();

#define CATCH_ALL \
    catch (...) { return -1; }

handle_t filesystem_file_open(const char *filename, file_access_t file_access, file_mode_t file_mode)
{
    try
    {
        auto file = make_object<k_filesystem_file>(filename, file_access, file_mode);
        return system_alloc_handle(make_accessor<object_access>(file));
    }
    catch (...)
    {
        return NULL_HANDLE;
    }
}

int filesystem_file_close(handle_t file)
{
    return io_close(file);
}

int filesystem_file_read(handle_t file, uint8_t *buffer, size_t buffer_len)
{
    try
    {
        FILE_ENTRY;

        return f->read({ buffer, std::ptrdiff_t(buffer_len) });
    }
    CATCH_ALL;
}

int filesystem_file_write(handle_t file, const uint8_t *buffer, size_t buffer_len)
{
    try
    {
        FILE_ENTRY;

        return f->write({ buffer, std::ptrdiff_t(buffer_len) });
    }
    CATCH_ALL;
}

fpos_t filesystem_file_get_position(handle_t file)
{
    try
    {
        FILE_ENTRY;

        return f->get_position();
    }
    CATCH_ALL;
}

int filesystem_file_set_position(handle_t file, fpos_t position)
{
    try
    {
        FILE_ENTRY;

        f->set_position(position);
        return 0;
    }
    CATCH_ALL;
}

uint64_t filesystem_file_get_size(handle_t file)
{
    try
    {
        FILE_ENTRY;

        return f->get_size();
    }
    CATCH_ALL;
}

int filesystem_file_flush(handle_t file)
{
    try
    {
        FILE_ENTRY;

        f->flush();
        return 0;
    }
    CATCH_ALL;
}

#define FIND_ENTRY                               \
    auto &obj = system_handle_to_object(handle); \
    configASSERT(obj.is<k_filesystem_find>());   \
    auto f = obj.as<k_filesystem_find>();

handle_t filesystem_find_first(const char *path, const char *pattern, find_find_data_t *find_data)
{
    try
    {
        auto find = make_object<k_filesystem_find>(path, pattern);
        find->fill_find_data(*find_data);
        auto handle = system_alloc_handle(make_accessor<object_access>(find));
        return handle;
    }
    catch (...)
    {
        return NULL_HANDLE;
    }
}

bool filesystem_find_next(handle_t handle, find_find_data_t *find_data)
{
    try
    {
        FIND_ENTRY;

        if (!f->move_next())
            return false;
        f->fill_find_data(*find_data);
        return true;
    }
    CATCH_ALL;
}

int filesystem_find_close(handle_t handle)
{
    return io_close(handle);
}

extern "C"
{
    DSTATUS disk_initialize(BYTE pdrv)
    {
        k_filesystem::get_filesystem(pdrv);
        return RES_OK;
    }

    DSTATUS disk_status(
        BYTE pdrv /* Physical drive nmuber to identify the drive */
    )
    {
        k_filesystem::get_filesystem(pdrv);
        return RES_OK;
    }

    DRESULT disk_read(
        BYTE pdrv, /* Physical drive nmuber to identify the drive */
        BYTE *buff, /* Data buffer to store read data */
        DWORD sector, /* Start sector in LBA */
        UINT count /* Number of sectors to read */
    )
    {
        auto fs = k_filesystem::get_filesystem(pdrv);
        auto &st = fs->get_storage();

        st.read_blocks(sector, count, { buff, ptrdiff_t(st.get_rw_block_size() * count) });
        return RES_OK;
    }

    DRESULT disk_write(
        BYTE pdrv, /* Physical drive nmuber to identify the drive */
        const BYTE *buff, /* Data to be written */
        DWORD sector, /* Start sector in LBA */
        UINT count /* Number of sectors to write */
    )
    {
        auto fs = k_filesystem::get_filesystem(pdrv);
        auto &st = fs->get_storage();

        st.write_blocks(sector, count, { buff, ptrdiff_t(st.get_rw_block_size() * count) });
        return RES_OK;
    }

    DRESULT disk_ioctl(
        BYTE pdrv, /* Physical drive nmuber (0..) */
        BYTE cmd, /* Control code */
        void *buff /* Buffer to send/receive control data */
    )
    {
        auto fs = k_filesystem::get_filesystem(pdrv);
        auto &st = fs->get_storage();

        switch (cmd)
        {
        case CTRL_SYNC:
            break;
        case GET_SECTOR_COUNT:
            *(DWORD *)buff = st.get_blocks_count();
            break;
        case GET_SECTOR_SIZE:
            *(DWORD *)buff = st.get_rw_block_size();
            break;
        case GET_BLOCK_SIZE:
            *(DWORD *)buff = st.get_rw_block_size();
            break;
        default:
            return RES_PARERR;
        }

        return RES_OK;
    }

    DWORD get_fattime(void)
    {
        return 0;
    }
}
