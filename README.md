# 📷 WiiUCamera (Homebrew WiiU App)

**WiiUCamera** is a native homebrew application for the **Nintendo Wii U** that transforms the GamePad into a functional photo camera. It was developed from scratch in C++ using the WUT (Wii U Tools) toolchain.

**WiiUCamera** es una aplicación nativa de homebrew para la **Nintendo Wii U** que transforma el GamePad en una cámara fotográfica funcional. Desarrollada desde cero en C++ utilizando el toolchain WUT (Wii U Tools).

> **Status / Estado:** 🚀 **Beta v0.5.0**
> **Current:** Color streaming (RGB), Graphic UI, Photo Capture (BMP).
> **Actual:** Streaming a color (RGB), Interfaz Gráfica, Captura de Fotos (BMP).

---

## 🌎 Language / Idioma
* [🇺🇸 English](#english)
* [🇪🇸 Español](#español)

---

<a name="english"></a>
## 🇺🇸 English

### 🎯 Objective
This project explores the computer vision capabilities of the Wii U GamePad. The main goal is to process the raw YUV camera signal, clean it using low-level CPU instructions, and render it using SDL2 with a user-friendly interface.

### ✨ Features (v0.5.0)
* **Real-Time Color View:** Smooth 30 FPS RGB rendering.
* **Graphical UI:** Grid-based menu with custom background and icons.
* **Photo Capture:** Takes snapshots in BMP format using L, R, or A buttons.
* **File Management:** Automatically saves images to `sd:/WiiUCamera Files/` without naming conflicts.
* **Visual Flash:** On-screen flash effect when a photo is taken.
* **Direct Memory Access:** Reads raw camera buffer directly from RAM for performance.

### 🎮 Controls
| Button | Action |
| :--- | :--- |
| **D-Pad / Left Stick** | Navigate Menu |
| **A** | Select / Take Photo |
| **B** | Return / Cancel |
| **L / R** | Take Photo (Shutter) |
| **HOME** | Exit (Experimental) |

### 🛠️ Tech Stack
* **Language:** C++
* **Libraries:** `wut` (Wii U Tools), `SDL2`, `coreinit`, `vpad`, `camera`.
* **Techniques:** Memory Alignment (256-byte), Cache Invalidation (`DCInvalidateRange`).

### 🚀 Roadmap
* [x] **Color:** Implement YUV to RGB conversion algorithms.
* [x] **UI:** Graphic User Interface with touch/button support.
* [x] **Storage:** Save screenshots to SD card (`.bmp`).
* [ ] **Video:** Video recording implementation (`.mp4` or `.avi`).
* [ ] **Filters:** Real-time filters (Sepia, Edge Detection).
* [ ] **Stability:** Fix Home Button resume/suspend loop (ProcUI).

---

<a name="español"></a>
## 🇪🇸 Español

### 🎯 Objetivo
Este proyecto explora las capacidades de visión artificial del GamePad de Wii U. El objetivo principal es procesar la señal de cámara YUV pura, limpiarla mediante instrucciones de CPU de bajo nivel y renderizarla usando SDL2 con una interfaz amigable.

### ✨ Características (v0.5.0)
* **Vista a Color en Tiempo Real:** Renderizado RGB fluido a 30 FPS.
* **Interfaz Gráfica:** Menú basado en cuadrícula con fondo e iconos personalizados.
* **Captura de Fotos:** Toma fotografías en formato BMP usando los botones L, R o A.
* **Gestión de Archivos:** Guarda automáticamente las imágenes en `sd:/WiiUCamera Files/` evitando conflictos de nombres.
* **Flash Visual:** Efecto de flash en pantalla al tomar la foto.
* **Acceso Directo:** Lectura del buffer de la cámara directamente desde la RAM para mayor rendimiento.

### 🎮 Controles
| Botón | Acción |
| :--- | :--- |
| **D-Pad / Stick Izq** | Navegar por el Menú |
| **A** | Seleccionar / Tomar Foto |
| **B** | Volver / Cancelar |
| **L / R** | Tomar Foto (Disparador) |
| **HOME** | Salir (Experimental) |

### 🛠️ Stack Técnico
* **Lenguaje:** C++
* **Librerías:** `wut` (Wii U Tools), `SDL2`, `coreinit`, `vpad`, `camera`.
* **Técnicas:** Alineación de memoria (256 bytes), Invalidación de Caché (`DCInvalidateRange`).

### 🚀 Roadmap / Próximos Pasos
* [x] **Color:** Implementar algoritmos de conversión YUV a RGB.
* [x] **UI:** Interfaz Gráfica de Usuario con soporte táctil/botones.
* [x] **Almacenamiento:** Guardar capturas en la tarjeta SD (`.bmp`).
* [ ] **Video:** Implementación de grabación de video (`.mp4` o `.avi`).
* [ ] **Filtros:** Añadir filtros en tiempo real (Sepia, Detección de bordes).
* [ ] **Estabilidad:** Arreglar el ciclo de suspensión/resumen con el botón HOME (ProcUI).

---

## 👨‍💻 Author / Autor
**ClaudiWolf2056**

## 📄 License / Licencia
GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.
