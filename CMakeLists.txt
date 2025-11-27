cmake_minimum_required(VERSION 3.16)

project(musil LANGUAGES CXX)

# Global compile definitions (visible to all targets)
add_compile_definitions(VERSION="0.1")
add_compile_definitions(COPYRIGHT="2025")

# Options
option(BUILD_MUSIL_IDE "Build Musil FLTK-based IDE" OFF)

# Subdirectories
add_subdirectory(cli)

if(BUILD_MUSIL_IDE)
    add_subdirectory(ide)
endif()
