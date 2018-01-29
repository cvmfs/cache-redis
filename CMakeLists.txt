cmake_minimum_required (VERSION 2.8)

project(cache-redis)
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unsupported-friend")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(CVMFS_CACHE REQUIRED)
include_directories(${CVMFS_CACHE_INCLUDE_DIRS})

# We don't need to explicitly link OpenSSL on macOS, since the libcvmfs_cache.a
# static library already includes OpenSSL on this platform
if (NOT APPLE)
    find_package(OpenSSL REQUIRED)
    include_directories(${OPENSSL_INCLUDE_DIRS})
endif(NOT APPLE)

add_subdirectory(external/redox)

add_subdirectory(src)
add_subdirectory(test)
