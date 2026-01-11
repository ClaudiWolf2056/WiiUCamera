#ifndef CAMERA_EFFECTS_H
#define CAMERA_EFFECTS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <camera/camera.h>
#include <vpad/input.h>
#include <coreinit/cache.h>
#include <malloc.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#define CAM_WIDTH 640
#define CAM_HEIGHT 480
#define CAM_PITCH 768 

// Enum de efectos disponibles
enum TipoEfecto {
    EFECTO_NORMAL,
    EFECTO_GRAYSCALE,
    EFECTO_SEPIA,
    EFECTO_GAMEBOY,
    EFECTO_NEGATIVO,
    EFECTO_TOTAL // Para saber cuantos hay
};

struct ContextoCamaraFx {
    CAMHandle handle;
    void* workMem;
    uint8_t* rawBuffer;
    uint32_t* cleanBuffer;
    CAMSurface surface;
    bool exito;
    SDL_Texture* textura;
    int efectoActual;
};

static volatile bool camFxFrameListo = false;

static void CallbackCamaraFx(CAMEventData *evento) {
    if (evento->eventType == CAMERA_DECODE_DONE) camFxFrameListo = true;
}

#ifndef CLAMP
#define CLAMP(v) (((v)>255)?255:(((v)<0)?0:(v)))
#endif

// --- UTILIDADES ---
void EscribirShortLE_Fx(FILE* f, uint16_t valor) {
    uint16_t le = (valor >> 8) | (valor << 8);
    fwrite(&le, 2, 1, f);
}
void EscribirIntLE_Fx(FILE* f, uint32_t valor) {
    uint32_t le = ((valor >> 24) & 0xFF) | ((valor >> 8) & 0xFF00) | ((valor << 8) & 0xFF0000) | ((valor << 24) & 0xFF000000);
    fwrite(&le, 4, 1, f);
}

// Lógica de píxeles para cada efecto
void ProcesarFrameEfectos(ContextoCamaraFx* ctx) {
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
                
                // APLICAR EFECTOS
                if (ctx->efectoActual == EFECTO_GRAYSCALE) {
                    int gray = (r * 0.3 + g * 0.59 + b * 0.11);
                    r = g = b = gray;
                } 
                else if (ctx->efectoActual == EFECTO_SEPIA) {
                    int tr = (r * 0.393) + (g * 0.769) + (b * 0.189);
                    int tg = (r * 0.349) + (g * 0.686) + (b * 0.168);
                    int tb = (r * 0.272) + (g * 0.534) + (b * 0.131);
                    r = CLAMP(tr); g = CLAMP(tg); b = CLAMP(tb);
                }
                else if (ctx->efectoActual == EFECTO_GAMEBOY) {
                    int gray = (r * 0.3 + g * 0.59 + b * 0.11);
                    // Mapear a paleta verde (aprox)
                    r = 0; b = 0;
                    if (gray < 64) { g = 15; }
                    else if (gray < 128) { g = 70; }
                    else if (gray < 192) { g = 140; }
                    else { g = 210; }
                    // Un toque verde-oscuro matrix
                    r = 0; b = 20; 
                }
                else if (ctx->efectoActual == EFECTO_NEGATIVO) {
                    r = 255 - r;
                    g = 255 - g;
                    b = 255 - b;
                }

                ctx->cleanBuffer[outIndices[k]] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
            }
        }
    }
}

void GuardarFotoFxRapido(SDL_Renderer* renderer, TTF_Font* font, ContextoCamaraFx* ctx) {
    SDL_Color col = {255, 255, 0, 255};
    SDL_Surface* sMsg = TTF_RenderText_Blended(font, "GUARDANDO FX... / SAVING FX...", col);
    if(sMsg) {
        SDL_Texture* tMsg = SDL_CreateTextureFromSurface(renderer, sMsg);
        SDL_Rect rMsg = { (1280 - sMsg->w)/2, 300, sMsg->w, sMsg->h };
        SDL_RenderCopy(renderer, tMsg, NULL, &rMsg);
        SDL_RenderPresent(renderer); 
        SDL_FreeSurface(sMsg); SDL_DestroyTexture(tMsg);
    }

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char nombre[100];
    sprintf(nombre, "fs:/vol/external01/WiiUCamera Files/FX_%02d%02d%02d_%02d%02d%02d.bmp", 
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    FILE* f = fopen(nombre, "wb");
    if (f) {
        uint32_t width = CAM_WIDTH;
        uint32_t height = CAM_HEIGHT;
        uint32_t imageSize = width * height * 4;
        uint32_t fileSize = 54 + imageSize;

        fwrite("BM", 1, 2, f);           
        EscribirIntLE_Fx(f, fileSize);  
        EscribirIntLE_Fx(f, 0);         
        EscribirIntLE_Fx(f, 54);        

        EscribirIntLE_Fx(f, 40);        
        EscribirIntLE_Fx(f, width);     
        EscribirIntLE_Fx(f, -((int)height)); 
        EscribirShortLE_Fx(f, 1);       
        EscribirShortLE_Fx(f, 32);      
        EscribirIntLE_Fx(f, 0);         
        EscribirIntLE_Fx(f, imageSize); 
        EscribirIntLE_Fx(f, 2835);      
        EscribirIntLE_Fx(f, 2835);      
        EscribirIntLE_Fx(f, 0);         
        EscribirIntLE_Fx(f, 0);         

        uint32_t* tempBuffer = (uint32_t*)malloc(imageSize);
        if (tempBuffer) {
            for (int i = 0; i < (width * height); i++) {
                uint32_t pixel = ctx->cleanBuffer[i];
                uint32_t r = (pixel >> 24) & 0xFF;
                uint32_t g = (pixel >> 16) & 0xFF;
                uint32_t b = (pixel >> 8) & 0xFF;
                uint32_t a = pixel & 0xFF;
                tempBuffer[i] = (b << 24) | (g << 16) | (r << 8) | a;
            }
            fwrite(tempBuffer, 1, imageSize, f);
            free(tempBuffer);
        }
        fclose(f);
    }
}

ContextoCamaraFx IniciarCamaraFxContexto(SDL_Renderer* renderer) {
    ContextoCamaraFx ctx;
    memset(&ctx, 0, sizeof(ContextoCamaraFx));
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
    setup.eventHandler = CallbackCamaraFx;
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
        ctx.efectoActual = EFECTO_NORMAL;
    }
    return ctx;
}

void CerrarCamaraFxContexto(ContextoCamaraFx* ctx) {
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

int EjecutarCamaraEfectos(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    ContextoCamaraFx ctx = IniciarCamaraFxContexto(renderer);
    if (!ctx.exito) { CerrarCamaraFxContexto(&ctx); return 0; }

    bool enCamara = true;
    int resultado = 0;
    SDL_Rect rectCamara = {0, 0, 960, 720}; 
    camFxFrameListo = false;
    int frameFlash = 0;
    int delayCambio = 0;

    // Nombres de efectos para mostrar
    const char* nombresFxEN[] = {"Normal", "Grayscale", "Sepia", "GameBoy", "Negative"};
    const char* nombresFxES[] = {"Normal", "Escala Grises", "Sepia", "GameBoy", "Negativo"};

    while (enCamara) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
             if (event.type == SDL_QUIT) { enCamara = false; resultado = -1; }
        }
        VPADStatus vpad; VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (vpad.trigger & VPAD_BUTTON_B) { enCamara = false; resultado = 1; }
        
        if (vpad.trigger & VPAD_BUTTON_A) {
            GuardarFotoFxRapido(renderer, font, &ctx);
            frameFlash = 5; 
        }

        if (delayCambio > 0) delayCambio--;
        if (delayCambio == 0) {
            if (vpad.hold & VPAD_BUTTON_RIGHT) {
                ctx.efectoActual++;
                if (ctx.efectoActual >= EFECTO_TOTAL) ctx.efectoActual = 0;
                delayCambio = 15;
            }
            if (vpad.hold & VPAD_BUTTON_LEFT) {
                ctx.efectoActual--;
                if (ctx.efectoActual < 0) ctx.efectoActual = EFECTO_TOTAL - 1;
                delayCambio = 15;
            }
        }

        if (ctx.exito && camFxFrameListo) {
            DCInvalidateRange(ctx.rawBuffer, CAMERA_YUV_BUFFER_SIZE);
            ProcesarFrameEfectos(&ctx);
            SDL_UpdateTexture(ctx.textura, NULL, ctx.cleanBuffer, CAM_WIDTH * 4);
            camFxFrameListo = false;
            CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (ctx.textura) SDL_RenderCopy(renderer, ctx.textura, NULL, &rectCamara);

        SDL_Color col = {255, 255, 255, 255};
        
        // --- UI ACTUALIZADA (Texto Lateral) ---
        // Nombre del efecto actual
        const char* nombreFx = esIngles ? nombresFxEN[ctx.efectoActual] : nombresFxES[ctx.efectoActual];
        
        if (esIngles) {
             SDL_Surface* sT = TTF_RenderText_Blended(font, "Mode: Effects", col);
             SDL_Surface* sN = TTF_RenderText_Blended(font, nombreFx, {255, 255, 0, 255}); // Amarillo
             SDL_Surface* s1 = TTF_RenderText_Blended(font, "(< >) Change FX", col);
             SDL_Surface* s2 = TTF_RenderText_Blended(font, "(A) Take Photo", col);
             SDL_Surface* s3 = TTF_RenderText_Blended(font, "(B) Exit Mode", col);
             
             if(sT){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sT); SDL_Rect r={980,50,sT->w,sT->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sT); SDL_DestroyTexture(t); }
             if(sN){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sN); SDL_Rect r={980,90,sN->w,sN->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sN); SDL_DestroyTexture(t); }
             
             if(s1){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s1); SDL_Rect r={980,150,s1->w,s1->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s1); SDL_DestroyTexture(t); }
             if(s2){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s2); SDL_Rect r={980,200,s2->w,s2->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s2); SDL_DestroyTexture(t); }
             if(s3){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s3); SDL_Rect r={980,250,s3->w,s3->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s3); SDL_DestroyTexture(t); }
        } else {
             SDL_Surface* sT = TTF_RenderText_Blended(font, "Modo: Efectos", col);
             SDL_Surface* sN = TTF_RenderText_Blended(font, nombreFx, {255, 255, 0, 255});
             SDL_Surface* s1 = TTF_RenderText_Blended(font, "(< >) Cambiar FX", col);
             SDL_Surface* s2 = TTF_RenderText_Blended(font, "(A) Tomar Foto", col);
             SDL_Surface* s3 = TTF_RenderText_Blended(font, "(B) Salir Modo", col);

             if(sT){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sT); SDL_Rect r={980,50,sT->w,sT->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sT); SDL_DestroyTexture(t); }
             if(sN){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sN); SDL_Rect r={980,90,sN->w,sN->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sN); SDL_DestroyTexture(t); }

             if(s1){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s1); SDL_Rect r={980,150,s1->w,s1->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s1); SDL_DestroyTexture(t); }
             if(s2){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s2); SDL_Rect r={980,200,s2->w,s2->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s2); SDL_DestroyTexture(t); }
             if(s3){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s3); SDL_Rect r={980,250,s3->w,s3->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s3); SDL_DestroyTexture(t); }
        }

        if (frameFlash > 0) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 150); 
            SDL_RenderFillRect(renderer, NULL); 
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            frameFlash--;
        }
        SDL_RenderPresent(renderer);
    }
    CerrarCamaraFxContexto(&ctx);
    return resultado;
}

#endif