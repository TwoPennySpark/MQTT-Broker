cmake_minimum_required(VERSION 3.5)
project(client)

# Default to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Boost 1.79.0 REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(
  src/include
  ../../src/include/
  ../../src/include/NetCommon
)

set(SOURCES ../../src/mqtt.cpp src/client.cpp)

add_executable(${PROJECT_NAME} src/main.cpp ${SOURCES})

target_link_libraries(${PROJECT_NAME} pthread ${Boost_LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/../install)
