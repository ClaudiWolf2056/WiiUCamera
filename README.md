# 📷 WiiUCamera (Homebrew WiiU App)

**WiiUCamera** is a native homebrew application for the **Nintendo Wii U** that allows access to and processing of the GamePad camera's video signal. It was developed from scratch in C++ using the WUT (Wii U Tools) toolchain.

**WiiUCamera** es una aplicación nativa de homebrew para la **Nintendo Wii U** que permite acceder y procesar la señal de video de la cámara del GamePad. Desarrollada desde cero en C++ utilizando el toolchain WUT (Wii U Tools).

> **Estado:** 🚧 **Desarrollo / Alpha**
> Actualmente: Streaming estable en blanco y negro con gestión manual de caché.

---

## 🌎 Language / Idioma
* [English](#english)
* [Español](#español)

---

<a name="english"></a>
## 🇺🇸 English

### 🎯 Objective
This project explores the computer vision capabilities of the Wii U GamePad. The main goal is to process the raw YUV camera signal, clean it using low-level CPU instructions, and render it using SDL2.

### 🛠️ Tech Stack
* **Language:** C++
* **Libraries:** `wut` (Wii U Tools), `SDL2`, `coreinit`, `vpad`, `camera`.

### ✨ Features
* **Memory Alignment:** Proper buffer allocation (256-byte alignment) for the Wii U hardware.
* **Cache Invalidation:** Implementation of `DCInvalidateRange` to prevent visual artifacts from the CPU cache.
* **Direct Access:** Reading the raw camera buffer directly from memory.

---

<a name="español"></a>
## 🇪🇸 Español

### 🎯 Objetivo
Este proyecto explora las capacidades de visión artificial del GamePad de Wii U. El objetivo principal es procesar la señal de cámara YUV pura, limpiarla mediante instrucciones de CPU de bajo nivel y renderizarla usando SDL2.

### 🛠️ Stack Técnico
* **Lenguaje:** C++
* **Librerías:** `wut` (Wii U Tools), `SDL2`, `coreinit`, `vpad`, `camera`.

### ✨ Características Actuales
* **Alineación de Memoria:** Reserva de buffers con alineación de 256 bytes, requerida por el hardware de Wii U.
* **Invalidación de Caché:** Implementación de `DCInvalidateRange` para evitar artefactos visuales causados por la caché de la CPU.
* **Acceso Directo:** Lectura del buffer de la cámara directamente desde la RAM.

---

## 🚀 Roadmap / Próximos Pasos
* [ ] **Color:** Implementar algoritmos de conversión YUV a RGB.
* [ ] **Filters:** Añadir filtros en tiempo real (Negativo, Detección de bordes, Sepia).
* [ ] **Storage:** Guardar capturas de pantalla en la tarjeta SD.

## 👨‍💻 Author / Autor
**Claudio** - *Estudiante de Ingeniería Mecatrónica @ UTEC*

## 📄 License / Licencia
MIT License - see the [LICENSE](LICENSE) file for details.
