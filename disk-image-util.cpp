#include "DiskImage.h"
#include <fstream>
#include <string>
#include <cstdlib>
#include <sys/stat.h>

void usage()
{
  std::cout << "disk-image-util <filename> <sector-count> <sector-size>\n";
}

int main(int argc, const char **argv)
{
  unsigned long sector_count = 0;
  unsigned long sector_size = 0;
  if (argc != 4) {
    usage();
    return -1;
  }

  sector_count = strtoul(argv[2], NULL, 0);
  sector_size = strtoul(argv[3], NULL, 0);    
  return DiskImage::createBackingFile(argv[1], sector_count, sector_size);
}


