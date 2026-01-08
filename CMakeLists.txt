cmake_minimum_required(VERSION 3.13)
project(WiiUCamera)

# Configuración WUT
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/wut/share/wut.toolchain.cmake")
endif()
include("$ENV{DEVKITPRO}/wut/share/wut.cmake")

file(GLOB_RECURSE SOURCES source/*.cpp source/*.h)

add_executable(WiiUCamera ${SOURCES})

# --- 1. BUSCAR PAQUETES ---
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_mixer REQUIRED)

# --- 2. INCLUIR CABECERAS ---
target_include_directories(WiiUCamera PUBLIC 
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
    $ENV{DEVKITPRO}/portlibs/wiiu/include 
)

# --- 3. VINCULACIÓN MANUAL LIGERA ---
# Quitamos vorbis, opus y flac porque no los tienes y (esperemos) no los necesitamos.
# Mantenemos mpg123 y modplug porque esos SI dieron error antes.

target_link_libraries(WiiUCamera wut
    # Librerías Principales
    -lSDL2_mixer
    -lSDL2_ttf
    -lSDL2_image
    -lSDL2

    # Dependencias de Audio CRÍTICAS (Solo las que fallaron antes)
    -lmpg123      
    -lmodplug     
    -logg         
    
    # Dependencias de Fuentes e Imagen
    -lharfbuzz
    -lfreetype
    -lpng
    -ljpeg
    -lz
    -lm
)

wut_create_rpx(WiiUCamera WiiUCamera.rpx)
wut_create_wuhb(WiiUCamera WiiUCamera.wuhb)