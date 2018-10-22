cmake_minimum_required(VERSION 3.5)
project(telnet C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -static")

set(SOURCE_FILES main.c)
set(HEADER_FILES )

add_executable(telnet ${HEADER_FILES} ${SOURCE_FILES})

if(WIN32)

    target_link_libraries(telnet wsock32 ws2_32)

elseif(UNIX)

    target_link_libraries(telnet pthread)

endif()
