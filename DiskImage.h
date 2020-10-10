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
    DiskImage(const std::string &path);
    ~DiskImage();

    static int8_t createBackingFile(const std::string &path,
                                    blkif_sector_t num_sectors,
                                    blkif_sector_t sector_size);

    int writeSector(blkif_sector_t sector_number, const std::vector<char> &sector);
    int readSector(blkif_sector_t sector_number, std::vector<char> &sector);

    int writeSectors(blkif_sector_t start_sector, uint64_t nr_sectors, const uint8_t *buffer);
    int readSectors(blkif_sector_t start_sector, uint64_t nr_sectors, uint8_t *buffer);
    int discard(blkif_sector_t start_sector, uint64_t nr_sectors);

    void flushBackingFile();

    constexpr uint32_t getSectorSize() const noexcept { return SECTOR_SIZE; }
    uint64_t getSectorCount() const noexcept { return mSectorCount; }

private:
    std::fstream mBackingFile;
    uint64_t mSectorCount{0};

    std::unique_ptr<MemoryMappedFile> mFile{nullptr};
};

#endif // DISK_IMAGE__H
