#ifndef CHROMA_H
#define CHROMA_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <camera/camera.h>
#include <vpad/input.h>
#include <coreinit/cache.h>
#include <malloc.h>
#include <unistd.h>
#include <string>
#include <stdlib.h> 
#include <netdb.h> 

#include "upload.h" 
#include "qrcodegen.hpp" 
using namespace qrcodegen;

#define CAM_WIDTH 640
#define CAM_HEIGHT 480
#define CAM_PITCH 768 

#ifndef CLAMP
#define CLAMP(v) (((v)>255)?255:(((v)<0)?0:(v)))
#endif

static int g_minU = -60; static int g_maxU = -10;
static int g_minV = -60; static int g_maxV = -10;
static int g_tolerancia = 80; 
static std::string g_lastError = "";

struct CtxChroma {
    CAMHandle handle; void* workMem;
    uint8_t* rawBuffer; uint8_t* refBuffer;
    uint32_t* cleanBuffer; uint32_t* bgBuffer;
    CAMSurface surface; bool exito; SDL_Texture* textura;
    bool tieneReferencia;   
};

static volatile bool chFrameListo = false;
static void CallbackChroma(CAMEventData *e) { if (e->eventType == CAMERA_DECODE_DONE) chFrameListo = true; }

std::string Chroma_GetLocalIP() {
    char hostbuffer[256]; gethostname(hostbuffer, sizeof(hostbuffer));
    struct hostent *host_entry = gethostbyname(hostbuffer);
    if (host_entry && host_entry->h_addr_list[0]) {
        return std::string(inet_ntoa(*(struct in_addr*)host_entry->h_addr_list[0]));
    }
    return "0.0.0.0";
}

SDL_Texture* GenQR(SDL_Renderer* renderer, std::string text) {
    QrCode qr = QrCode::encodeText(text.c_str(), QrCode::Ecc::LOW);
    int size = qr.getSize(); int scale = 6; int border = 2; int dim = (size+border*2)*scale;
    SDL_Surface* s = SDL_CreateRGBSurface(0, dim, dim, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 255,255,255));
    for(int y=0;y<size;y++) for(int x=0;x<size;x++) if(qr.getModule(x,y)) {
        SDL_Rect r={(x+border)*scale,(y+border)*scale,scale,scale}; SDL_FillRect(s,&r,0);
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); SDL_FreeSurface(s);
    return t;
}

// --- CARGAR FONDO (ARGB) ---
uint32_t* CargarFondoRaw(SDL_Renderer* ren, const char* ruta) {
    SDL_Surface* temp = IMG_Load(ruta); 
    if (!temp) { g_lastError = std::string(IMG_GetError()); return NULL; }

    SDL_Surface* opt = SDL_ConvertSurfaceFormat(temp, SDL_PIXELFORMAT_ARGB8888, 0); 
    SDL_FreeSurface(temp);
    if (!opt) { g_lastError = "Convert Failed"; return NULL; }

    SDL_Surface* fin = SDL_CreateRGBSurface(0, CAM_WIDTH, CAM_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_BlitScaled(opt, NULL, fin, NULL); SDL_FreeSurface(opt);

    uint32_t* buf = (uint32_t*)memalign(32, CAM_WIDTH * CAM_HEIGHT * 4);
    if (buf) memcpy(buf, fin->pixels, CAM_WIDTH * CAM_HEIGHT * 4);
    SDL_FreeSurface(fin); 
    return buf;
}

// --- PROCESAMIENTO ---
void ProcesarChromaVerde(CtxChroma* ctx) {
    int off=CAM_PITCH*CAM_HEIGHT; uint32_t* bg=ctx->bgBuffer;
    for (int y=0; y<CAM_HEIGHT; y+=2) {
        int fY1=y*CAM_PITCH; int fY2=(y+1)*CAM_PITCH; int fUV=off+((y/2)*CAM_PITCH); int fO1=y*CAM_WIDTH; int fO2=(y+1)*CAM_WIDTH;
        for (int x=0; x<CAM_WIDTH; x+=2) {
            int u=ctx->rawBuffer[fUV+x]-128; int v=ctx->rawBuffer[fUV+x+1]-128;
            bool green=(u>=g_minU && u<=g_maxU) && (v>=g_minV && v<=g_maxV);
            int idx[4]={fO1+x,fO1+x+1,fO2+x,fO2+x+1};
            int ys[4]={ctx->rawBuffer[fY1+x],ctx->rawBuffer[fY1+x+1],ctx->rawBuffer[fY2+x],ctx->rawBuffer[fY2+x+1]};
            for(int k=0;k<4;k++){
                if(green && bg) ctx->cleanBuffer[idx[k]] = bg[idx[k]];
                else if (green) ctx->cleanBuffer[idx[k]] = 0xFF000000;
                else {
                    int yv=ys[k]; int r=yv+((351*v)>>8); int g=yv-(((86*u)+(179*v))>>8); int b=yv+((444*u)>>8);
                    ctx->cleanBuffer[idx[k]]=(0xFF<<24)|(CLAMP(r)<<16)|(CLAMP(g)<<8)|CLAMP(b);
                }
            }
        }
    }
}

void ProcesarDifferenceKey(CtxChroma* ctx) {
    int off=CAM_PITCH*CAM_HEIGHT; uint32_t* bg=ctx->bgBuffer;
    if (!ctx->tieneReferencia) { ProcesarChromaVerde(ctx); return; } 
    for (int y=0; y<CAM_HEIGHT; y+=2) {
        int fY1=y*CAM_PITCH; int fY2=(y+1)*CAM_PITCH; int fUV=off+((y/2)*CAM_PITCH); int fO1=y*CAM_WIDTH; int fO2=(y+1)*CAM_WIDTH;
        for (int x=0; x<CAM_WIDTH; x+=2) {
            int iUV=fUV+x; int u=ctx->rawBuffer[iUV]-128; int v=ctx->rawBuffer[iUV+1]-128;
            int dU=abs((int)ctx->rawBuffer[iUV]-(int)ctx->refBuffer[iUV])*2;
            int dV=abs((int)ctx->rawBuffer[iUV+1]-(int)ctx->refBuffer[iUV+1])*2;
            int dC=dU+dV;
            int idx[4]={fO1+x,fO1+x+1,fO2+x,fO2+x+1};
            int iY[4]={fY1+x,fY1+x+1,fY2+x,fY2+x+1};
            for(int k=0;k<4;k++){
                int dY=abs((int)ctx->rawBuffer[iY[k]]-(int)ctx->refBuffer[iY[k]]);
                if((dY+dC < g_tolerancia) && bg) ctx->cleanBuffer[idx[k]] = bg[idx[k]];
                else if (dY+dC < g_tolerancia) ctx->cleanBuffer[idx[k]] = 0xFF000000;
                else {
                    int yv=ctx->rawBuffer[iY[k]]; int r=yv+((351*v)>>8); int g=yv-(((86*u)+(179*v))>>8); int b=yv+((444*u)>>8);
                    ctx->cleanBuffer[idx[k]]=(0xFF<<24)|(CLAMP(r)<<16)|(CLAMP(g)<<8)|CLAMP(b);
                }
            }
        }
    }
}

void CerrarChroma(CtxChroma* ctx) {
    if (ctx->exito) { CAMClose(ctx->handle); CAMExit(ctx->handle); }
    if (ctx->textura) SDL_DestroyTexture(ctx->textura);
    if (ctx->rawBuffer) free(ctx->rawBuffer); if (ctx->refBuffer) free(ctx->refBuffer);
    if (ctx->cleanBuffer) free(ctx->cleanBuffer); if (ctx->bgBuffer) free(ctx->bgBuffer);
    if (ctx->workMem) free(ctx->workMem); ctx->exito=false;
}

void EjecutarChroma(SDL_Renderer* ren, TTF_Font* font, std::string rootPath) {
    CtxChroma ctx; memset(&ctx, 0, sizeof(ctx));
    std::string bgPath = rootPath + "/chroma_bg.png";
    std::string tempBgPath = "fs:/vol/external01/WiiUCamera Files/custom_bg.png"; 
    
    ctx.bgBuffer = CargarFondoRaw(ren, tempBgPath.c_str());
    if(!ctx.bgBuffer) ctx.bgBuffer = CargarFondoRaw(ren, bgPath.c_str());
    if(!ctx.bgBuffer) {
        ctx.bgBuffer = (uint32_t*)memalign(32, CAM_WIDTH*CAM_HEIGHT*4);
        for(int i=0; i<CAM_WIDTH*CAM_HEIGHT; i++) ctx.bgBuffer[i] = 0xFF0000FF; 
    }

    CAMStreamInfo info; info.type = CAMERA_STREAM_TYPE_1; info.width = CAM_WIDTH; info.height = CAM_HEIGHT;
    int memSize = CAMGetMemReq(&info); ctx.workMem = memalign(256, memSize);
    CAMSetupInfo setup; memset(&setup, 0, sizeof(setup));
    setup.streamInfo = info; setup.workMem.pMem = ctx.workMem; setup.workMem.size = memSize;
    setup.eventHandler = CallbackChroma; setup.mode.fps = CAMERA_FPS_30;
    
    int err=0; ctx.handle = CAMInit(0, &setup, &err);
    if (err == CAMERA_ERROR_OK) {
        ctx.exito = true; CAMOpen(ctx.handle);
        ctx.rawBuffer = (uint8_t*)memalign(256, CAMERA_YUV_BUFFER_SIZE);
        ctx.refBuffer = (uint8_t*)memalign(256, CAMERA_YUV_BUFFER_SIZE);
        ctx.cleanBuffer = (uint32_t*)memalign(256, CAM_WIDTH * CAM_HEIGHT * 4);
        memset(&ctx.surface, 0, sizeof(CAMSurface));
        ctx.surface.width = CAM_WIDTH; ctx.surface.height = CAM_HEIGHT; ctx.surface.pitch = CAM_PITCH;
        ctx.surface.alignment = CAMERA_YUV_BUFFER_ALIGNMENT; ctx.surface.surfaceSize = CAMERA_YUV_BUFFER_SIZE; ctx.surface.surfaceBuffer = ctx.rawBuffer;
        CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        ctx.textura = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, CAM_WIDTH, CAM_HEIGHT);
    } else { if(ctx.workMem) free(ctx.workMem); return; }

    bool running = true; chFrameListo = false;
    int modo = 0; bool modoUpload = false; 
    SDL_Texture* qrTex = NULL; SDL_Rect rCam = {160, 0, 960, 720}; 
    std::string statusMsg = "";

    while (running) {
        SDL_Event ev; while (SDL_PollEvent(&ev)) if (ev.type == SDL_QUIT) running = false;
        VPADStatus vpad; VPADRead(VPAD_CHAN_0, &vpad, 1, NULL);
        if (vpad.trigger & VPAD_BUTTON_B) {
            if (modoUpload) { modoUpload=false; Up_CerrarServidor(); if(qrTex) SDL_DestroyTexture(qrTex); qrTex=NULL; }
            else running = false;
        }

        if (modoUpload) {
            SDL_SetRenderDrawColor(ren, 10, 10, 20, 255); SDL_RenderClear(ren);
            int res = Up_Procesar(tempBgPath.c_str());
            if (res == 1) { 
                statusMsg = "Image received. Processing...";
                // RECARGA DE FONDO
                if(ctx.bgBuffer) free(ctx.bgBuffer);
                ctx.bgBuffer = NULL;
                
                // Intento 1: Cargar la nueva
                ctx.bgBuffer = CargarFondoRaw(ren, tempBgPath.c_str()); 
                
                if(ctx.bgBuffer) statusMsg = "SUCCESS! New Background Set.";
                else {
                    // Fallo: Cargar la default
                    ctx.bgBuffer = CargarFondoRaw(ren, bgPath.c_str());
                    statusMsg = "ERROR: " + g_lastError;
                }
                
                modoUpload = false; Up_CerrarServidor(); if(qrTex) SDL_DestroyTexture(qrTex); qrTex=NULL;
                SDL_Delay(1000); 
            }
            
            if(qrTex) { SDL_Rect rQ = {(1280-250)/2, 150, 250, 250}; SDL_RenderCopy(ren, qrTex, NULL, &rQ); }
            SDL_Surface* s = TTF_RenderText_Blended(font, "Scan & Upload Image", {0,255,255,255});
            SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s); SDL_Rect r = {(1280-s->w)/2, 80, s->w, s->h};
            SDL_RenderCopy(ren, t, NULL, &r); SDL_FreeSurface(s); SDL_DestroyTexture(t);
            
            s = TTF_RenderText_Blended(font, "Connect to Wi-Fi", {150,150,150,255});
            t = SDL_CreateTextureFromSurface(ren, s); SDL_Rect r2 = {(1280-s->w)/2, 450, s->w, s->h};
            SDL_RenderCopy(ren, t, NULL, &r2); SDL_FreeSurface(s); SDL_DestroyTexture(t);
            SDL_RenderPresent(ren); continue; 
        }

        if (vpad.trigger & VPAD_BUTTON_MINUS) { modo = !modo; ctx.tieneReferencia = false; }
        if (vpad.trigger & VPAD_BUTTON_PLUS) {
            modoUpload = true;
            std::string ip = Chroma_GetLocalIP(); std::string url = "http://" + ip + ":8080";
            qrTex = GenQR(ren, url); Up_IniciarServidor();
        }

        if (modo == 0) {
            if (vpad.hold & VPAD_BUTTON_UP) g_maxU++; if (vpad.hold & VPAD_BUTTON_DOWN) g_maxU--;
            if (vpad.hold & VPAD_BUTTON_RIGHT) g_minU++; if (vpad.hold & VPAD_BUTTON_LEFT) g_minU--;
            if (g_maxU > 0) g_maxU = 0; if (g_minU < -120) g_minU = -120;
        } else {
            if (vpad.trigger & VPAD_BUTTON_A) { memcpy(ctx.refBuffer, ctx.rawBuffer, CAMERA_YUV_BUFFER_SIZE); ctx.tieneReferencia = true; }
            if (vpad.hold & VPAD_BUTTON_UP) g_tolerancia+=2; if (vpad.hold & VPAD_BUTTON_DOWN) g_tolerancia-=2;
            if (g_tolerancia<10) g_tolerancia=10;
        }

        if (ctx.exito && chFrameListo) {
            DCInvalidateRange(ctx.rawBuffer, CAMERA_YUV_BUFFER_SIZE);
            if (modo == 0) ProcesarChromaVerde(&ctx); else ProcesarDifferenceKey(&ctx);
            SDL_UpdateTexture(ctx.textura, NULL, ctx.cleanBuffer, CAM_WIDTH * 4);
            chFrameListo = false; CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        }

        SDL_SetRenderDrawColor(ren, 20, 20, 20, 255); SDL_RenderClear(ren);
        if (ctx.textura) SDL_RenderCopy(ren, ctx.textura, NULL, &rCam);

        char buf[128];
        if (modo == 0) sprintf(buf, "GREEN SCREEN | U:%d-%d | (+) Upload BG", g_minU, g_maxU);
        else sprintf(buf, "MAGIC BG (Tol:%d) | (+) Upload BG", g_tolerancia);
        
        SDL_Surface* s = TTF_RenderText_Blended(font, buf, {0,255,0,255});
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s); SDL_Rect r = {(1280-s->w)/2, 20, s->w, s->h};
            SDL_RenderCopy(ren, t, NULL, &r); SDL_FreeSurface(s); SDL_DestroyTexture(t);
        }
        
        if(statusMsg.length() > 0) {
             SDL_Color cMsg = {255, 255, 0, 255};
             if(statusMsg.find("ERROR") != std::string::npos) cMsg = {255, 0, 0, 255};
             if(statusMsg.find("SUCCESS") != std::string::npos) cMsg = {0, 255, 0, 255};
             
             s = TTF_RenderText_Blended(font, statusMsg.c_str(), cMsg);
             SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s); SDL_Rect r = {(1280-s->w)/2, 60, s->w, s->h};
             SDL_RenderCopy(ren, t, NULL, &r); SDL_FreeSurface(s); SDL_DestroyTexture(t);
        }

        s = TTF_RenderText_Blended(font, modo==0?"(-) Go Magic":"(-) Go Green", {200,200,200,255});
        SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s); SDL_Rect r2 = {20, 680, s->w, s->h};
        SDL_RenderCopy(ren, t, NULL, &r2); SDL_FreeSurface(s); SDL_DestroyTexture(t);

        SDL_RenderPresent(ren);
    }
    CerrarChroma(&ctx);
}

#endif