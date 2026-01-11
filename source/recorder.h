#ifndef RECORDER_H
#define RECORDER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <camera/camera.h>
#include <vpad/input.h>
#include <proc_ui/procui.h>
#include <coreinit/cache.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h> 

#define REC_ANCHO 640
#define REC_ALTO  480
#define REC_PITCH 768 
#define MAX_FRAMES 120 

struct VideoManager {
    uint32_t* granBufferRAM; 
    int framesCapturados;
    bool grabando;
};

struct ContextoRecorder {
    CAMHandle handle;
    void* workMem;
    uint8_t* rawBuffer;
    uint32_t* cleanBuffer;
    CAMSurface surface;
    bool exito;
    SDL_Texture* textura;
};

static volatile bool recFrameListo = false;

static void CallbackRecorder(CAMEventData *evento) {
    if (evento->eventType == CAMERA_DECODE_DONE) recFrameListo = true;
}

void EscribirIntLE(FILE* f, uint32_t valor) {
    uint32_t le = ((valor >> 24) & 0xFF) | ((valor >> 8) & 0xFF00) | ((valor << 8) & 0xFF0000) | ((valor << 24) & 0xFF000000);
    fwrite(&le, 4, 1, f);
}
void EscribirShortLE(FILE* f, uint16_t valor) {
    uint16_t le = (valor >> 8) | (valor << 8);
    fwrite(&le, 2, 1, f);
}
void Escribir4Chars(FILE* f, const char* s) { fwrite(s, 1, 4, f); }

void EscribirCabeceraAVI(FILE* f, int frames, int ancho, int alto) {
    uint32_t frameSize = ancho * alto * 4;
    uint32_t dataSize = (frameSize + 8) * frames; 
    uint32_t riffSize = dataSize + 220; 

    Escribir4Chars(f, "RIFF"); EscribirIntLE(f, riffSize); Escribir4Chars(f, "AVI ");
    Escribir4Chars(f, "LIST"); EscribirIntLE(f, 192);      Escribir4Chars(f, "hdrl");
    
    Escribir4Chars(f, "avih"); EscribirIntLE(f, 56);       
    EscribirIntLE(f, 33333); 
    EscribirIntLE(f, frameSize * 30); 
    EscribirIntLE(f, 0); EscribirIntLE(f, 0x10); 
    EscribirIntLE(f, frames); EscribirIntLE(f, 0); EscribirIntLE(f, 1); EscribirIntLE(f, frameSize);
    EscribirIntLE(f, ancho); EscribirIntLE(f, alto); 
    EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0); 

    Escribir4Chars(f, "LIST"); EscribirIntLE(f, 116);      Escribir4Chars(f, "strl");
    Escribir4Chars(f, "strh"); EscribirIntLE(f, 56);
    Escribir4Chars(f, "vids"); Escribir4Chars(f, "DIB ");
    EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0);
    EscribirIntLE(f, 1); EscribirIntLE(f, 30);
    EscribirIntLE(f, 0); EscribirIntLE(f, frames);
    EscribirIntLE(f, frameSize);
    EscribirIntLE(f, -1); EscribirIntLE(f, 0);
    EscribirShortLE(f, 0); EscribirShortLE(f, 0); EscribirShortLE(f, ancho); EscribirShortLE(f, alto);

    Escribir4Chars(f, "strf"); EscribirIntLE(f, 40);       
    EscribirIntLE(f, 40); EscribirIntLE(f, ancho); EscribirIntLE(f, alto);
    EscribirShortLE(f, 1); EscribirShortLE(f, 32);
    EscribirIntLE(f, 0); EscribirIntLE(f, frameSize);
    EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0); 

    Escribir4Chars(f, "LIST"); EscribirIntLE(f, dataSize + 4); Escribir4Chars(f, "movi");
}

ContextoRecorder IniciarGrabadoraContexto(SDL_Renderer* renderer) {
    ContextoRecorder ctx;
    memset(&ctx, 0, sizeof(ContextoRecorder));
    CAMError error = CAMERA_ERROR_UNINITIALIZED;
    CAMStreamInfo streamInfo;
    streamInfo.type = CAMERA_STREAM_TYPE_1;
    streamInfo.width = REC_ANCHO;
    streamInfo.height = REC_ALTO;
    int workMemSize = CAMGetMemReq(&streamInfo);
    ctx.workMem = memalign(256, workMemSize);
    if(ctx.workMem) memset(ctx.workMem, 0, workMemSize);
    CAMSetupInfo setupInfo;
    memset(&setupInfo, 0, sizeof(CAMSetupInfo));
    setupInfo.streamInfo = streamInfo;
    setupInfo.workMem.pMem = ctx.workMem;
    setupInfo.workMem.size = workMemSize;
    setupInfo.eventHandler = CallbackRecorder;
    setupInfo.mode.fps = CAMERA_FPS_30;
    ctx.handle = CAMInit(0, &setupInfo, &error);
    if (error == CAMERA_ERROR_OK) {
        ctx.exito = true;
        CAMOpen(ctx.handle);
        ctx.rawBuffer = (uint8_t*)memalign(256, CAMERA_YUV_BUFFER_SIZE);
        ctx.cleanBuffer = (uint32_t*)memalign(256, REC_ANCHO * REC_ALTO * 4);
        memset(&ctx.surface, 0, sizeof(CAMSurface));
        ctx.surface.width = REC_ANCHO;
        ctx.surface.height = REC_ALTO;
        ctx.surface.pitch = REC_PITCH;
        ctx.surface.alignment = CAMERA_YUV_BUFFER_ALIGNMENT;
        ctx.surface.surfaceSize = CAMERA_YUV_BUFFER_SIZE;
        ctx.surface.surfaceBuffer = ctx.rawBuffer;
        CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        ctx.textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, REC_ANCHO, REC_ALTO);
    } else {
        if(ctx.workMem) free(ctx.workMem);
        ctx.exito = false;
    }
    return ctx;
}

void CerrarGrabadoraContexto(ContextoRecorder* ctx) {
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

void GuardarVideoAVI(SDL_Renderer* renderer, VideoManager* vm, TTF_Font* font) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_Color colorTexto = {255, 255, 0, 255};
    SDL_Surface* txtSurf = TTF_RenderText_Blended(font, "GUARDANDO... / SAVING...", colorTexto);
    SDL_Texture* txtTex = SDL_CreateTextureFromSurface(renderer, txtSurf);
    SDL_Rect rect = {(1280 - txtSurf->w)/2, 360, txtSurf->w, txtSurf->h};
    SDL_RenderCopy(renderer, txtTex, NULL, &rect);
    SDL_RenderPresent(renderer);
    SDL_FreeSurface(txtSurf); SDL_DestroyTexture(txtTex);

    time_t t = time(NULL);
    char nombreArchivo[256];
    sprintf(nombreArchivo, "fs:/vol/external01/WiiUCamera Files/Video_%ld.avi", (long)t);

    FILE* f = fopen(nombreArchivo, "wb");
    if (f) {
        EscribirCabeceraAVI(f, vm->framesCapturados, REC_ANCHO, REC_ALTO);
        
        uint32_t tamanoFrame = REC_ANCHO * REC_ALTO * 4;
        uint32_t* frameBuffer = (uint32_t*)malloc(tamanoFrame);

        if (frameBuffer) {
            for(int i=0; i < vm->framesCapturados; i++) {
                 Escribir4Chars(f, "00db"); EscribirIntLE(f, tamanoFrame);
                 uint32_t* ptrFrameStart = vm->granBufferRAM + (i * (REC_ANCHO * REC_ALTO));
                 int idx = 0;
                 for (int y = REC_ALTO - 1; y >= 0; y--) { 
                     uint32_t* ptrLinea = ptrFrameStart + (y * REC_ANCHO);
                     for (int x = 0; x < REC_ANCHO; x++) {
                         uint32_t pixelOriginal = ptrLinea[x];
                         uint32_t r = (pixelOriginal >> 24) & 0xFF;
                         uint32_t g = (pixelOriginal >> 16) & 0xFF;
                         uint32_t b = (pixelOriginal >> 8) & 0xFF;
                         uint32_t a = pixelOriginal & 0xFF;
                         frameBuffer[idx++] = (b << 24) | (g << 16) | (r << 8) | a;
                     }
                 }
                 fwrite(frameBuffer, 1, tamanoFrame, f);
            }
            free(frameBuffer);
        }
        fflush(f);
        fclose(f);
    }
}

void DibujarInfoLateral(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int y) {
    SDL_Color col = {200, 200, 200, 255};
    SDL_Surface* s = TTF_RenderText_Blended(font, texto, col);
    if(s) {
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        SDL_Rect r = {980, y, s->w, s->h}; 
        SDL_RenderCopy(renderer, t, NULL, &r);
        SDL_FreeSurface(s); SDL_DestroyTexture(t);
    }
}

int EjecutarGrabadora(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    ContextoRecorder ctx = IniciarGrabadoraContexto(renderer);
    VideoManager vm;
    vm.framesCapturados = 0;
    vm.grabando = false;
    
    size_t tamanoTotalVideo = REC_ANCHO * REC_ALTO * 4 * MAX_FRAMES;
    vm.granBufferRAM = (uint32_t*)memalign(256, tamanoTotalVideo);

    if (!vm.granBufferRAM) { 
        CerrarGrabadoraContexto(&ctx); return 0; 
    }

    SDL_Rect destinoRect = {0, 0, 960, 720}; 
    int offsetUV = REC_PITCH * REC_ALTO;
    bool enRecorder = true;
    recFrameListo = false;
    int resultado = 0;

    while (enRecorder) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
             if (event.type == SDL_QUIT) { enRecorder = false; resultado = -1; }
        }
        VPADStatus vpad; VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (vpad.trigger & VPAD_BUTTON_B) { 
            if (vm.grabando) { vm.grabando = false; GuardarVideoAVI(renderer, &vm, font); vm.framesCapturados = 0; }
            else { enRecorder = false; resultado = 1; }
        }
        if (vpad.trigger & VPAD_BUTTON_A) {
            if (!vm.grabando) { vm.grabando = true; vm.framesCapturados = 0; }
            else { vm.grabando = false; GuardarVideoAVI(renderer, &vm, font); }
        }

        if (ctx.exito && recFrameListo) {
            DCInvalidateRange(ctx.rawBuffer, CAMERA_YUV_BUFFER_SIZE);
            for (int y = 0; y < REC_ALTO; y += 2) {
                int fY1=y*REC_PITCH, fY2=(y+1)*REC_PITCH, fOut1=y*REC_ANCHO, fOut2=(y+1)*REC_ANCHO;
                int fUV = offsetUV + ((y/2)*REC_PITCH);
                for (int x = 0; x < REC_ANCHO; x += 2) {
                    int idx=fUV+x, u=ctx.rawBuffer[idx]-128, v=ctx.rawBuffer[idx+1]-128;
                    int cR=(351*v)>>8, cG=((86*u)+(179*v))>>8, cB=(444*u)>>8;
                    #define CL(v) (((v)>255)?255:(((v)<0)?0:(v)))
                    auto p = [&](int yv){ return (CL(yv+cR)<<24) | (CL(yv-cG)<<16) | (CL(yv+cB)<<8) | 0xFF; };
                    ctx.cleanBuffer[fOut1+x] = p(ctx.rawBuffer[fY1+x]); ctx.cleanBuffer[fOut1+x+1] = p(ctx.rawBuffer[fY1+x+1]);
                    ctx.cleanBuffer[fOut2+x] = p(ctx.rawBuffer[fY2+x]); ctx.cleanBuffer[fOut2+x+1] = p(ctx.rawBuffer[fY2+x+1]);
                }
            }
            SDL_UpdateTexture(ctx.textura, NULL, ctx.cleanBuffer, REC_ANCHO * 4);
            
            if (vm.grabando && vm.framesCapturados < MAX_FRAMES) {
                uint32_t* destino = vm.granBufferRAM + (vm.framesCapturados * (REC_ANCHO * REC_ALTO));
                memcpy(destino, ctx.cleanBuffer, REC_ANCHO * REC_ALTO * 4);
                vm.framesCapturados++;
                if (vm.framesCapturados >= MAX_FRAMES) { vm.grabando = false; GuardarVideoAVI(renderer, &vm, font); }
            }
            recFrameListo = false;
            CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (ctx.textura) SDL_RenderCopy(renderer, ctx.textura, NULL, &destinoRect);
        
        // --- INTERFAZ UI CORREGIDA (Derecha) ---
        if (vm.grabando) {
            static int blink = 0; blink++;
            if ((blink / 10) % 2 == 0) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_Rect recDot = {1100, 50, 30, 30};
                SDL_RenderFillRect(renderer, &recDot);
            }
            int barraW = (vm.framesCapturados * 280) / MAX_FRAMES; 
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_Rect barra = {980, 100, barraW, 20};
            SDL_RenderFillRect(renderer, &barra);
            
            DibujarInfoLateral(renderer, font, "REC...", 130);
            if (esIngles) DibujarInfoLateral(renderer, font, "Press B: Stop/Save", 170);
            else DibujarInfoLateral(renderer, font, "Presiona B: Guardar", 170);

        } else {
             if (esIngles) {
                DibujarInfoLateral(renderer, font, "Mode: AVI Video", 50);
                DibujarInfoLateral(renderer, font, "(A) Start REC", 100);
                DibujarInfoLateral(renderer, font, "(B) Exit Mode", 140);
                DibujarInfoLateral(renderer, font, "Max: 3.5 sec", 220);
                DibujarInfoLateral(renderer, font, "File: >100 MB", 260);
             } else {
                DibujarInfoLateral(renderer, font, "Modo: Video AVI", 50);
                DibujarInfoLateral(renderer, font, "(A) Iniciar REC", 100);
                DibujarInfoLateral(renderer, font, "(B) Salir Modo", 140);
                DibujarInfoLateral(renderer, font, "Max: 3.5 seg", 220);
                DibujarInfoLateral(renderer, font, "Archivo: >100MB", 260);
             }
        }
        SDL_RenderPresent(renderer);
    }

    if (vm.granBufferRAM) { free(vm.granBufferRAM); vm.granBufferRAM = NULL; }
    CerrarGrabadoraContexto(&ctx);
    return resultado;
}

#endif