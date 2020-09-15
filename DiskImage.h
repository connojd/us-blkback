#ifndef DISK_IMAGE__H
#define DISK_IMAGE__H
#include <memory>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include "MemoryMappedFile.h"

extern "C" {
#include <xen/io/blkif.h>
};

#define SECTOR_SIZE 512

class DiskImage {
public:
    DiskImage(const std::string &path, blkif_sector_t sector_size = 512);
    ~DiskImage();

    static int8_t createBackingFile(const std::string &path,
                                    blkif_sector_t num_sectors,
                                    blkif_sector_t sector_size);

    int8_t writeSector(blkif_sector_t sector_number, const std::vector<char> &sector);
    int8_t readSector(blkif_sector_t sector_number, std::vector<char> &sector);

    int8_t writeSector(blkif_sector_t sector_number, char *buffer, uint32_t size);
    int8_t readSector(blkif_sector_t sector_number, char *buffer, uint32_t size);

    int8_t discard(blkif_sector_t sector_number, uint64_t count);

    void flushBackingFile();

    uint32_t getSectorSize() { return mSectorSize; }
    uint32_t getSectorCount() { return mSectorCount; }
private:
    std::fstream mBackingFile;
    blkif_sector_t mSectorSize{SECTOR_SIZE};
    blkif_sector_t mSectorCount{0};

    std::unique_ptr<MemoryMappedFile> mFile{nullptr};
};

#endif // DISK_IMAGE__H
