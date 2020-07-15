#include "DiskImage.h"

DiskImage::DiskImage(const std::string &path, blkif_sector_t sector_size) : mBackingFile(path, std::fstream::binary|std::fstream::in|std::fstream::out), mSectorSize(sector_size)
{
    mBackingFile.seekg(0, mBackingFile.end);
    mSectorCount = mBackingFile.tellg()/mSectorSize;
    mBackingFile.seekg(0, mBackingFile.beg);
}

DiskImage::~DiskImage()
{
    flushBackingFile();
}

int8_t
DiskImage::createBackingFile(const std::string &path, blkif_sector_t num_sectors, blkif_sector_t sector_size)
{
    std::vector<char> empty(sector_size, 0);
    std::ofstream ofs(path, std::ios::binary | std::ios::out);

    for(uint64_t i = 0; i < num_sectors; i++)
    {
        if (!ofs.write(&empty[0], empty.size()))
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
    /*
    if(sector.size() != mSectorSize) {
        return -1;
    }
    */
    if (sector_number >= mSectorCount) {
        return -1;
    }
 
    mBackingFile.seekg(mSectorSize*sector_number, mBackingFile.beg);
    mBackingFile.read(sector.data(), sector.size());
    if ((mBackingFile.rdstate() & std::ifstream::failbit)){
        return -1;
    }
    mBackingFile.sync();
    
    return 0;
}

int8_t
DiskImage::writeSector(blkif_sector_t sector_number, const std::vector<char> &sector)
{
    if (sector_number >= mSectorCount) {
        return -1;
    }

    mBackingFile.seekp(mSectorSize*sector_number);
    mBackingFile.write(sector.data(), sector.size());
    if ((mBackingFile.rdstate() & std::ifstream::failbit)){
        return -1;
    }
    mBackingFile.flush();
    
    return 0;
}

#include <cstring>

int8_t
DiskImage::readSector(blkif_sector_t sector_number, char *buffer, uint32_t size)
{
    if (sector_number >= mSectorCount) {
        return -1;
    }
    
    mBackingFile.seekg(mSectorSize*sector_number, mBackingFile.beg);
    mBackingFile.read(buffer, size);
    if ((mBackingFile.rdstate() & std::ifstream::failbit)){
        return -1;
    }
    mBackingFile.sync();

    return 0;
}

int8_t
DiskImage::writeSector(blkif_sector_t sector_number, char *buffer, uint32_t size)
{
    if (sector_number >= mSectorCount) {
        return 0;
    }
    
    mBackingFile.seekp(mSectorSize*sector_number, mBackingFile.beg);
    mBackingFile.write(buffer, size);
    if ((mBackingFile.rdstate() & std::ifstream::failbit)){
        exit(-1);
        return -1;
    }
    mBackingFile.flush();
    
    return 0;
}

int8_t
DiskImage::discard(blkif_sector_t sector_number, uint64_t count)
{
    if (sector_number + count >= mSectorCount) {
        return -1;
    }

    const char zero = 0x00;
    
    mBackingFile.seekp(mSectorSize*sector_number);
    
    for(uint64_t i = 0; i < count*mSectorSize; i++) {
        mBackingFile.put(zero);
    }
    
    if ((mBackingFile.rdstate() & std::ifstream::failbit)){
        return -1;
    }
    return 0;
}

void
DiskImage::flushBackingFile()
{
    mBackingFile.sync();
    mBackingFile.flush();
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
