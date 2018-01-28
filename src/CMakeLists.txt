include_directories(${PROJECT_SOURCE_DIR}/external/redox/include ${PROJECT_SOURCE_DIR}/src)

set(lib${PROJECT_NAME_LOWER}_sources redis_plugin.cc util.cc)

add_library(lib${PROJECT_NAME_LOWER} ${lib${PROJECT_NAME_LOWER}_sources})

add_executable(${PROJECT_NAME_LOWER} main.cc)
set(${PROJECT_NAME_LOWER}_LIBRARIES lib${PROJECT_NAME_LOWER} ${CVMFS_CACHE_LIBRARIES})
if (NOT APPLE)
    list(APPEND ${PROJECT_NAME_LOWER}_LIBRARIES ${OPENSSL_LIBRARIES})
endif (NOT APPLE)
target_link_libraries(${PROJECT_NAME_LOWER} lib${PROJECT_NAME_LOWER} ${CVMFS_CACHE_LIBRARIES} ${OPENSSL_LIBRARIES} redox_static)
