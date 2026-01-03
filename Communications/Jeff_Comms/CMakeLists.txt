cmake_minimum_required(VERSION 3.20)
project(mhab_comms_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sim_main
    src/sim/sim_main.cpp
)

target_include_directories(sim_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(unofficial-sodium CONFIG REQUIRED)
target_link_libraries(sim_main PRIVATE unofficial-sodium::sodium)

