project (backsynth)

SET(CMAKE_CXX_FLAGS "-Wall -g -O2 -std=c++17 -I . -Wno-return-type-c-linkage")

add_executable(musil musil.cpp musil.h)
target_link_libraries (musil ${LIBS})
