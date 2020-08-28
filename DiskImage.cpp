#include "DiskImage.h"
#include <cstring>

#ifndef _WIN32
#define memmapfile UnixMemoryMappedFile
#else
#define memmapfile WinMemoryMappedFile
#endif

DiskImage::DiskImage(const std::string &path, blkif_sector_t sector_size) : mSectorSize(sector_size), mFile(new memmapfile(path))
{
    mSectorCount = mFile->size()/mSectorSize;
}

DiskImage::~DiskImage()
{
    flushBackingFile();
}

int8_t
DiskImage::createBackingFile(const std::string &path,
                             blkif_sector_t num_sectors,
                             blkif_sector_t sector_size)
{
    std::vector<char> empty(sector_size, 0);
    std::ofstream ofs(path, std::ios::binary | std::ios::out);

    for(uint64_t i = 0; i < num_sectors; i++)
    {
        if (!ofs.write(empty.data(), empty.size()))
        {
            std::cerr << "problem writing to file" << std::endl;
            return -1;
        }
    }

    return 0;
}

int8_t
DiskImage::readSector(blkif_sector_t sector_number, std::vector<char> &sector)
{
    if(sector.size() != mSectorSize) {
        return -1;
    }

    if (sector_number >= mSectorCount) {
        return -1;
    }

    memcpy(sector.data(), mFile->get() + uintptr_t(sector_number*mSectorSize), sector.size());

    return 0;
}

int8_t
DiskImage::writeSector(blkif_sector_t sector_number, const std::vector<char> &sector)
{
    if(sector.size() != mSectorSize) {
        return -1;
    }

    if (sector_number >= mSectorCount) {
        return -1;
    }

    memcpy(mFile->get() + (uintptr_t)(sector_number*mSectorSize), sector.data(), sector.size());
    mFile->flush();

    return 0;
}

int8_t
DiskImage::readSector(blkif_sector_t sector_number, char *buffer, uint32_t size)
{
    if (sector_number >= mSectorCount) {
        return -1;
    }

	mFile->flush();

    memcpy(buffer,
           mFile->get() + (uintptr_t)(sector_number*mSectorSize),
           size);

    return 0;
}

int8_t
DiskImage::writeSector(blkif_sector_t sector_number, char *buffer, uint32_t size)
{
    if (sector_number >= mSectorCount) {
        return 0;
    }

    memcpy(mFile->get() + uintptr_t(sector_number*mSectorSize),
           buffer,
           size);

    return 0;
}

int8_t
DiskImage::discard(blkif_sector_t sector_number, uint64_t count)
{
    if (sector_number + count >= mSectorCount) {
        return -1;
    }

    memset(mFile->get() + uintptr_t(sector_number*mSectorSize),
           0x00,
           count*mSectorSize);

    return 0;
}

void
DiskImage::flushBackingFile()
{
    mFile->flush();
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
