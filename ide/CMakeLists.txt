# ide/CMakeLists.txt
#
# Optional FLTK-based Musil IDE.

if(NOT BUILD_MUSIL_IDE)
    message(STATUS "BUILD_MUSIL_IDE=OFF, skipping Musil IDE (GUI)")
    return()
endif()

find_package(FLTK REQUIRED)
if(NOT FLTK_FOUND)
    message(FATAL_ERROR "FLTK not found, cannot build Musil IDE")
endif()

# Paths (relative to this CMakeLists.txt)
set(MUSIL_STDLIB_SCM "${CMAKE_SOURCE_DIR}/src/stdlib.scm")
# musil.icns is in the same folder as musil_ide.cpp
set(MUSIL_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/musil.icns")

if(APPLE)
    # Bundle executable
    add_executable(musil_ide MACOSX_BUNDLE
        musil_ide.cpp
        ${MUSIL_ICON_FILE}
    )

    # Put the icon in Contents/Resources
    set_source_files_properties(${MUSIL_ICON_FILE}
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )

    set_target_properties(musil_ide PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER       "com.carminecella.musil.ide"
        MACOSX_BUNDLE_BUNDLE_NAME          "Musil IDE"
        MACOSX_BUNDLE_INFO_STRING          "Musil IDE"
        MACOSX_BUNDLE_BUNDLE_VERSION       "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
        MACOSX_BUNDLE_ICON_FILE            "musil.icns"  # filename only
    )

    # Copy stdlib.scm into bundle Resources
    set(MUSIL_RESOURCES_DIR "$<TARGET_BUNDLE_DIR:musil_ide>/Contents/Resources")
    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MUSIL_RESOURCES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MUSIL_STDLIB_SCM}"
            "${MUSIL_RESOURCES_DIR}/stdlib.scm"
        COMMENT "Copying stdlib.scm into Musil IDE.app Resources"
    )

else()
    # Non-macOS build: normal executable
    add_executable(musil_ide
        musil_ide.cpp
    )

    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MUSIL_STDLIB_SCM}"
            "$<TARGET_FILE_DIR:musil_ide>/stdlib.scm"
        COMMENT "Copying stdlib.scm next to musil_ide executable"
    )
endif()

target_include_directories(musil_ide
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${FLTK_INCLUDE_DIR}
)

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

target_link_libraries(musil_ide PRIVATE ${FLTK_LIBRARIES})

install(TARGETS musil_ide
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)
