cmake_minimum_required(VERSION 3.16)

project(grafcik VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(grafcik INTERFACE)
target_include_directories(grafcik INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(grafcik INTERFACE cxx_std_23)
add_library(grafcik::Grafcik ALIAS grafcik)
