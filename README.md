# WiiUCamera 📸

[English](#english) | [Español](#español)

---

<a name="english"></a>
## 🇬🇧 English

A Homebrew application for Nintendo Wii U that turns the GamePad into a photo camera and **video recorder (AVI)**.

### 🚀 What's New in v0.9.5 (Video Update)
We achieved the impossible! Video recording is now functional.

* **🎥 AVI Video Recording:** You can now record short clips (approx. 3.5 seconds).
* **✨ Color & Orientation Fix:** Videos are saved with correct colors (BGRA fix) and correct orientation.
* **📂 PC Compatible:** The `.avi` files work perfectly in VLC Media Player on Windows/Linux.
* **🖥️ UI Improvements:** Better text placement and recording indicators.
* **✅ Stability:** Fixed crashes when saving large files to the SD card.

### ⚠️ Important Notes regarding Video
* **File Size:** Video is uncompressed (High Quality). A 3.5s clip is around **115 MB**.
* **Processing Time:** When you stop recording, **please wait**. The console needs a few seconds to write the RAM buffer to the SD card.
* **Duration:** Currently limited to 100 frames (~3.5 seconds) to prevent RAM overflow.

### 🎮 Controls
* **D-Pad / Left Stick:** Navigate menu.
* **A Button:** Select / Record Video / Take Photo.
* **B Button:** Back / Exit / Stop Recording.
* **Touch Screen:** Select options (Beta).

### 🛠️ Installation
1.  Download the latest `.wuhb` file from Releases.
2.  Copy it to your SD card in the `wiiu/apps/` folder.
3.  Launch it from the Homebrew Launcher (Aroma/Tiramisu).

---

<a name="español"></a>
## 🇪🇸 Español

Una aplicación Homebrew para Nintendo Wii U que permite usar el GamePad como cámara de fotos y **grabadora de video (AVI)**.

### 🚀 Novedades v0.9.5 (Actualización de Video)
¡Logramos lo imposible! La grabación de video ya es funcional.

* **🎥 Grabación de Video AVI:** Ahora puedes grabar clips cortos (aprox 3.5 segundos).
* **✨ Corrección de Color y Orientación:** Los videos se guardan con los colores reales (Fix BGRA) y derechos.
* **📂 Compatible con PC:** Los archivos `.avi` funcionan perfectamente en VLC Media Player en Windows/Linux.
* **🖥️ Mejoras de Interfaz:** Textos mejor ubicados e indicadores de grabación (REC).
* **✅ Estabilidad:** Se arreglaron los cuelgues al guardar archivos grandes en la SD.

### ⚠️ Notas Importantes sobre el Video
* **Peso del Archivo:** El video es sin compresión (Alta Calidad). Un clip de 3.5s pesa unos **115 MB**.
* **Tiempo de Procesado:** Al detener la grabación, **por favor espera**. La consola necesita unos segundos para pasar los datos de la RAM a la SD.
* **Duración:** Limitado actualmente a 100 frames (~3.5 segundos) para no saturar la memoria RAM.

### 🎮 Controles
* **D-Pad / Stick Izquierdo:** Navegar por el menú.
* **Botón A:** Seleccionar / Grabar Video / Tomar Foto.
* **Botón B:** Atrás / Salir / Detener Grabación.
* **Pantalla Táctil:** Seleccionar opciones (Beta).

### 🛠️ Instalación
1.  Descarga el archivo `.wuhb` más reciente.
2.  Cópialo a la tarjeta SD en la carpeta `wiiu/apps/`.
3.  Ejecuta desde el Homebrew Launcher (Aroma/Tiramisu).

---

## 👨‍💻 Credits / Créditos
Developed by **ClaudiWolf2056**.
Libraries used: SDL2, SDL2_mixer, SDL2_image, SDL2_ttf for Wii U.
