cmake_minimum_required(VERSION 3.18)

project(ump-keyboard)

set(CMAKE_CXX_STANDARD 23)

include(cmake/CPM.cmake)

CPMAddPackage(
        NAME           libremidi
        GIT_TAG        v5.2.0
        GIT_REPOSITORY https://github.com/celtera/libremidi
        EXCLUDE_FROM_ALL
)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

include(FetchContent)

FetchContent_Declare(cmidi2
        GIT_REPOSITORY https://github.com/atsushieno/cmidi2
        GIT_TAG        0.7
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(cmidi2)

FetchContent_Declare(midicci
        NAME            midicci
        GIT_TAG         0c8342c
        GIT_REPOSITORY  https://github.com/atsushieno/midicci
)
FetchContent_MakeAvailable(midicci)

#add_compile_options("-fsanitize=address")
#link_libraries("-fsanitize=address")
#add_link_options(-fPIC)

add_subdirectory(src/)
