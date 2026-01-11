cmake_minimum_required(VERSION 3.13)

project(WiiUCamera C CXX ASM)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/cmake/WiiU.cmake")
endif()

# --- PARCHE: Cargar wut y whb correctamente ---
set(WUT_SHARE_PATH "$ENV{DEVKITPRO}/wut/share/wut.cmake")
if(EXISTS "${WUT_SHARE_PATH}")
    include("${WUT_SHARE_PATH}")
else()
    message(FATAL_ERROR "No se encontro wut.cmake.")
endif()

link_directories(
    "$ENV{DEVKITPRO}/wut/lib"
    "$ENV{DEVKITPRO}/portlibs/wiiu/lib"
    "$ENV{DEVKITPRO}/portlibs/ppc/lib"
)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(SDL2_ttf REQUIRED)

include_directories(
    source
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    "$ENV{DEVKITPRO}/portlibs/wiiu/include"
    "$ENV{DEVKITPRO}/portlibs/ppc/include"
)

file(GLOB_RECURSE SOURCES "source/*.cpp" "source/*.c")

add_executable(WiiUCamera ${SOURCES})

target_link_libraries(WiiUCamera 
    SDL2_image SDL2_mixer SDL2_ttf
    SDL2::SDL2
    freetype harfbuzz bz2 png jpeg
    mpg123 modplug vorbisidec ogg
    wut m z
)

wut_create_rpx(WiiUCamera)

wut_create_wuhb(WiiUCamera
    NAME "WiiUCamera"
    AUTHOR "ClaudiWolf2056"
    CONTENT "fs" 
    ICON "fs/icon.png" 
)