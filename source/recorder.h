#ifndef RECORDER_H
#define RECORDER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vpad/input.h>
#include <coreinit/dynload.h>
#include <coreinit/cache.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h> 
#include <mic/mic.h> 

#if __has_include(<camera/camera.h>)
    #include <camera/camera.h>
#else
    typedef int CAMError; typedef int CAMHandle;
    #define CAMERA_ERROR_OK 0
    #define CAMERA_STREAM_TYPE_1 1
    #define CAMERA_FPS_30 30
    #define CAMERA_YUV_BUFFER_SIZE 0x96000 
    #define CAMERA_YUV_BUFFER_ALIGNMENT 256
    #define CAMERA_DECODE_DONE 2
    typedef struct { int type; int width; int height; } CAMStreamInfo;
    typedef struct { void* pMem; int size; } CAMWorkMem;
    typedef struct { int eventType; int arg1; int arg2; } CAMEventData;
    typedef void (*CAMEventHandler)(CAMEventData*);
    typedef struct { int fps; } CAMMode;
    typedef struct { CAMStreamInfo streamInfo; CAMWorkMem workMem; CAMEventHandler eventHandler; CAMMode mode; } CAMSetupInfo;
    typedef struct { int width; int height; int pitch; int alignment; int surfaceSize; void* surfaceBuffer; } CAMSurface;
#endif

int (*ptr_CAMInit)(int, CAMSetupInfo*, CAMError*);
int (*ptr_CAMOpen)(CAMHandle);
int (*ptr_CAMClose)(CAMHandle);
int (*ptr_CAMExit)(CAMHandle);
int (*ptr_CAMSubmitTargetSurface)(CAMHandle, CAMSurface*);
int (*ptr_CAMGetMemReq)(CAMStreamInfo*);

OSDynLoad_Module g_camMod = 0;
bool g_dynCamLoaded = false;

bool CargarSimbolosCamara() {
    if (g_dynCamLoaded) return true;
    OSDynLoad_Acquire("camera.rpl", &g_camMod);
    OSDynLoad_FindExport(g_camMod, (OSDynLoad_ExportType)0, "CAMInit", (void**)&ptr_CAMInit);
    OSDynLoad_FindExport(g_camMod, (OSDynLoad_ExportType)0, "CAMOpen", (void**)&ptr_CAMOpen);
    OSDynLoad_FindExport(g_camMod, (OSDynLoad_ExportType)0, "CAMClose", (void**)&ptr_CAMClose);
    OSDynLoad_FindExport(g_camMod, (OSDynLoad_ExportType)0, "CAMExit", (void**)&ptr_CAMExit);
    OSDynLoad_FindExport(g_camMod, (OSDynLoad_ExportType)0, "CAMSubmitTargetSurface", (void**)&ptr_CAMSubmitTargetSurface);
    OSDynLoad_FindExport(g_camMod, (OSDynLoad_ExportType)0, "CAMGetMemReq", (void**)&ptr_CAMGetMemReq);
    if (ptr_CAMInit) { g_dynCamLoaded = true; return true; }
    return false;
}

#define REC_ANCHO 640
#define REC_ALTO  480
#define REC_PITCH 768 
#define VID_W 320
#define VID_H 240
#define MAX_FRAMES 600 // 30s @ 20 FPS

struct VideoManager { uint8_t* videoRAM; int framesCapturados; bool grabando; };
struct ContextoRecorder { CAMHandle handle; void* workMem; uint8_t* rawBuffer; uint32_t* cleanBuffer; CAMSurface surface; bool exito; SDL_Texture* textura; };


static MICHandle g_recHMic = -1;
static void* g_recMicBuf = NULL;
static MICWorkMemory g_recMicWorkMem; 
static bool g_recMicReady = false; 

static uint8_t* g_recAudioRAM = NULL;
static volatile uint32_t g_recAudioBytesCapturados = 0;
static volatile bool g_recAudioActive = false;
static volatile uint32_t g_recLastMicPos = 0;

static OSThread g_recAudioThread;
static uint8_t g_recThreadStack[0x4000];
static volatile bool g_recThreadRunning = false;

int RecAudioCaptureThread(int argc, const char **argv) {
    while (g_recThreadRunning) {
        if (g_recMicReady) { 
            MICStatus st; MICGetStatus(g_recHMic, &st);
            if (g_recAudioActive) {
                DCInvalidateRange(g_recMicBuf, 0x10000);
                uint32_t currPos = st.bufferPos;
                int bytesAvail = 0;

                if (currPos >= g_recLastMicPos) bytesAvail = currPos - g_recLastMicPos;
                else bytesAvail = (0x10000 - g_recLastMicPos) + currPos;

                if (bytesAvail > 0 && g_recAudioBytesCapturados + bytesAvail < (32000 * 2 * 35)) {
                    uint8_t* src = (uint8_t*)g_recMicBuf;
                    if (currPos >= g_recLastMicPos) {
                        memcpy(&g_recAudioRAM[g_recAudioBytesCapturados], &src[g_recLastMicPos], bytesAvail);
                    } else {
                        int part1 = 0x10000 - g_recLastMicPos;
                        int part2 = currPos;
                        memcpy(&g_recAudioRAM[g_recAudioBytesCapturados], &src[g_recLastMicPos], part1);
                        memcpy(&g_recAudioRAM[g_recAudioBytesCapturados + part1], &src[0], part2);
                    }
                    g_recAudioBytesCapturados += bytesAvail;
                    g_recLastMicPos = currPos;
                }
            } else {
                g_recLastMicPos = st.bufferPos;
            }
        }
        OSSleepTicks(OSMillisecondsToTicks(3));
    }
    return 0;
}


static volatile bool recFrameListo = false;
static void CallbackRecorder(CAMEventData *evento) { if (evento->eventType == CAMERA_DECODE_DONE) recFrameListo = true; }

void EscribirIntLE(FILE* f, uint32_t valor) { uint32_t le = ((valor >> 24) & 0xFF) | ((valor >> 8) & 0xFF00) | ((valor << 8) & 0xFF0000) | ((valor << 24) & 0xFF000000); fwrite(&le, 4, 1, f); }
void EscribirShortLE(FILE* f, uint16_t valor) { uint16_t le = (valor >> 8) | (valor << 8); fwrite(&le, 2, 1, f); }
void Escribir4Chars(FILE* f, const char* s) { fwrite(s, 1, 4, f); }


void EscribirCabeceraAVI(FILE* f, int frames, int ancho, int alto) {
    uint32_t frameSize = ancho * alto * 3; 
    uint32_t audioPerFrame = 3200; 
    uint32_t vidDataSize = (frameSize + 8) * frames;
    uint32_t audDataSize = (audioPerFrame + 8) * frames;
    uint32_t riffSize = 316 + vidDataSize + audDataSize;
    
    Escribir4Chars(f, "RIFF"); EscribirIntLE(f, riffSize); Escribir4Chars(f, "AVI "); 
    Escribir4Chars(f, "LIST"); EscribirIntLE(f, 292); Escribir4Chars(f, "hdrl");
    Escribir4Chars(f, "avih"); EscribirIntLE(f, 56); EscribirIntLE(f, 50000); EscribirIntLE(f, frameSize * 20); EscribirIntLE(f, 0); EscribirIntLE(f, 0x10); 
    EscribirIntLE(f, frames); EscribirIntLE(f, 0); EscribirIntLE(f, 2); EscribirIntLE(f, 0); EscribirIntLE(f, ancho); EscribirIntLE(f, alto); EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0); 
    Escribir4Chars(f, "LIST"); EscribirIntLE(f, 116); Escribir4Chars(f, "strl"); 
    Escribir4Chars(f, "strh"); EscribirIntLE(f, 56); Escribir4Chars(f, "vids"); Escribir4Chars(f, "DIB ");
    EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 1); EscribirIntLE(f, 20); EscribirIntLE(f, 0); EscribirIntLE(f, frames); EscribirIntLE(f, frameSize); EscribirIntLE(f, -1); EscribirIntLE(f, 0);
    EscribirShortLE(f, 0); EscribirShortLE(f, 0); EscribirShortLE(f, ancho); EscribirShortLE(f, alto);
    Escribir4Chars(f, "strf"); EscribirIntLE(f, 40); EscribirIntLE(f, 40); EscribirIntLE(f, ancho); EscribirIntLE(f, alto); EscribirShortLE(f, 1); EscribirShortLE(f, 24);
    EscribirIntLE(f, 0); EscribirIntLE(f, frameSize); EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0); 
    Escribir4Chars(f, "LIST"); EscribirIntLE(f, 92); Escribir4Chars(f, "strl");
    Escribir4Chars(f, "strh"); EscribirIntLE(f, 56); Escribir4Chars(f, "auds"); EscribirIntLE(f, 0); 
    EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 0); EscribirIntLE(f, 1); EscribirIntLE(f, 32000);
    EscribirIntLE(f, 0); EscribirIntLE(f, frames * 1600); EscribirIntLE(f, audioPerFrame); EscribirIntLE(f, -1); EscribirIntLE(f, 0); 
    EscribirShortLE(f, 0); EscribirShortLE(f, 0); EscribirShortLE(f, 0); EscribirShortLE(f, 0);
    Escribir4Chars(f, "strf"); EscribirIntLE(f, 16); EscribirShortLE(f, 1); EscribirShortLE(f, 1); 
    EscribirIntLE(f, 32000); EscribirIntLE(f, 64000); EscribirShortLE(f, 2); EscribirShortLE(f, 16); 
    Escribir4Chars(f, "LIST"); EscribirIntLE(f, vidDataSize + audDataSize + 4); Escribir4Chars(f, "movi");
}

ContextoRecorder IniciarGrabadoraContexto(SDL_Renderer* renderer) {
    ContextoRecorder ctx; memset(&ctx, 0, sizeof(ContextoRecorder));
    if (!CargarSimbolosCamara()) return ctx; 
    CAMError error = 0; CAMStreamInfo streamInfo; streamInfo.type = CAMERA_STREAM_TYPE_1; streamInfo.width = REC_ANCHO; streamInfo.height = REC_ALTO;
    int workMemSize = ptr_CAMGetMemReq(&streamInfo);
    ctx.workMem = memalign(256, workMemSize); if(ctx.workMem) memset(ctx.workMem, 0, workMemSize);
    CAMSetupInfo setupInfo; memset(&setupInfo, 0, sizeof(CAMSetupInfo));
    setupInfo.streamInfo = streamInfo; setupInfo.workMem.pMem = ctx.workMem; setupInfo.workMem.size = workMemSize;
    setupInfo.eventHandler = CallbackRecorder; setupInfo.mode.fps = CAMERA_FPS_30;
    
    ctx.handle = ptr_CAMInit(0, &setupInfo, &error);
    if (error == CAMERA_ERROR_OK) {
        ctx.exito = true; ptr_CAMOpen(ctx.handle);
        ctx.rawBuffer = (uint8_t*)memalign(256, CAMERA_YUV_BUFFER_SIZE);
        ctx.cleanBuffer = (uint32_t*)memalign(256, REC_ANCHO * REC_ALTO * 4);
        memset(&ctx.surface, 0, sizeof(CAMSurface));
        ctx.surface.width = REC_ANCHO; ctx.surface.height = REC_ALTO; ctx.surface.pitch = REC_PITCH;
        ctx.surface.alignment = CAMERA_YUV_BUFFER_ALIGNMENT; ctx.surface.surfaceSize = CAMERA_YUV_BUFFER_SIZE;
        ctx.surface.surfaceBuffer = ctx.rawBuffer;
        ptr_CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        ctx.textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, REC_ANCHO, REC_ALTO);
    } else { if(ctx.workMem) free(ctx.workMem); ctx.exito = false; }
    return ctx;
}

void CerrarGrabadoraContexto(ContextoRecorder* ctx) {
    if (ctx->exito && ptr_CAMClose) { ptr_CAMClose(ctx->handle); ptr_CAMExit(ctx->handle); }
    if (ctx->textura) SDL_DestroyTexture(ctx->textura);
    if (ctx->rawBuffer) free(ctx->rawBuffer);
    if (ctx->cleanBuffer) free(ctx->cleanBuffer);
    if (ctx->workMem) free(ctx->workMem);
    ctx->exito = false;
}


void MinionMuxerAVI(SDL_Renderer* renderer, VideoManager* vm, TTF_Font* font) {
    if (vm->framesCapturados == 0) return;

    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255); SDL_RenderClear(renderer);
    SDL_Surface* txP = TTF_RenderText_Blended(font, "SINCORNIZANDO VIDEO Y AUDIO...", {255, 255, 0, 255});
    if (txP) { SDL_Texture* tT = SDL_CreateTextureFromSurface(renderer, txP); SDL_Rect rT = {(1280 - txP->w)/2, 250, txP->w, txP->h}; SDL_RenderCopy(renderer, tT, NULL, &rT); SDL_FreeSurface(txP); SDL_DestroyTexture(tT); }
    SDL_RenderPresent(renderer);

    int16_t* samples = (int16_t*)g_recAudioRAM;
    size_t sampleCount = g_recAudioBytesCapturados / 2;
    for (size_t i = 0; i < sampleCount; i++) {
        samples[i] = (samples[i] << 8) | ((samples[i] >> 8) & 0xFF);
    }

    time_t t = time(NULL); 
    char nameAVI[256]; sprintf(nameAVI, "fs:/vol/external01/WiiUCamera Files/Video_%ld.avi", (long)t);

    FILE* fAvi = fopen(nameAVI, "wb");
    if (fAvi) {
        EscribirCabeceraAVI(fAvi, vm->framesCapturados, VID_W, VID_H);
        uint32_t vidChunkSize = VID_W * VID_H * 3;
        uint32_t audChunkSize = 3200; 
        
        for(int i=0; i < vm->framesCapturados; i++) {
            Escribir4Chars(fAvi, "00db"); EscribirIntLE(fAvi, vidChunkSize);
            fwrite(vm->videoRAM + (i * vidChunkSize), 1, vidChunkSize, fAvi);

            Escribir4Chars(fAvi, "01wb"); EscribirIntLE(fAvi, audChunkSize);
            int audOffset = i * audChunkSize;
            if (audOffset + audChunkSize <= g_recAudioBytesCapturados) {
                fwrite(g_recAudioRAM + audOffset, 1, audChunkSize, fAvi);
            } else {
                uint8_t silence[3200] = {0}; fwrite(silence, 1, audChunkSize, fAvi); 
            }

            if (i % 10 == 0 || i == vm->framesCapturados - 1) {
                SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255); SDL_RenderClear(renderer);
                SDL_Surface* txtSurf = TTF_RenderText_Blended(font, "GUARDANDO VIDEO CON AUDIO EN SD...", {0, 255, 100, 255});
                if (txtSurf) { SDL_Texture* txtTex = SDL_CreateTextureFromSurface(renderer, txtSurf); SDL_Rect rect = {(1280 - txtSurf->w)/2, 250, txtSurf->w, txtSurf->h}; SDL_RenderCopy(renderer, txtTex, NULL, &rect); SDL_FreeSurface(txtSurf); SDL_DestroyTexture(txtTex); }

                int wBar = 600; int hBar = 40; int fillW = (i * wBar) / vm->framesCapturados;
                SDL_Rect rBg = {(1280-wBar)/2, 330, wBar, hBar}; SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_RenderFillRect(renderer, &rBg);
                SDL_Rect rFg = {(1280-wBar)/2, 330, fillW, hBar}; SDL_SetRenderDrawColor(renderer, 0, 255, 100, 255); SDL_RenderFillRect(renderer, &rFg);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &rBg);

                char pct[32]; sprintf(pct, "%d%%", (i * 100) / vm->framesCapturados);
                SDL_Surface* pctSurf = TTF_RenderText_Blended(font, pct, {255, 255, 255, 255});
                if (pctSurf) { SDL_Texture* pctTex = SDL_CreateTextureFromSurface(renderer, pctSurf); SDL_Rect rectPct = {(1280 - pctSurf->w)/2, 400, pctSurf->w, pctSurf->h}; SDL_RenderCopy(renderer, pctTex, NULL, &rectPct); SDL_FreeSurface(pctSurf); SDL_DestroyTexture(pctTex); }

                SDL_RenderPresent(renderer);
            }
        }
        fflush(fAvi); fsync(fileno(fAvi)); fclose(fAvi);
    }
}

void DibujarInfoLateral(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int y) {
    SDL_Surface* s = TTF_RenderText_Blended(font, texto, {200, 200, 200, 255});
    if(s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); SDL_Rect r = {980, y, s->w, s->h}; SDL_RenderCopy(renderer, t, NULL, &r); SDL_FreeSurface(s); SDL_DestroyTexture(t); }
}

int EjecutarGrabadora(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    ContextoRecorder ctx = IniciarGrabadoraContexto(renderer);
    VideoManager vm; vm.framesCapturados = 0; vm.grabando = false; 
    
    vm.videoRAM = (uint8_t*)memalign(256, VID_W * VID_H * 3 * MAX_FRAMES); 
    g_recAudioRAM = (uint8_t*)memalign(256, 32000 * 2 * 35); 
    if (!vm.videoRAM || !g_recAudioRAM) { CerrarGrabadoraContexto(&ctx); return 0; }

    g_recMicBuf = memalign(0x40, 0x10000); 
    memset(g_recMicBuf, 0, 0x10000);
    g_recMicWorkMem.sampleBuffer = g_recMicBuf; 
    g_recMicWorkMem.sampleMaxCount = 0x10000/2;
    g_recMicReady = false;

    MICError mErr;
    g_recHMic = MICInit(MIC_INSTANCE_0, 0, &g_recMicWorkMem, &mErr); 
    
    if (g_recHMic >= 0 && (int)mErr == 0) { 
        if ((int)MICOpen(g_recHMic) == 0) {
            g_recMicReady = true;
        }
    }

    g_recThreadRunning = true; g_recAudioActive = false;
    OSCreateThread(&g_recAudioThread, RecAudioCaptureThread, 0, NULL, g_recThreadStack + sizeof(g_recThreadStack), sizeof(g_recThreadStack), 10, OS_THREAD_ATTRIB_AFFINITY_CPU1);
    OSResumeThread(&g_recAudioThread);

    SDL_Rect destinoRect = {0, 0, 960, 720}; int offsetUV = REC_PITCH * REC_ALTO;
    bool enRecorder = true; recFrameListo = false; int resultado = 0; int skipCounter = 0;

    while (enRecorder) {
        SDL_Event event; while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) { enRecorder = false; resultado = -1; } }
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (vpad.trigger & VPAD_BUTTON_B) { 
            if (vm.grabando) { g_recAudioActive = false; vm.grabando = false; MinionMuxerAVI(renderer, &vm, font); vm.framesCapturados = 0; }
            else { enRecorder = false; resultado = 1; }
        }
        if (vpad.trigger & VPAD_BUTTON_A) {
            if (!vm.grabando) { 
                vm.framesCapturados = 0; g_recAudioBytesCapturados = 0; 
                MICStatus st; if(g_recMicReady) { MICGetStatus(g_recHMic, &st); g_recLastMicPos = st.bufferPos; }
                vm.grabando = true; g_recAudioActive = true; 
            } else { 
                g_recAudioActive = false; vm.grabando = false; MinionMuxerAVI(renderer, &vm, font); 
            }
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
                skipCounter++;
                if (skipCounter % 3 != 0) { 
                    uint8_t* frameDest = vm.videoRAM + (vm.framesCapturados * (VID_W * VID_H * 3));
                    int destIdx = 0;
                    for(int y = REC_ALTO - 2; y >= 0; y -= 2) {
                        uint32_t* srcRow = ctx.cleanBuffer + (y * REC_ANCHO);
                        for(int x = 0; x < REC_ANCHO; x += 2) {
                            uint32_t pixel = srcRow[x];
                            frameDest[destIdx++] = (pixel >> 8) & 0xFF;  
                            frameDest[destIdx++] = (pixel >> 16) & 0xFF; 
                            frameDest[destIdx++] = (pixel >> 24) & 0xFF; 
                        }
                    }
                    vm.framesCapturados++;
                    if (vm.framesCapturados >= MAX_FRAMES) { g_recAudioActive = false; vm.grabando = false; MinionMuxerAVI(renderer, &vm, font); }
                }
            } 
            recFrameListo = false; ptr_CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
        if (ctx.textura) SDL_RenderCopy(renderer, ctx.textura, NULL, &destinoRect);
        
        if (vm.grabando) {
            static int blink = 0; blink++;
            if ((blink / 10) % 2 == 0) { SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); SDL_Rect recDot = {1100, 50, 30, 30}; SDL_RenderFillRect(renderer, &recDot); }
            int barraW = (vm.framesCapturados * 280) / MAX_FRAMES; 
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); SDL_Rect barra = {980, 100, barraW, 20}; SDL_RenderFillRect(renderer, &barra);
            char tiempo[32]; sprintf(tiempo, "REC %ds / 30s", vm.framesCapturados / 20);
            DibujarInfoLateral(renderer, font, tiempo, 130);
            if (esIngles) DibujarInfoLateral(renderer, font, "Press A: Stop/Save", 170); else DibujarInfoLateral(renderer, font, "Presiona A: Finalizar", 170);
        } else {
             if (esIngles) { DibujarInfoLateral(renderer, font, "Mode: AVI Video", 50); DibujarInfoLateral(renderer, font, "(A) Start REC", 100); DibujarInfoLateral(renderer, font, "(B) Exit Mode", 140); } 
             else { DibujarInfoLateral(renderer, font, "Modo: Video AVI", 50); DibujarInfoLateral(renderer, font, "(A) Iniciar REC", 100); DibujarInfoLateral(renderer, font, "(B) Salir Modo", 140); }
        }
        SDL_RenderPresent(renderer);
    }
    
    g_recThreadRunning = false; OSJoinThread(&g_recAudioThread, NULL);
    
    
    if(g_recMicReady) { 
        MICClose(g_recHMic); 
        MICUninit(g_recHMic); 
        g_recMicReady = false;
    }
    
    if(g_recMicBuf) free(g_recMicBuf);
    if (vm.videoRAM) free(vm.videoRAM); 
    if (g_recAudioRAM) free(g_recAudioRAM);
    
    CerrarGrabadoraContexto(&ctx); return resultado;
}
#endif