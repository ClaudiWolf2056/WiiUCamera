#ifndef CAMARA_H
#define CAMARA_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_ttf.h> 
#include <camera/camera.h>
#include <vpad/input.h>
#include <proc_ui/procui.h>
#include <coreinit/cache.h> // <--- IMPORTANTE: Para arreglar la corrupción
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h> 
#include <math.h> 

// --- CONFIGURACIÓN ORIGINAL ---
#define CAM_ANCHO_REAL 640
#define CAM_ALTO       480
#define CAM_PITCH      768 
#define CLAMP(x) (((x) > 255) ? 255 : (((x) < 0) ? 0 : (x)))

struct DatosGuardado {
    uint32_t* bufferPixeles;
    int ancho;
    int alto;
};

struct ContextoCamara {
    CAMHandle handle;
    void* workMem;
    uint8_t* rawBuffer;
    uint32_t* cleanBuffer;
    CAMSurface surface;
    bool exito;
    SDL_Texture* textura;
};

static volatile bool frameListo = false;

static void MiCallbackDeCamara(CAMEventData *evento) {
    if (evento->eventType == CAMERA_DECODE_DONE) frameListo = true;
}

int HiloGuardarFoto(void* data) {
    DatosGuardado* datos = (DatosGuardado*)data;
    const char* rutaCarpeta = "fs:/vol/external01/WiiUCamera Files";
    mkdir(rutaCarpeta, 0777);

    time_t t = time(NULL);
    char nombreArchivo[256];
    sprintf(nombreArchivo, "%s/Foto_%ld.bmp", rutaCarpeta, (long)t);

    // SDL Masks para Big Endian (Wii U)
    SDL_Surface* surfaceFoto = SDL_CreateRGBSurfaceFrom(
        datos->bufferPixeles, datos->ancho, datos->alto, 32, datos->ancho * 4,
        0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
    );

    if (surfaceFoto) {
        SDL_SaveBMP(surfaceFoto, nombreArchivo);
        SDL_FreeSurface(surfaceFoto);
    }

    free(datos->bufferPixeles);
    free(datos); 
    return 0; 
}

void LlenarCirculo(SDL_Renderer *renderer, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrt(radius*radius - dy*dy);
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void DibujarTextoEnCaja(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int y, int x_inicio, int ancho_caja) {
    if (!font) return;
    SDL_Color blanco = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, texto, blanco);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        int x_centrado = x_inicio + (ancho_caja - surface->w) / 2;
        SDL_Rect rect = { x_centrado, y, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }
}

ContextoCamara IniciarCamaraContexto(SDL_Renderer* renderer) {
    ContextoCamara ctx;
    memset(&ctx, 0, sizeof(ContextoCamara));
    
    CAMError error = CAMERA_ERROR_UNINITIALIZED;
    CAMStreamInfo streamInfo;
    streamInfo.type = CAMERA_STREAM_TYPE_1;
    streamInfo.width = CAM_ANCHO_REAL;
    streamInfo.height = CAM_ALTO;

    int workMemSize = CAMGetMemReq(&streamInfo);
    ctx.workMem = memalign(256, workMemSize);
    if(ctx.workMem) memset(ctx.workMem, 0, workMemSize);

    CAMSetupInfo setupInfo;
    memset(&setupInfo, 0, sizeof(CAMSetupInfo));
    setupInfo.streamInfo = streamInfo;
    setupInfo.workMem.pMem = ctx.workMem;
    setupInfo.workMem.size = workMemSize;
    setupInfo.eventHandler = MiCallbackDeCamara;
    setupInfo.mode.fps = CAMERA_FPS_30;

    ctx.handle = CAMInit(0, &setupInfo, &error);
    if (error == CAMERA_ERROR_OK) {
        ctx.exito = true;
        CAMOpen(ctx.handle);
        ctx.rawBuffer = (uint8_t*)memalign(256, CAMERA_YUV_BUFFER_SIZE);
        ctx.cleanBuffer = (uint32_t*)memalign(256, CAM_ANCHO_REAL * CAM_ALTO * 4);
        memset(&ctx.surface, 0, sizeof(CAMSurface));
        ctx.surface.width = CAM_ANCHO_REAL;
        ctx.surface.height = CAM_ALTO;
        ctx.surface.pitch = CAM_PITCH;
        ctx.surface.alignment = CAMERA_YUV_BUFFER_ALIGNMENT;
        ctx.surface.surfaceSize = CAMERA_YUV_BUFFER_SIZE;
        ctx.surface.surfaceBuffer = ctx.rawBuffer;
        CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        ctx.textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, CAM_ANCHO_REAL, CAM_ALTO);
    } else {
        if(ctx.workMem) free(ctx.workMem);
        ctx.exito = false;
    }
    return ctx;
}

void CerrarCamaraContexto(ContextoCamara* ctx) {
    if (ctx->exito) { CAMClose(ctx->handle); CAMExit(ctx->handle); }
    if (ctx->textura) SDL_DestroyTexture(ctx->textura);
    if (ctx->rawBuffer) free(ctx->rawBuffer);
    if (ctx->cleanBuffer) free(ctx->cleanBuffer);
    if (ctx->workMem) free(ctx->workMem);
    ctx->exito = false;
}

void DibujarInterfazFoto(SDL_Renderer* renderer, bool disparando, TTF_Font* font, bool esIngles) {
    int cx = 1120; int cy = 360; int radio = 45;
    if (disparando) SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
    else            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    LlenarCirculo(renderer, cx, cy, radio);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);

    if (esIngles) {
        DibujarTextoEnCaja(renderer, font, "Press A / L / R", 50, 960, 320);
        DibujarTextoEnCaja(renderer, font, "to take photo", 85, 960, 320);
        DibujarTextoEnCaja(renderer, font, "Press B to exit", 650, 960, 320);
    } else {
        DibujarTextoEnCaja(renderer, font, "Presiona A / L / R", 50, 960, 320);
        DibujarTextoEnCaja(renderer, font, "para tomar foto", 85, 960, 320);
        DibujarTextoEnCaja(renderer, font, "Presiona B para salir", 650, 960, 320);
    }
}

int EjecutarCamara(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    ContextoCamara ctx = IniciarCamaraContexto(renderer);
    
    SDL_Rect destinoRect = {0, 0, 960, 720}; 
    int offsetUV = CAM_PITCH * CAM_ALTO;
    bool enCamara = true;
    int resultado = 0; 
    frameListo = false;
    
    int delayFoto = 0;
    int framesFlash = 0;

    while (enCamara) {
        ProcUIStatus status = ProcUISubProcessMessages(true);
        
        if (status == PROCUI_STATUS_EXITING) { enCamara = false; resultado = -1; }
        else if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
            CerrarCamaraContexto(&ctx);
            ProcUIDrawDoneRelease(); 
        }
        else {
            if (!ctx.exito) { ctx = IniciarCamaraContexto(renderer); frameListo = false; }

            VPADStatus vpad; VPADReadError err;
            VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
            if (delayFoto > 0) delayFoto--;
            if (framesFlash > 0) framesFlash--;

            if (vpad.trigger & VPAD_BUTTON_B) { enCamara = false; resultado = 1; }

            if (ctx.exito && frameListo) {
                DCInvalidateRange(ctx.rawBuffer, CAMERA_YUV_BUFFER_SIZE);
                for (int y = 0; y < CAM_ALTO; y += 2) {
                    int fY1=y*CAM_PITCH, fY2=(y+1)*CAM_PITCH, fOut1=y*CAM_ANCHO_REAL, fOut2=(y+1)*CAM_ANCHO_REAL;
                    int fUV = offsetUV + ((y/2)*CAM_PITCH);
                    for (int x = 0; x < CAM_ANCHO_REAL; x += 2) {
                        int idx=fUV+x, u=ctx.rawBuffer[idx]-128, v=ctx.rawBuffer[idx+1]-128;
                        int cR=(351*v)>>8, cG=((86*u)+(179*v))>>8, cB=(444*u)>>8;
                        auto p = [&](int yv){ return (CLAMP(yv+cR)<<24)|(CLAMP(yv-cG)<<16)|(CLAMP(yv+cB)<<8)|0xFF; };
                        ctx.cleanBuffer[fOut1+x] = p(ctx.rawBuffer[fY1+x]); ctx.cleanBuffer[fOut1+x+1] = p(ctx.rawBuffer[fY1+x+1]);
                        ctx.cleanBuffer[fOut2+x] = p(ctx.rawBuffer[fY2+x]); ctx.cleanBuffer[fOut2+x+1] = p(ctx.rawBuffer[fY2+x+1]);
                    }
                }
                SDL_UpdateTexture(ctx.textura, NULL, ctx.cleanBuffer, CAM_ANCHO_REAL * 4);
                frameListo = false;
                CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
            }

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            if (ctx.textura) SDL_RenderCopy(renderer, ctx.textura, NULL, &destinoRect);
            
            bool disparo = (vpad.trigger & VPAD_BUTTON_L) || (vpad.trigger & VPAD_BUTTON_R) || (vpad.trigger & VPAD_BUTTON_A);
            
            DibujarInterfazFoto(renderer, disparo, font, esIngles);

            if (framesFlash > 0) {
                int alpha = (framesFlash * 255) / 10; 
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
                SDL_RenderFillRect(renderer, NULL); 
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }

            if (disparo && delayFoto == 0) {
                DatosGuardado* datos = (DatosGuardado*)malloc(sizeof(DatosGuardado));
                if (datos) {
                    datos->ancho = CAM_ANCHO_REAL;
                    datos->alto = CAM_ALTO;
                    size_t tamano = CAM_ANCHO_REAL * CAM_ALTO * 4;
                    datos->bufferPixeles = (uint32_t*)memalign(256, tamano);
                    if (datos->bufferPixeles) {
                        memcpy(datos->bufferPixeles, ctx.cleanBuffer, tamano);
                        
                        // --- LA CURA MÁGICA ---
                        // Forzamos a la CPU a guardar los datos en RAM para que el hilo los lea bien
                        DCStoreRange(datos->bufferPixeles, tamano);
                        
                        SDL_Thread* hilo = SDL_CreateThread(HiloGuardarFoto, "GuardarFoto", datos);
                        SDL_DetachThread(hilo); 
                        framesFlash = 10; 
                        delayFoto = 30;   
                    } else { free(datos); }
                }
            }
            SDL_RenderPresent(renderer);
        }
    }
    CerrarCamaraContexto(&ctx);
    return resultado;
}

#endif