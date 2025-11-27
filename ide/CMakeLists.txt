# ide/CMakeLists.txt
#
# Optional FLTK-based Musil IDE.
# Musil is header-only: we include ../src and link against FLTK (+ threads).

# This subdir is only added if BUILD_MUSIL_IDE=ON at the top level,
# so we don't redefine the option here.

if(NOT BUILD_MUSIL_IDE)
    message(STATUS "BUILD_MUSIL_IDE=OFF, skipping Musil IDE (GUI)")
    return()
endif()

# Find FLTK
find_package(FLTK REQUIRED)

if(NOT FLTK_FOUND)
    message(FATAL_ERROR "FLTK not found, cannot build Musil IDE")
endif()

# IDE executable
add_executable(musil_ide
    musil_ide.cpp
)

# Include dirs: core headers and FLTK
target_include_directories(musil_ide
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${FLTK_INCLUDE_DIR}
)

# C++ standard and warnings/optimizations
target_compile_features(musil_ide PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(musil_ide PRIVATE /W4)
else()
    target_compile_options(musil_ide PRIVATE
        -Wall
        -g
        -O2
        -Wunused-function
        -Wno-return-type-c-linkage
    )
endif()

# Link FLTK (and optionally threads, if you use them)
# If you also need pthreads/std::thread, uncomment the Threads section.
#
# find_package(Threads REQUIRED)
# target_link_libraries(musil_ide PRIVATE ${FLTK_LIBRARIES} Threads::Threads)

target_link_libraries(musil_ide PRIVATE ${FLTK_LIBRARIES})

#
# macOS bundle configuration + stdlib.scm handling
#

if(APPLE)
    # Turn the IDE into a bundle application
    set_target_properties(musil_ide PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Musil IDE"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.carminecella.musil.ide"
        MACOSX_BUNDLE_INFO_STRING "Musil IDE"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    )

    # Path to Resources directory inside the bundle
    set(MUSIL_RESOURCES_DIR "$<TARGET_BUNDLE_DIR:musil_ide>/Contents/Resources")

    # Copy stdlib.scm into the bundle Resources after build
    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MUSIL_RESOURCES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/stdlib.scm"
            "${MUSIL_RESOURCES_DIR}/stdlib.scm"
        COMMENT "Copying stdlib.scm into Musil IDE.app Resources"
    )

else()
    # On Linux/Windows (non-bundle), copy stdlib.scm next to the executable
    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/stdlib.scm"
            "$<TARGET_FILE_DIR:musil_ide>/stdlib.scm"
        COMMENT "Copying stdlib.scm next to musil_ide executable"
    )
endif()

#
# Installation (optional)
#
# You can install the IDE bundle/binary as well; for now we just do:
#
install(TARGETS musil_ide
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)
