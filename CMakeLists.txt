cmake_minimum_required(VERSION 3.13)

# --- 1. TOOLCHAIN ---
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{DEVKITPRO})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/cmake/WiiU.cmake")
    else()
        message(FATAL_ERROR "Please set DEVKITPRO in your environment.")
    endif()
endif()

project(WiiUCamera C CXX)
include(${CMAKE_TOOLCHAIN_FILE})

set(CMAKE_CXX_STANDARD 17)

# --- 2. RUTAS ---
include_directories(
    source
    $ENV{DEVKITPRO}/wut/include
    $ENV{DEVKITPRO}/portlibs/wiiu/include
)

link_directories(
    $ENV{DEVKITPRO}/wut/lib
    $ENV{DEVKITPRO}/portlibs/wiiu/lib
)

# --- 3. FUENTES ---
file(GLOB_RECURSE SOURCE_FILES "source/*.cpp" "source/*.h")

# --- 4. EJECUTABLE (NOMBRE LIMPIO) ---
# Antes era WiiUCamera.elf, ahora es solo WiiUCamera
add_executable(WiiUCamera ${SOURCE_FILES})

# --- 5. LIBRERIAS ---
target_link_libraries(WiiUCamera
    # Capa Superior (SDL)
    -lSDL2_ttf
    -lSDL2_mixer
    -lSDL2_image
    -lSDL2
    
    # Dependencias de TTF/Imagen
    -lfreetype
    -lharfbuzz
    -lbz2
    -lpng
    -ljpeg
    -lz
    
    # Dependencias de Audio
    -lmpg123
    -lmodplug
    -lvorbisidec
    -logg
    
    # Sistema
    -lm
    -lwut 
)

# --- 6. SALIDA ---
wut_create_rpx(WiiUCamera)
wut_create_wuhb(WiiUCamera
    CONTENT content
    NAME "WiiUCamera"
    AUTHOR "ClaudiWolf2056"
    ICON content/icon.png
)