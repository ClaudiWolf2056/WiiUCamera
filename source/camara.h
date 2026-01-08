#ifndef CAMARA_H
#define CAMARA_H

#include <SDL2/SDL.h>
#include <camera/camera.h>
#include <vpad/input.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h> 

#define CAM_ANCHO_REAL 640
#define CAM_ALTO       480
#define CAM_PITCH      768 
#define CLAMP(x) (((x) > 255) ? 255 : (((x) < 0) ? 0 : (x)))

static volatile bool frameListo = false;

static void MiCallbackDeCamara(CAMEventData *evento) {
    if (evento->eventType == CAMERA_DECODE_DONE) frameListo = true;
}

// --- GUARDADO SEGURO (USANDO RAM, NO TEXTURA) ---
void GuardarFotoEnSD(uint32_t* pixelesRGBA) {
    // 1. Carpeta Maestra
    const char* rutaCarpeta = "fs:/vol/external01/WiiUCamera Files";
    
    // Esto es seguro: Si la carpeta ya existe, mkdir simplemente devuelve -1 y sigue.
    // No borra nada ni crea duplicados.
    mkdir(rutaCarpeta, 0777);

    // 2. Nombre Único
    time_t t = time(NULL);
    char nombreArchivo[256];
    sprintf(nombreArchivo, "%s/Foto_%ld.bmp", rutaCarpeta, (long)t);

    // 3. Crear Superficie directamente desde los datos crudos (Mucho más estable)
    //    Usamos el buffer 'imagenLimpia' que ya tiene los colores correctos.
    SDL_Surface* surfaceFoto = SDL_CreateRGBSurfaceFrom(
        pixelesRGBA, 
        CAM_ANCHO_REAL, CAM_ALTO, 
        32, // Profundidad (32 bits)
        CAM_ANCHO_REAL * 4, // Pitch (Ancho * 4 bytes por pixel)
        0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF // Máscaras RGBA
    );

    if (surfaceFoto) {
        SDL_SaveBMP(surfaceFoto, nombreArchivo);
        SDL_FreeSurface(surfaceFoto);
    }
}

// --- DIBUJAR BOTÓN ---
void DibujarInterfazFoto(SDL_Renderer* renderer, bool disparando) {
    SDL_Rect btnRect = { 1080, 320, 80, 80 }; 
    if (disparando) SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
    else            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &btnRect);
    
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_Rect borde = { 1075, 315, 90, 90 };
    SDL_RenderDrawRect(renderer, &borde);
    
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_Rect lente = { 1100, 340, 40, 40 };
    SDL_RenderFillRect(renderer, &lente);
}

int EjecutarCamara(SDL_Renderer* renderer) {
    // SETUP
    CAMSetupInfo setupInfo;
    CAMError error = CAMERA_ERROR_UNINITIALIZED;
    CAMHandle camHandle = 0;
    bool exito = false;
    void* workMemPtr = NULL;
    VPADStatus vpad_data;
    VPADReadError vpad_error;
    CAMStreamInfo streamInfo;
    
    streamInfo.type = CAMERA_STREAM_TYPE_1;
    streamInfo.width = CAM_ANCHO_REAL;
    streamInfo.height = CAM_ALTO;

    int workMemSize = CAMGetMemReq(&streamInfo);
    workMemPtr = memalign(256, workMemSize); 
    if (workMemPtr) memset(workMemPtr, 0, workMemSize);

    memset(&setupInfo, 0, sizeof(CAMSetupInfo));
    setupInfo.streamInfo = streamInfo;
    setupInfo.workMem.pMem = workMemPtr;
    setupInfo.workMem.size = workMemSize;
    setupInfo.eventHandler = MiCallbackDeCamara; 
    setupInfo.mode.fps = CAMERA_FPS_30;

    camHandle = CAMInit(0, &setupInfo, &error);
    if (error == CAMERA_ERROR_OK) { exito = true; CAMOpen(camHandle); } 
    else { if(workMemPtr) free(workMemPtr); return 0; }

    uint8_t* imagenRaw = (uint8_t*)memalign(256, CAMERA_YUV_BUFFER_SIZE);
    uint32_t* imagenLimpia = (uint32_t*)memalign(256, CAM_ANCHO_REAL * CAM_ALTO * 4);
    
    CAMSurface superficie;
    memset(&superficie, 0, sizeof(CAMSurface));
    superficie.width = CAM_ANCHO_REAL;
    superficie.height = CAM_ALTO;
    superficie.pitch = CAM_PITCH;
    superficie.alignment = CAMERA_YUV_BUFFER_ALIGNMENT;
    superficie.surfaceSize = CAMERA_YUV_BUFFER_SIZE;
    superficie.surfaceBuffer = imagenRaw;
    CAMSubmitTargetSurface(camHandle, &superficie);

    SDL_Texture* textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, CAM_ANCHO_REAL, CAM_ALTO);
    SDL_Rect destinoRect = {0, 0, 960, 720}; 
    
    int offsetUV = CAM_PITCH * CAM_ALTO;
    bool enCamara = true;
    int resultado = 0; 
    frameListo = false;
    
    int delayFoto = 0;
    int timerFlash = 0; // NUEVO: Contador para el efecto blanco

    while (enCamara) {
        SDL_PumpEvents(); 
        VPADRead(VPAD_CHAN_0, &vpad_data, 1, &vpad_error);
        
        if (delayFoto > 0) delayFoto--;
        if (timerFlash > 0) timerFlash--; // Reducir tiempo del flash

        if (vpad_data.trigger & VPAD_BUTTON_B) { enCamara = false; resultado = 1; }
        if (vpad_data.trigger & VPAD_BUTTON_HOME) { enCamara = false; resultado = -1; }

        if (exito && frameListo) {
            DCInvalidateRange(imagenRaw, CAMERA_YUV_BUFFER_SIZE);
            for (int y = 0; y < CAM_ALTO; y += 2) {
                int filaY1 = y * CAM_PITCH; int filaY2 = (y + 1) * CAM_PITCH;
                int filaOut1 = y * CAM_ANCHO_REAL; int filaOut2 = (y + 1) * CAM_ANCHO_REAL;
                int filaUV = offsetUV + ((y / 2) * CAM_PITCH);

                for (int x = 0; x < CAM_ANCHO_REAL; x += 2) {
                    int idxUV = filaUV + x;
                    int u = imagenRaw[idxUV] - 128; int v = imagenRaw[idxUV + 1] - 128;
                    int cR = (351 * v) >> 8; int cG = ((86 * u) + (179 * v)) >> 8; int cB = (444 * u) >> 8;
                    auto pixel = [&](int y_val) { 
                        return (CLAMP(y_val + cR) << 24) | (CLAMP(y_val - cG) << 16) | (CLAMP(y_val + cB) << 8) | 0xFF; 
                    };
                    imagenLimpia[filaOut1 + x] = pixel(imagenRaw[filaY1 + x]);
                    imagenLimpia[filaOut1 + x + 1] = pixel(imagenRaw[filaY1 + x + 1]);
                    imagenLimpia[filaOut2 + x] = pixel(imagenRaw[filaY2 + x]);
                    imagenLimpia[filaOut2 + x + 1] = pixel(imagenRaw[filaY2 + x + 1]);
                }
            }
            SDL_UpdateTexture(textura, NULL, imagenLimpia, CAM_ANCHO_REAL * 4);
            frameListo = false;
            CAMSubmitTargetSurface(camHandle, &superficie);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // Dibujar Cámara
        SDL_RenderCopy(renderer, textura, NULL, &destinoRect);
        
        // Dibujar UI (Botón)
        bool presionoDisparador = (vpad_data.trigger & VPAD_BUTTON_L) || 
                                  (vpad_data.trigger & VPAD_BUTTON_R) || 
                                  (vpad_data.trigger & VPAD_BUTTON_A);

        DibujarInterfazFoto(renderer, presionoDisparador);
        
        // --- LÓGICA DE FOTO Y FLASH ---
        if (presionoDisparador && delayFoto == 0) {
            // 1. Guardar la imagen (Usando buffer de RAM, sin bloqueos)
            GuardarFotoEnSD(imagenLimpia);
            
            // 2. Activar Flash
            timerFlash = 10; // Durará 10 cuadros (aprox 0.2 segundos)
            
            // 3. Delay para no tomar 100 fotos
            delayFoto = 60;
        }

        // --- DIBUJAR EL EFECTO FLASH ---
        if (timerFlash > 0) {
            // Rectángulo blanco gigante
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_Rect flashRect = {0, 0, 1280, 720};
            SDL_RenderFillRect(renderer, &flashRect);
        }

        SDL_RenderPresent(renderer);
    }

    if (exito) { CAMClose(camHandle); CAMExit(camHandle); }
    if (textura) SDL_DestroyTexture(textura);
    if (imagenRaw) free(imagenRaw);
    if (imagenLimpia) free(imagenLimpia);
    if (workMemPtr) free(workMemPtr);
    return resultado;
}

#endif