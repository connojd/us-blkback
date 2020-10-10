#include "DiskImage.h"
#include <cstring>

#ifndef _WIN32
#define memmapfile UnixMemoryMappedFile
#else
#define memmapfile WinMemoryMappedFile
#endif

DiskImage::DiskImage(const std::string &path) :
    mFile(new memmapfile(path))
{
    if (mFile->size() == 0U) {
        std::cerr << "Size of file " << path << " is 0, bailing";
        throw;
    }

    if ((mFile->size() % this->getSectorSize()) != 0U) {
        std::cerr << "Size of file " << path << " is not a multiple of "
                  << "the sector size (file size = " << mFile->size() << "B, "
                  << "sector size = " << this->getSectorSize() << "B), bailing";
        throw;
    }

    mSectorCount = mFile->size() / this->getSectorSize();

    if (mSectorCount >= INT_MAX) {
        std::cerr << "Sector count (=" << mSectorCount << ") of file " << path
                  << " is too large, bailing";
        throw;
    }
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

int
DiskImage::readSector(blkif_sector_t sector_number, std::vector<char> &sector)
{
    if(sector.size() != this->getSectorSize()) {
        return BLKIF_RSP_ERROR;
    }

    if (sector_number >= this->getSectorCount()) {
        return BLKIF_RSP_ERROR;
    }

    memcpy(sector.data(),
           mFile->get() + uintptr_t(sector_number * this->getSectorSize()),
           sector.size());

    return BLKIF_RSP_OKAY;
}

int
DiskImage::writeSector(blkif_sector_t sector_number, const std::vector<char> &sector)
{
    if(sector.size() != this->getSectorSize()) {
        return BLKIF_RSP_ERROR;
    }

    if (sector_number >= this->getSectorCount()) {
        return BLKIF_RSP_ERROR;
    }

    memcpy(mFile->get() + (uintptr_t)(sector_number * this->getSectorSize()),
           sector.data(),
           sector.size());

    return BLKIF_RSP_OKAY;
}

int
DiskImage::readSectors(blkif_sector_t start_sector,
                       uint64_t nr_sectors,
                       uint8_t *buffer)
{
    const blkif_sector_t last_sector = start_sector + nr_sectors - 1U;

    if (last_sector >= this->getSectorCount()) {
        std::cerr << "readSector failed, last_sector = " << last_sector
                  << ", sector_count = " << this->getSectorCount();
        return BLKIF_RSP_ERROR;
    }

    memcpy(buffer,
           mFile->get() + (uintptr_t)(start_sector * this->getSectorSize()),
           nr_sectors * this->getSectorSize());

    return BLKIF_RSP_OKAY;
}

int
DiskImage::writeSectors(blkif_sector_t start_sector,
                        uint64_t nr_sectors,
                        const uint8_t *buffer)
{
    const blkif_sector_t last_sector = start_sector + nr_sectors - 1U;

    if (last_sector >= this->getSectorCount()) {
        std::cerr << "writeSector failed, last_sector = " << last_sector
                  << ", sector_count = " << this->getSectorCount();
        return BLKIF_RSP_ERROR;
    }

    memcpy(mFile->get() + (uintptr_t)(start_sector * this->getSectorSize()),
           buffer,
           nr_sectors * this->getSectorSize());

    return BLKIF_RSP_OKAY;
}

int
DiskImage::discard(blkif_sector_t start_sector, uint64_t nr_sectors)
{
    if (start_sector + nr_sectors > this->getSectorCount()) {
        return BLKIF_RSP_ERROR;
    }

    memset(mFile->get() + uintptr_t(start_sector * this->getSectorSize()),
           0x00,
           nr_sectors * this->getSectorSize());

    return BLKIF_RSP_OKAY;
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
