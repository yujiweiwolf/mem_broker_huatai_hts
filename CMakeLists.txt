# cmake_minimum_required(VERSION <specify CMake version here>)
cmake_minimum_required(VERSION 3.19)
project(mem_huatai_hts_broker)

#Release Debug
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall")
add_compile_options(-std=c++17 -Wall -O2)

SET(ROOT_DIR "/home/work/sys/lib")
macro(SUBDIRLIST result curdir)
    file(GLOB children RELATIVE ${curdir} ${curdir}/*)
    set(dirlist "")
    foreach(child ${children})
        if(IS_DIRECTORY ${curdir}/${child})
            LIST(APPEND dirlist ${child})
        endif()
    endforeach()
    set(${result} ${dirlist})
endmacro()

SUBDIRLIST(all_sub_index ${ROOT_DIR})
foreach(child ${all_sub_index})
    list(APPEND all_include ${ROOT_DIR}/${child}/include)
    list(APPEND all_lib ${ROOT_DIR}/${child}/lib)
endforeach()

include_directories(${CMAKE_CURRENT_LIST_DIR}/lib/itpdk5.2.40.0/linux/include /home/work/sys/develop/libmembroker/src ${all_include})
link_directories(${CMAKE_CURRENT_LIST_DIR}/lib/itpdk5.2.40.0/linux/lib  /home/work/sys/develop/libmembroker/cmake-build-debug ${all_lib})

aux_source_directory (./src/libbroker_hts LIB_LIST)
add_library (broker_huatai_hts ${LIB_LIST})

SET(BROKER "mem_dongwu_hts_broker")
SET(BROKER_TEST "test")

# 可执行文件
add_executable(${BROKER} src/hts_broker/main.cc)
target_link_libraries(${BROKER}
        broker_huatai_hts membroker fixapi secitpdk coral swordfish x stdc++fs yaml-cpp  clickhouse-cpp-lib-static boost_date_time boost_filesystem boost_regex boost_system  boost_chrono boost_log boost_program_options boost_thread boost_iostreams z protobuf protobuf-lite sodium zmq ssl crypto iconv pthread dl)

# 测试文件
add_executable(${BROKER_TEST} src/test_broker_hts/test_broker_hts.cc)
target_link_libraries(${BROKER_TEST}
        broker_huatai_hts membroker fixapi secitpdk coral swordfish x stdc++fs yaml-cpp clickhouse-cpp-lib-static boost_date_time boost_filesystem boost_regex boost_system  boost_chrono boost_log boost_program_options boost_thread boost_iostreams z protobuf protobuf-lite sodium zmq ssl crypto iconv pthread dl)

FILE(COPY Dockerfile image.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB LIBS_NAME "lib/itpdk5.2.40.0/linux/lib/*so*")
FILE(COPY ${LIBS_NAME} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB COMMON_LIB_NAME "${ROOT_DIR}/common_lib/lib/*so*")
FILE(COPY ${COMMON_LIB_NAME} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/image.sh "n=$1; v=`./$n -v`; ldd $n | grep /sys/lib/ | awk '{print $3}' | xargs -i cp -L -n {} .; docker build -t $n:$v .; rm -rf \${n}_$v.docker.tar.gz; docker save $n:$v > \${n}_$v.docker.tar && gzip \${n}_$v.docker.tar")
add_custom_target(image COMMAND sh image.sh ${BROKER} DEPENDS ${BROKER})
