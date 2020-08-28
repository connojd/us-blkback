#ifndef MEMORYMAPPEDFILE__H
#define MEMORYMAPPEDFILE__H

class MemoryMappedFile {
public:
    MemoryMappedFile(const std::string &path) {}
    virtual ~MemoryMappedFile() {}

    virtual char *get() = 0;
    virtual void flush() = 0;
    virtual uint64_t size() = 0;
};

#ifdef _WIN32

#include <Windows.h>

class WinMemoryMappedFile : public MemoryMappedFile {
public:
WinMemoryMappedFile(const std::string &path) : MemoryMappedFile(path)
    {
        LARGE_INTEGER size;
		SetCurrentDirectory("C:\\");
        m_file = CreateFileA(path.c_str(),
                             (GENERIC_READ | GENERIC_WRITE),
                             0,
                             nullptr,
                             OPEN_EXISTING,
                             (FILE_ATTRIBUTE_NORMAL),
                             nullptr);

        if(m_file == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            throw;
        }

        if(!GetFileSizeEx(m_file, &size)) {
            DWORD err = GetLastError();
            throw;
        }

        m_size = size.QuadPart;
        m_file_view = CreateFileMappingA(
            m_file,
            nullptr,
            PAGE_READWRITE,
            size.u.HighPart,
            size.u.LowPart,
            nullptr);

        if(!m_file_view) {
            DWORD err = GetLastError();
            throw;
        }

        m_ptr = MapViewOfFile(
            m_file_view,
            (FILE_MAP_READ|FILE_MAP_WRITE),
            0,
            0,
            m_size);
        if(!m_ptr) {
            DWORD err = GetLastError();
            throw;
        }

		FlushViewOfFile(m_ptr, m_size);
    }

    ~WinMemoryMappedFile()
    {
        flush();

        UnmapViewOfFile(m_ptr);
        m_ptr = nullptr;

        m_size = 0;

        CloseHandle(m_file_view);
        m_file_view = nullptr;

        CloseHandle(m_file);
        m_file = nullptr;
    }

    virtual char *get() { return (char *)m_ptr; }
    virtual void flush()
    {
        FlushViewOfFile(m_ptr, m_size);
    }
    virtual uint64_t size() { return m_size; }

private:
    HANDLE m_file;
    HANDLE m_file_view;
    uint64_t m_size{0};
    std::string m_path;
    void *m_ptr{nullptr};
};
#else

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

class UnixMemoryMappedFile : public MemoryMappedFile {
public:
UnixMemoryMappedFile(const std::string &path) : MemoryMappedFile(path)
    {
        struct stat sb;
        m_fd = open(path.c_str(), O_RDWR);
        if(m_fd < -1) {
            throw;
        }

        if (fstat(m_fd, &sb) == -1) {
            throw;
        }

        m_size = sb.st_size;
        m_ptr = mmap(NULL, m_size, PROT_READ|PROT_WRITE,
                    MAP_SHARED, m_fd, 0);
        if(m_ptr == MAP_FAILED) {
            throw;
        }

    }

    ~UnixMemoryMappedFile()
    {
        msync(m_ptr, m_size, MS_SYNC);
        munmap(m_ptr, m_size);
        close(m_fd);
    }

    char* get() { return (char*) m_ptr; }
    void flush() { msync(m_ptr, m_size, MS_SYNC); }
    uint64_t size() { return m_size; }

private:
    int m_fd{-1};
    uint64_t m_size{0};
    std::string m_path;
    void *m_ptr{nullptr};
};
#endif // _WIN32
#endif //MEMORYMAPPEDFILE__H
