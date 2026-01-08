cmake_minimum_required(VERSION 3.13)
project(WiiUCamera C CXX)

# 1. CARGAR WUT (LEGACY)
if(EXISTS "$ENV{DEVKITPRO}/wut/share/wut.cmake")
    include("$ENV{DEVKITPRO}/wut/share/wut.cmake")
elseif(EXISTS "/opt/devkitpro/wut/share/wut.cmake")
    include("/opt/devkitpro/wut/share/wut.cmake")
endif()

# 2. RUTAS DE INCLUSIÓN (HEADERS)
include_directories(
    include
    ${WUT_INCLUDE_DIRS}
    "/opt/devkitpro/portlibs/wiiu/include"
    "/opt/devkitpro/portlibs/wiiu/include/SDL2"
    "/opt/devkitpro/portlibs/wiiu/include/freetype2"
    "/opt/devkitpro/portlibs/ppc/include"             # <--- NUEVO: Cajón PowerPC
    "/opt/devkitpro/portlibs/ppc/include/harfbuzz"    # <--- NUEVO: Específico HarfBuzz
)

# 3. RUTAS DE LIBRERÍAS (ARCHIVOS .A)
link_directories(
    "/opt/devkitpro/portlibs/wiiu/lib"
    "/opt/devkitpro/portlibs/ppc/lib"                 # <--- NUEVO: Aquí vive libharfbuzz.a
)

# 4. ARCHIVOS FUENTE
file(GLOB_RECURSE SOURCES "source/*.c" "source/*.cpp")

# 5. CREAR EJECUTABLE
add_executable(WiiUCamera ${SOURCES})

# 6. VINCULAR LIBRERÍAS
target_link_libraries(WiiUCamera
    SDL2_image
    SDL2_ttf
    harfbuzz    # Ahora sí lo encontrará
    SDL2
    freetype
    png
    jpeg
    z
    m
)

# 7. GENERAR RPX
wut_create_rpx(WiiUCamera)