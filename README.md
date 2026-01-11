# 📸 WiiUCamera v1.1.8 (Beta)

[English](#english) | [Español](#español)

---

<a name="english"></a>
## 🇺🇸 English

A Nintendo Wii U application that transforms your GamePad into a functional camera with recording capabilities, filters, and a gallery.

### ✨ Main Features
* **📷 Photo Mode:** Take photographs and save them to the SD card.
* **🎥 Video Mode:** Record video clips in AVI format (m-jpeg).
* **✨ Effects:** Apply real-time filters to the camera feed.
* **🖼️ Integrated Gallery:** View your photos and videos without leaving the application.
* **🎵 Music & SFX:** Interface with background audio and sound effects.
* **🇺🇸/🇪🇸 Multi-language:** Support for English and Spanish (changeable via the menu).

### 📥 Installation (For Users)
1.  Download the latest **`WiiUCamera.wuhb`** file.
2.  Copy the file to your SD card in the following path:
    `SD:/wiiu/apps/WiiUCamera/WiiUCamera.wuhb`
3.  Insert the SD card into your console and launch the application from the Aroma menu.

> **Note:** Photos and videos will be saved automatically to: `SD:/wiiu/apps/WiiUCamera Files/`

### 🎮 Controls

| Button | Action |
| :--- | :--- |
| **D-Pad / Left Stick** | Navigate the menu |
| **A Button** | Select / Take Photo / Start Recording |
| **B Button** | Go Back / Stop Recording |
| **Touch Screen** | Full interface navigation |
| **HOME Button** | Exit application (Clean close) |

### 🛠️ Compilation (For Developers)
If you wish to compile this project from the source code, follow these strict instructions to avoid dependency errors or missing assets.

#### 1. Prerequisites
You must have **devkitPro** installed with the Wii U environment configured.

#### 2. Dependency Installation
This project uses SDL2 and various support libraries (images, fonts, audio). Run the following command in your MSYS2/Pacman terminal to install everything needed:

```bash
pacman -S wiiu-dev wut-tools wut ppc-sdl2 ppc-sdl2_image ppc-sdl2_mixer ppc-sdl2_ttf ppc-freetype ppc-libpng ppc-libjpeg-turbo ppc-mpg123 ppc-libmodplug ppc-libvorbisidec ppc-zlib ppc-bzip2
```
#### 3. Project Structure
For CMakeLists.txt to work and package resources correctly, your folder must look exactly like this:

```WiiUCamera/
├── CMakeLists.txt       # Build configuration
├── README.md
├── source/              # Source Code
│   ├── main.cpp
│   ├── camara.h
│   ├── camara.cpp
│   ├── recorder.h
│   ├── etc...
└── fs/                  # Resource Files (Assets)
    ├── font.ttf         # (Must be named font.ttf, NOT Font.TTF)
    ├── background.png
    ├── button_iniciar.png
    ├── music.mp3
    ├── move.wav
    ├── icon.png         # Icon for the Wii U menu
    └── ... (rest of the images)
```

#### 4. Build Commands
Open your terminal in the project folder and run:

```bash
mkdir build
cd build
source /etc/profile.d/devkit-env.sh
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake ../
make
```

Upon completion, you will get a WiiUCamera.wuhb file ready to use.

📝 Changelog (v1.1.8)
CRITICAL FIX: Solved the freeze issue when closing the application. Now uses libwhb logic (WHBProcInit/Shutdown) for a clean and instant exit.
IMPROVEMENT: Resources (images/fonts) are now automatically packed inside the .wuhb file. Manual folder copying to SD is no longer necessary.
FIX: Corrected inverted video orientation in the gallery (still has blue filter).
NEW: Instructions added to camera modes.

🤝 Credits and Acknowledgements
Main Developer: ClaudiWolf2056

Libraries: devkitPro, WUT, SDL2 for Wii U.

🌟 Special thanks to whateveritwas for:
Providing the crucial code for the exit logic.
(You can find them and follow their projects on GitHub).

Made with ❤️ for users who still use Wii U.

<a name="español"></a>
## 🇪🇸 Español
Una aplicación para Nintendo Wii U que transforma tu GamePad en una cámara funcional con capacidad de grabación, filtros y galería.

### ✨ Características principales
* **📷 Modo Foto:** Toma fotos y guárdalas en la tarjeta SD.
* **🎥 Modo Vídeo:** Graba videoclips en formato AVI (m-jpeg).
* **✨ Efectos:** Aplica filtros en tiempo real a la señal de la cámara.
* **🖼️ Galería integrada:** Ve tus fotos y vídeos sin salir de la aplicación.
* **🎵 Música y efectos de sonido:** Interfaz con audio de fondo y efectos de sonido.
* **🇪🇸/🇪🇸 Multiidioma:** Disponible en inglés y español (cambiable desde el menú).

### 📥 Instalación (Para usuarios)
1. Descarga la última versión del archivo **`WiiUCamera.wuhb`**.
2. Copia el archivo a tu tarjeta SD en la siguiente ruta:
`SD:/wiiu/apps/WiiUCamera/WiiUCamera.wuhb`
3. Inserta la tarjeta SD en tu consola e inicia la aplicación desde el menú Aroma

> **Nota:** Las fotos y los vídeos se guardarán automáticamente en: `SD:/wiiu/apps/WiiUCamera Files/`

| Botón | Acción |
| :--- | :--- |
| **Cruce de dirección / Joystick izquierdo** | Navegar por el menú |
| **Botón A** | Seleccionar / Tomar foto / Iniciar grabación |
| **Botón B** | Retroceder / Detener grabación |
| **Pantalla táctil** | Navegación completa de la interfaz |
| **Botón INICIO** | Salir de la aplicación (Cierre completo) |

### 🛠️ Compilación (Para desarrolladores)
Si deseas compilar este proyecto desde el código fuente, sigue estas instrucciones estrictas para evitar errores de dependencia o la falta de recursos.

#### 1. Requisitos previos
Debes tener **devkitPro** instalado y el entorno de Wii U configurado.

#### 2. Instalación de dependencias
Este proyecto utiliza SDL2 y varias bibliotecas de soporte (imágenes, fuentes, audio). Ejecuta el siguiente comando en tu terminal MSYS2/Pacman para instalar todo lo necesario:

```bash
pacman -S wiiu-dev wut-tools wut ppc-sdl2 ppc-sdl2_image ppc-sdl2_mixer ppc-sdl2_ttf ppc-freetype ppc-libpng ppc-libjpeg-turbo ppc-mpg123 ppc-libmodplug ppc-libvorbisidec ppc-zlib ppc-bzip2
```
#### 3. Estructura del proyecto
Para que CMakeLists.txt funcione y empaquete los recursos correctamente, tu carpeta debe tener este aspecto:

```WiiUCamera/
├── CMakeLists.txt # Configuración de compilación
├── README.md
├── source/ # Código fuente
│ ├── main.cpp
│ ├── camara.h
│ ├── camara.cpp
│ ├── recorder.h
│ ├── etc...
└── fs/ # Archivos de recursos (Recursos)
├── font.ttf # (Debe llamarse font.ttf, NO Font.TTF)
├── background.png
├── button_iniciar.png
├── music.mp3
├── move.wav
├── icon.png # Icono del menú de Wii U
└── ... (resto del imágenes)
```

#### 4. Comandos de compilación
Abre tu terminal en la carpeta del proyecto y ejecuta:

```bash
mkdir build
cd build
source /etc/profile.d/devkit-env.sh
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake ../
make
```

Al finalizar, obtendrás el archivo WiiUCamera.wuhb listo para usar.

📝 Registro de cambios (v1.1.8)
CORRECCIÓN CRÍTICA: Se solucionó el problema de congelamiento al cerrar la aplicación. Ahora usa la lógica de libwhb (WHBProcInit/Shutdown) para una salida limpia e instantánea.
MEJORA: Los recursos (imágenes/fuentes) ahora se empaquetan automáticamente dentro del archivo .wuhb. Ya no es necesario copiar manualmente la carpeta a la tarjeta SD. SOLUCIÓN: Se corrigió la orientación invertida del video en la galería (aún tiene filtro azul).
NUEVO: Se añadieron instrucciones a los modos de cámara.

🤝 Créditos y agradecimientos
Desarrollador principal: ClaudiWolf2056

Bibliotecas: devkitPro, WUT, SDL2 para Wii U.

🌟 Agradecimientos especiales a whateveritwas por:
Proporcionar el código crucial para la lógica de salida.
(Puedes encontrarlos y seguir sus proyectos en GitHub).

Hecho con ❤️ para los usuarios que aún usan Wii U.
