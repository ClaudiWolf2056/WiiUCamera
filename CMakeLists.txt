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

# --- 4. EJECUTABLE ---
add_executable(WiiUCamera ${SOURCE_FILES})

# --- 5. LIBRERIAS (ORDEN ESTRICTO CORREGIDO) ---
target_link_libraries(WiiUCamera
    # 1. Capas superiores (Las que piden ayuda)
    SDL2_ttf
    SDL2_image
    SDL2_mixer
    SDL2
    
    # 2. Fuentes y procesamiento de texto
    freetype
    harfbuzz
    
    # 3. Formatos de compresión y bases (A las que les piden ayuda)
    webp
    jpeg
    png
    brotlidec
    brotlicommon
    bz2
    z
    
    # 4. Audio
    mpg123
    modplug
    vorbisidec
    ogg
    
    # 5. Sistema base
    m
    wut 
)

# --- 6. SALIDA ----
wut_create_rpx(WiiUCamera)
wut_create_wuhb(WiiUCamera
    CONTENT content
    NAME "WiiUCamera"
    AUTHOR "ClaudiWolf2056"
    ICON content/icon.png
)