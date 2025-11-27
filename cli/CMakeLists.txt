# cli/CMakeLists.txt
#
# Command-line Musil interpreter.
# Uses musil.cpp and the header-only core in ../src.

# No cmake_minimum_required() or project() here:
# they are defined at the top level.

add_executable(musil
    musil.cpp
)

# Include the core headers
target_include_directories(musil
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# C++ standard and warnings/optimizations (per-target, not global)
target_compile_features(musil PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(musil PRIVATE /W4)
else()
    target_compile_options(musil PRIVATE
        -Wall
        -g
        -O2
        -Wunused-function
        -Wno-return-type-c-linkage
    )
endif()

# If you need extra libs for the CLI, add them here, e.g.:
# target_link_libraries(musil PRIVATE some_lib)

#
# Installation
#
# 1) Install the executable under "bin" in the install prefix.
# 2) Install stdlib.scm under a data directory inside the prefix
#    (e.g. ${CMAKE_INSTALL_PREFIX}/share/musil).
#

# Install CLI binary under ${CMAKE_INSTALL_PREFIX}/bin
install(TARGETS musil
    RUNTIME DESTINATION bin
)

# Install stdlib.scm under ${CMAKE_INSTALL_PREFIX}/share/musil
install(FILES ${CMAKE_SOURCE_DIR}/src/stdlib.scm
    DESTINATION share/musil
)
