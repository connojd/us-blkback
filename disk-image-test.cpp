#include "DiskImage.h"
#include <fstream>
#include <string>
#include <sys/stat.h>
#define CATCH_CONFIG_MAIN
#include "include/catch.hpp"


int8_t rc = DiskImage::createBackingFile("./test.img", 1024, 512);
DiskImage di("./test.img");


TEST_CASE("Test Create Backing File", "[backingFile]"){
    SECTION("Checking if ./test.img exists"){
        // create test.img
        const std::string& name = "./test.img";
        struct stat buffer;
        REQUIRE(stat (name.c_str(), &buffer) == 0); // determine if the file exists
    }
}

TEST_CASE("Test Read Sector","[readSector]"){
    SECTION("Checking read Sector Function"){
        std::vector<char> ff(512, 0xff);
        std::vector<char> empty(512, 0x00);

        rc = di.readSector(512, empty);
        REQUIRE(rc == 0);

        rc = di.readSector(512, ff);
        REQUIRE(rc == 0);

        int16_t tooHigh = 10000;
        rc = di.readSector(tooHigh, empty);
        REQUIRE(rc == -1);

        rc = di.readSector(tooHigh, ff);
        REQUIRE(rc == -1);
    }
}

TEST_CASE("Test Write Sector","[readSector]"){
    SECTION("Checking Write Sector"){
        std::vector<char> ff(512, 0xff);
        std::vector<char> empty(512, 0x00);

        // WRITE SECTOR
        rc = di.writeSector(512, ff);
        REQUIRE(rc == 0);

        rc = di.writeSector(256, ff);
        REQUIRE(rc == 0);

        rc = di.writeSector(256, ff);
        REQUIRE(rc == 0);

        int16_t tooHigh = 10000;
        rc = di.writeSector(tooHigh, ff);
        REQUIRE(rc == -1);
    }
}

TEST_CASE("Sector Read/Write is Bounded Correctly", "[writeSector]"){
    SECTION("Sector Write Bounding"){
        std::vector<char> buf1(512,0xAA);
        std::vector<char> buf2(512, 0xBB);
        std::vector<char> buf3(512,0xCC);
        std::vector<char> buf4(512,0xDD);
        std::vector<char> buf5(512,0xEE);
        std::vector<char> buf6(512,0xFF);

        blkif_sector_t sectNum = 512;
        std::vector<char> empty(512, 0x00);
        std::vector<char> ff(512, 0xff);

        //511
        rc = di.writeSector(sectNum-1, empty);
        rc = di.readSector(sectNum-1,buf1);
        REQUIRE(std::equal(empty.begin(),empty.end(), buf1.begin()) == true);

        //512
        rc = di.writeSector(sectNum, ff);
        rc = di.readSector(sectNum, buf2);
        REQUIRE(std::equal(ff.begin(), ff.end(), buf2.begin()) == true);

        //513
        rc = di.writeSector(sectNum+1, empty);
        rc = di.readSector(sectNum+1, buf3);
        REQUIRE(std::equal(empty.begin(), empty.end(),buf3.begin())==true);

        // reverse order 513 -> 512 -> 511
        //513
        rc = di.writeSector(sectNum+1, empty);
        rc = di.readSector(sectNum+1, buf4);
        REQUIRE(std::equal(empty.begin(), empty.end(),buf4.begin())==true);

        // 512
        rc = di.writeSector(sectNum, ff);
        rc = di.readSector(sectNum, buf5);
        REQUIRE(std::equal(ff.begin(), ff.end(), buf5.begin()) == true);

        //511
        rc = di.writeSector(sectNum, empty);
        rc = di.readSector(sectNum, buf6);
        REQUIRE(std::equal(empty.begin(), empty.end(), buf6.begin()) == true);
    }
    SECTION("Testing zero and edge cases"){
        blkif_sector_t sectNum = 512;
        std::vector<char> empty(512, 0x00);
        std::vector<char> ff(512, 0xff);

        //large sector cases
        // large sectNumber
        std::vector<char> buf7(512,0x11);
        std::vector<char> buf8(512,0x22);
        std::vector<char> buf9(512,0x33);
        std::vector<char> buf10(512,0x44);

        //512*2 -> 2024
        //ff
        rc = di.writeSector(sectNum*2-1, ff);
        REQUIRE(rc == 0);
        rc = di.readSector(sectNum*2-1, buf7);
        REQUIRE(rc == 0);
        REQUIRE(std::equal(ff.begin(), ff.end(), buf7.begin()) == true);
        // empty
        rc = di.writeSector(sectNum*2 - 1, empty); //512*2
        REQUIRE(rc == 0);
        rc = di.readSector(sectNum*2 - 1, buf8);
        REQUIRE(rc == 0);
        REQUIRE(std::equal(empty.begin(), empty.end(), buf8.begin()) == true);

        //zero case
        //ff
        rc = di.writeSector(0, ff);
        REQUIRE(rc == 0);
        rc = di.readSector(0, buf9);
        REQUIRE(rc == 0);
        REQUIRE(std::equal(ff.begin(), ff.end(), buf7.begin()) == true);
        // empty
        rc = di.writeSector(0, empty); //512*2
        REQUIRE(rc == 0);
        rc = di.readSector(0, buf10);
        REQUIRE(rc == 0);
        REQUIRE(std::equal(empty.begin(), empty.end(), buf8.begin()) == true);
    };
}

// todo: at this time this is not necessary
int setup(int argc, const char **argv)
{
    if (rc) {
        return rc;
    }

    DiskImage di("./test.img");

    //fill vector
    std::vector<char> ff(512, 0xff);

    //empty vector
    std::vector<char> empty(512, 0x00);

    // WRITE SECTOR
    rc = di.writeSector(512, ff);

    if (rc) {
        return rc;
    }

    // READ SECTOR
    rc = di.readSector(512, empty);
    if (rc) {
        return rc;
    }

    return 0;
}


