#include "DiskImage.h"

DiskImage::DiskImage(const std::string &path, blkif_sector_t sector_size) : mBackingFile(path, std::fstream::binary|std::fstream::in|std::fstream::out), mSectorSize(sector_size)
{
    mBackingFile.seekg(0, mBackingFile.end);
    mSectorCount = mBackingFile.tellg()/mSectorSize;
}

DiskImage::~DiskImage()
{

}

int8_t
DiskImage::createBackingFile(const std::string &path, blkif_sector_t num_sectors, blkif_sector_t sector_size)
{
    std::vector<char> empty(sector_size, 0);
    std::ofstream ofs(path, std::ios::binary | std::ios::out);

    for(int i = 0; i < num_sectors; i++)
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
    if(sector.size() != mSectorSize) {
        return -1;
    }

    if (sector_number > mSectorCount) {
        return -1;
    }

    mBackingFile.seekg(mSectorSize*sector_number);
    mBackingFile.read(&sector[0], mSectorSize);

    return 0;
}

int8_t
DiskImage::writeSector(blkif_sector_t sector_number, const std::vector<char> &sector)
{
    if(sector.size() != mSectorSize) {
        return -1;
    }

    if (sector_number > mSectorCount) {
        return -1;
    }

    mBackingFile.seekg(mSectorSize*sector_number);
    mBackingFile.write(&sector[0], mSectorSize);

    return 0;
}
