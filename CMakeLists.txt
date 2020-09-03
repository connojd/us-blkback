project(us-blkback CXX)
cmake_minimum_required(VERSION 3.12)

if(NOT WITH_WIN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=gnu++11 -Wall")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /D__x86_64__")
endif()

message(STATUS)
message(STATUS "${PROJECT_NAME} Configuration:")
message(STATUS "CMAKE_BUILD_TYPE              = ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX          = ${CMAKE_INSTALL_PREFIX}")
message(STATUS)
message(STATUS "XEN_PUBLIC_INCLUDE_PATH       = ${XEN_PUBLIC_INCLUDE_PATH}")
message(STATUS "LIBXENBE_INCLUDE_PATH         = ${LIBXENBE_INCLUDE_PATH}")
message(STATUS "LIBXENBE_LIB_PATH             = ${LIBXENBE_LIB_PATH}")
message(STATUS "XENIFACE_INCLUDE_PATH         = ${XENIFACE_INCLUDE_PATH}")
message(STATUS "XENIFACE_LIB_PATH             = ${XENIFACE_LIB_PATH}")
message(STATUS "XENBUS_INCLUDE_PATH           = ${XENBUS_INCLUDE_PATH}")
message(STATUS "XENBUS_LIB_PATH               = ${XENBUS_LIB_PATH}")
message(STATUS "XENVBD_INCLUDE_PATH           = ${XENVBD_INCLUDE_PATH}")
message(STATUS)

################################################################################
# Includes
################################################################################

include_directories(${XEN_PUBLIC_INCLUDE_PATH})
include_directories(${LIBXENBE_INCLUDE_PATH})
include_directories(${XENIFACE_INCLUDE_PATH})
include_directories(${XENBUS_INCLUDE_PATH})
include_directories(${XENVBD_INCLUDE_PATH})

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
# Libraries
################################################################################
link_directories(${XENIFACE_LIB_PATH})
link_directories(${XENBUS_LIB_PATH})
link_directories(${LIBXENBE_LIB_PATH})

################################################################################
# Targets
################################################################################
add_executable(us-blkback ${BLKBACK_SOURCES})
add_executable(disk-image-util ${DISK_IMAGE_UTIL_SOURCES})
add_executable(disk-image-test ${DISK_IMAGE_TEST_SOURCES})

#set_target_properties(us-blkback PROPERTIES
#            CXX_STANDARD 17
#            CXX_EXTENSIONS OFF
#            )

if(WITH_WIN)
target_link_libraries(us-blkback
  xenbe
  xencontrol
)
else()
target_link_libraries(us-blkback
  xenbe
  xenstore
  xenevtchn
  xengnttab
  xenctrl
  pthread
)
endif()
