#include "DiskImage.h"

int main(int argc, const char **argv)
{
    int8_t rc = DiskImage::createBackingFile("./test.img", 1024, 512);
    if (rc) {
        return rc;
    }

    DiskImage di("./test.img");

    std::vector<char> ff(512, 0xff);
    std::vector<char> empty(512, 0x00);

    rc = di.writeSector(512, ff);
    if (rc) {
        return rc;
    }

    di.readSector(512, empty);
    if (rc) {
        return rc;
    }

    return 0;
}
