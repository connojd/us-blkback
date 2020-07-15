project(win-blkback)
cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=gnu++11 -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

################################################################################
# Includes
################################################################################

#include_directories(.)

################################################################################
# Sources
################################################################################
#	
#
set(BLKBACK_SOURCES
  BlkBackend.cpp
  DiskImage.cpp
  )

set(DISK_IMAGE_UTIL_SOURCES
  disk-image-util.cpp
  DiskImage.cpp
  )

set(DISK_IMAGE_TEST_SOURCES
  disk-image-test.cpp
  DiskImage.cpp
  )

################################################################################
# Targets
################################################################################
add_executable(blk-back ${BLKBACK_SOURCES})
add_executable(disk-image-util ${DISK_IMAGE_UTIL_SOURCES})
add_executable(disk-image-test ${DISK_IMAGE_TEST_SOURCES})


#set_target_properties(blk-back PROPERTIES
#            CXX_STANDARD 17
#            CXX_EXTENSIONS OFF
#            )

################################################################################
# Libraries
################################################################################

target_link_libraries(blk-back
	xenbe
)
