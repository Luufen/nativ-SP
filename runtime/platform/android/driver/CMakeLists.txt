cmake_minimum_required(VERSION 3.14)
project(nativ_vulkan LANGUAGES C CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory("${ADRENOTOOLS_SOURCE}" adrenotools)

add_library(nativ_vulkan SHARED vulkan.c)
set_target_properties(nativ_vulkan PROPERTIES C_VISIBILITY_PRESET hidden)
target_link_libraries(nativ_vulkan PRIVATE adrenotools log dl)
