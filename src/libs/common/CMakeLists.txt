FILE(GLOB_RECURSE SRC_LIST ./*.cc ./*.c)

include(../../utils.cmake)

INCLUDE_DIRECTORIES (
    .
    ../mysql/include
    ../libs/hsql/include/
    ../../libs/dep
    ../../daemons
    ../stat
    ../core
    ../log4cplus/include/
    ../zlib/include64
    ../yaml-cpp/include/)

LINK_DIRECTORIES (
    ${PROJECT_SOURCE_DIR}/src/libs/mysql/libs
    ${PROJECT_SOURCE_DIR}/src/libs/zlib/lib/libz64.a
    ${PROJECT_SOURCE_DIR}/src/libs/log4cplus/libs
    ${PROJECT_SOURCE_DIR}/src/libs/hsql/libs
    ${PROJECT_SOURCE_DIR}/src/libs/yaml-cpp/libs
)

LINK_LIBRARIES(liblog4cplus.a) 
LINK_LIBRARIES(libyaml-cpp.a)
LINK_LIBRARIES(libsqlparser.a)
LINK_LIBRARIES(libz64.a)
LINK_LIBRARIES(pthread)
LINK_LIBRARIES(libmysqlclient.a)

ADD_DEFINITIONS ("-g -fPIC -fpermissive -std=gnu++11")
ADD_DEFINITIONS(-Wno-builtin-macro-redefined)

ADD_LIBRARY (common ${SRC_LIST})

TARGET_LINK_LIBRARIES(common liblog4cplus.a libyaml-cpp.a libsqlparser.a libz64.a libmysqlclient.a)
redefine_file_macro(common)