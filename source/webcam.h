#ifndef WEBCAM_H
#define WEBCAM_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <camera/camera.h>
#include <vpad/input.h>
#include <coreinit/cache.h>
#include <malloc.h>
#include <unistd.h>

// Usamos las mismas definiciones que camara.h para estabilidad
#define CAM_WIDTH 640
#define CAM_HEIGHT 480
#define CAM_PITCH 768 

// Estructura de contexto simplificada (Solo para visualizar)
struct ContextoWebcam {
    CAMHandle handle;
    void* workMem;
    uint8_t* rawBuffer;
    uint32_t* cleanBuffer;
    CAMSurface surface;
    bool exito;
    SDL_Texture* textura;
};

static volatile bool webFrameListo = false;

static void CallbackWebcam(CAMEventData *evento) {
    if (evento->eventType == CAMERA_DECODE_DONE) webFrameListo = true;
}

#ifndef CLAMP
#define CLAMP(v) (((v)>255)?255:(((v)<0)?0:(v)))
#endif

// Procesamiento de imagen (Copiado de camara.h para consistencia)
void ProcesarFrameWebcam(ContextoWebcam* ctx) {
    int offsetUV = CAM_PITCH * CAM_HEIGHT;
    for (int y = 0; y < CAM_HEIGHT; y += 2) {
        int fY1 = y * CAM_PITCH; int fY2 = (y + 1) * CAM_PITCH;
        int fUV = offsetUV + ((y / 2) * CAM_PITCH);
        int fOut1 = y * CAM_WIDTH; int fOut2 = (y + 1) * CAM_WIDTH;

        for (int x = 0; x < CAM_WIDTH; x += 2) {
            int idx = fUV + x;
            int u = ctx->rawBuffer[idx] - 128; int v = ctx->rawBuffer[idx + 1] - 128;
            int ySamples[4] = { ctx->rawBuffer[fY1 + x], ctx->rawBuffer[fY1 + x + 1], ctx->rawBuffer[fY2 + x], ctx->rawBuffer[fY2 + x + 1] };
            int outIndices[4] = { fOut1 + x, fOut1 + x + 1, fOut2 + x, fOut2 + x + 1 };

            for (int k = 0; k < 4; k++) {
                int yVal = ySamples[k];
                int cR = yVal + ((351 * v) >> 8);
                int cG = yVal - (((86 * u) + (179 * v)) >> 8);
                int cB = yVal + ((444 * u) >> 8);
                int r = CLAMP(cR); int g = CLAMP(cG); int b = CLAMP(cB);
                ctx->cleanBuffer[outIndices[k]] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
            }
        }
    }
}

ContextoWebcam IniciarWebcamContexto(SDL_Renderer* renderer) {
    ContextoWebcam ctx;
    memset(&ctx, 0, sizeof(ContextoWebcam));
    CAMError err;
    CAMStreamInfo info;
    info.type = CAMERA_STREAM_TYPE_1;
    info.width = CAM_WIDTH; info.height = CAM_HEIGHT;
    int memSize = CAMGetMemReq(&info);
    ctx.workMem = memalign(256, memSize);
    CAMSetupInfo setup;
    memset(&setup, 0, sizeof(CAMSetupInfo));
    setup.streamInfo = info;
    setup.workMem.pMem = ctx.workMem;
    setup.workMem.size = memSize;
    setup.eventHandler = CallbackWebcam;
    setup.mode.fps = CAMERA_FPS_30;

    ctx.handle = CAMInit(0, &setup, &err);
    if (err == CAMERA_ERROR_OK) {
        ctx.exito = true;
        CAMOpen(ctx.handle);
        ctx.rawBuffer = (uint8_t*)memalign(256, CAMERA_YUV_BUFFER_SIZE);
        ctx.cleanBuffer = (uint32_t*)memalign(256, CAM_WIDTH * CAM_HEIGHT * 4);
        memset(&ctx.surface, 0, sizeof(CAMSurface));
        ctx.surface.width = CAM_WIDTH;
        ctx.surface.height = CAM_HEIGHT;
        ctx.surface.pitch = CAM_PITCH;
        ctx.surface.alignment = CAMERA_YUV_BUFFER_ALIGNMENT;
        ctx.surface.surfaceSize = CAMERA_YUV_BUFFER_SIZE;
        ctx.surface.surfaceBuffer = ctx.rawBuffer;
        CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        ctx.textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, CAM_WIDTH, CAM_HEIGHT);
    }
    return ctx;
}

void CerrarWebcamContexto(ContextoWebcam* ctx) {
    if (ctx->exito) { 
        CAMClose(ctx->handle); 
        CAMExit(ctx->handle); 
    }
    if (ctx->textura) SDL_DestroyTexture(ctx->textura);
    if (ctx->rawBuffer) free(ctx->rawBuffer);
    if (ctx->cleanBuffer) free(ctx->cleanBuffer);
    if (ctx->workMem) free(ctx->workMem);
    ctx->exito = false;
}

// --- BUCLE PRINCIPAL WEBCAM ---
void EjecutarWebcam(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    ContextoWebcam ctx = IniciarWebcamContexto(renderer);
    if (!ctx.exito) { CerrarWebcamContexto(&ctx); return; }

    bool enCamara = true;
    
    // CALCULO DE CENTRADO:
    // Pantalla: 1280 x 720
    // Imagen: 640 x 480
    // Escalado para llenar altura (720): 480 * 1.5 = 720
    // Nuevo Ancho: 640 * 1.5 = 960
    // Espacio horizontal restante: 1280 - 960 = 320
    // Margen izquierdo para centrar: 320 / 2 = 160
    SDL_Rect rectCamara = {160, 0, 960, 720}; 
    
    webFrameListo = false;

    while (enCamara) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
             if (event.type == SDL_QUIT) enCamara = false;
        }
        VPADStatus vpad; VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        // SALIR CON B
        if (vpad.trigger & VPAD_BUTTON_B) enCamara = false;
        
        // PROCESAR FRAME
        if (ctx.exito && webFrameListo) {
            DCInvalidateRange(ctx.rawBuffer, CAMERA_YUV_BUFFER_SIZE);
            ProcesarFrameWebcam(&ctx);
            SDL_UpdateTexture(ctx.textura, NULL, ctx.cleanBuffer, CAM_WIDTH * 4);
            webFrameListo = false;
            CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        }

        // RENDERIZAR
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Dibujar cámara centrada y grande
        if (ctx.textura) SDL_RenderCopy(renderer, ctx.textura, NULL, &rectCamara);

        // No dibujamos UI (Clean Feed)
        // Si quisieras un indicador de "presiona B", descomenta esto:
        /*
        SDL_Color col = {150, 150, 150, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, esIngles ? "Press B to Exit" : "Presiona B para Salir", col);
        if(s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            SDL_Rect r = {20, 680, s->w, s->h};
            SDL_RenderCopy(renderer, t, NULL, &r);
            SDL_FreeSurface(s); SDL_DestroyTexture(t);
        }
        */

        SDL_RenderPresent(renderer);
    }
    CerrarWebcamContexto(&ctx);
}

#endif