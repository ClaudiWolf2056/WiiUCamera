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

enum TipoEfecto {
    FX_NORMAL = 0, FX_GRAYSCALE, FX_SEPIA, FX_NEGATIVE, FX_GAMEBOY, FX_TOTAL
};

struct ContextoEfectos {
    CAMHandle handle;
    void* workMem;
    uint8_t* rawBuffer;
    uint32_t* cleanBuffer;
    CAMSurface surface;
    bool exito;
    SDL_Texture* textura;
};

static volatile bool fxFrameListo = false;
static void CallbackEfectos(CAMEventData *evento) {
    if (evento->eventType == CAMERA_DECODE_DONE) fxFrameListo = true;
}

#ifndef CLAMP
#define CLAMP(v) (((v)>255)?255:(((v)<0)?0:(v)))
#endif

void ProcesarFrameConEfecto(ContextoEfectos* ctx, int efectoActual) {
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

                if (efectoActual == FX_GRAYSCALE) { int gr = (r+g+b)/3; r=gr; g=gr; b=gr; } 
                else if (efectoActual == FX_SEPIA) {
                    int tr = (r * 0.393) + (g * 0.769) + (b * 0.189);
                    int tg = (r * 0.349) + (g * 0.686) + (b * 0.168);
                    int tb = (r * 0.272) + (g * 0.534) + (b * 0.131);
                    r = CLAMP(tr); g = CLAMP(tg); b = CLAMP(tb);
                }
                else if (efectoActual == FX_NEGATIVE) { r = 255 - r; g = 255 - g; b = 255 - b; }
                else if (efectoActual == FX_GAMEBOY) {
                    int gr = (r + g + b) / 3;
                    if (gr < 64) { r=15; g=56; b=15; } else if (gr < 128){ r=48; g=98; b=48; } 
                    else if (gr < 192){ r=139; g=172; b=15; } else { r=155; g=188; b=15; } 
                }
                ctx->cleanBuffer[outIndices[k]] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
            }
        }
    }
}

// FIX: Añadimos feedback visual y pausa para asegurar guardado
void GuardarFotoConEfecto(SDL_Renderer* renderer, TTF_Font* font, ContextoEfectos* ctx, int efecto) {
    // 1. Mostrar mensaje de guardado
    SDL_Color col = {255, 255, 0, 255};
    SDL_Surface* sMsg = TTF_RenderText_Blended(font, "GUARDANDO FOTO... / SAVING...", col);
    if(sMsg) {
        SDL_Texture* tMsg = SDL_CreateTextureFromSurface(renderer, sMsg);
        SDL_Rect rMsg = { (1280 - sMsg->w)/2, 300, sMsg->w, sMsg->h };
        SDL_RenderCopy(renderer, tMsg, NULL, &rMsg);
        SDL_RenderPresent(renderer); // Forzar que se vea en pantalla
        SDL_FreeSurface(sMsg); SDL_DestroyTexture(tMsg);
    }

    // 2. Crear carpeta y Guardar
    mkdir("fs:/vol/external01/WiiUCamera Files", 0777);
    
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char nombre[100];
    
    const char* sufijo = "";
    if (efecto == FX_GRAYSCALE) sufijo = "_BW";
    else if (efecto == FX_SEPIA) sufijo = "_SEPIA";
    else if (efecto == FX_NEGATIVE) sufijo = "_NEG";
    else if (efecto == FX_GAMEBOY) sufijo = "_RETRO";

    sprintf(nombre, "fs:/vol/external01/WiiUCamera Files/F_%02d%02d%02d_%02d%02d%02d%s.bmp", 
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
            tm->tm_hour, tm->tm_min, tm->tm_sec, sufijo);

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        ctx->cleanBuffer, CAM_WIDTH, CAM_HEIGHT, 32, CAM_WIDTH * 4,
        0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
    );

    if (surface) {
        SDL_SaveBMP(surface, nombre);
        SDL_FreeSurface(surface);
    }
    
    // 3. Pequeña pausa para que el usuario vea que se guardó
    usleep(500000); // 0.5 segundos
}

ContextoEfectos IniciarEfectosContexto(SDL_Renderer* renderer) {
    ContextoEfectos ctx;
    memset(&ctx, 0, sizeof(ContextoEfectos));
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
    setup.eventHandler = CallbackEfectos;
    setup.mode.fps = CAMERA_FPS_30;

    ctx.handle = CAMInit(0, &setup, &err);
    if (err == CAMERA_ERROR_OK) {
        ctx.exito = true;
        CAMOpen(ctx.handle);
        ctx.rawBuffer = (uint8_t*)memalign(256, CAMERA_YUV_BUFFER_SIZE);
        ctx.cleanBuffer = (uint32_t*)memalign(256, CAM_WIDTH * CAM_HEIGHT * 4);
        
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

void CerrarEfectosContexto(ContextoEfectos* ctx) {
    if (ctx->exito) { CAMClose(ctx->handle); CAMExit(ctx->handle); }
    if (ctx->textura) SDL_DestroyTexture(ctx->textura);
    if (ctx->rawBuffer) free(ctx->rawBuffer);
    if (ctx->cleanBuffer) free(ctx->cleanBuffer);
    if (ctx->workMem) free(ctx->workMem);
}

int EjecutarCamaraEfectos(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    ContextoEfectos ctx = IniciarEfectosContexto(renderer);
    if (!ctx.exito) { CerrarEfectosContexto(&ctx); return 0; }

    bool enCamara = true;
    int resultado = 0;
    int efectoActual = FX_NORMAL;
    const char* nombresES[] = { "Normal", "Blanco y Negro", "Sepia", "Negativo", "Retro GameBoy" };
    const char* nombresEN[] = { "Normal", "Grayscale", "Sepia", "Negative", "Retro GameBoy" };
    SDL_Rect rectCamara = {0, 0, 960, 720}; 
    fxFrameListo = false;
    int delayCambio = 0;

    while (enCamara) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
             if (event.type == SDL_QUIT) { enCamara = false; resultado = -1; }
        }

        VPADStatus vpad; VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
        if (delayCambio > 0) delayCambio--;

        if (vpad.trigger & VPAD_BUTTON_B) { enCamara = false; resultado = 1; }
        
        // MODIFICACIÓN: Pasamos renderer y font para el mensaje de guardado
        if (vpad.trigger & VPAD_BUTTON_A) {
            GuardarFotoConEfecto(renderer, font, &ctx, efectoActual);
        }

        if (delayCambio == 0) {
            if (vpad.hold & VPAD_BUTTON_RIGHT) { efectoActual++; if (efectoActual >= FX_TOTAL) efectoActual = 0; delayCambio = 15; }
            if (vpad.hold & VPAD_BUTTON_LEFT) { efectoActual--; if (efectoActual < 0) efectoActual = FX_TOTAL - 1; delayCambio = 15; }
        }

        if (ctx.exito && fxFrameListo) {
            DCInvalidateRange(ctx.rawBuffer, CAMERA_YUV_BUFFER_SIZE);
            ProcesarFrameConEfecto(&ctx, efectoActual);
            SDL_UpdateTexture(ctx.textura, NULL, ctx.cleanBuffer, CAM_WIDTH * 4);
            fxFrameListo = false;
            CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (ctx.textura) SDL_RenderCopy(renderer, ctx.textura, NULL, &rectCamara);

        SDL_Color col = {255, 255, 255, 255};
        const char* nombreFx = esIngles ? nombresEN[efectoActual] : nombresES[efectoActual];
        SDL_Surface* sFx = TTF_RenderText_Blended(font, nombreFx, col);
        SDL_Texture* tFx = SDL_CreateTextureFromSurface(renderer, sFx);
        SDL_Rect rFx = {980, 50, sFx->w, sFx->h};
        SDL_RenderCopy(renderer, tFx, NULL, &rFx);
        SDL_FreeSurface(sFx); SDL_DestroyTexture(tFx);

        SDL_Surface* sArr = TTF_RenderText_Blended(font, "<  D-Pad  >", col);
        SDL_Texture* tArr = SDL_CreateTextureFromSurface(renderer, sArr);
        SDL_Rect rArr = {980, 100, sArr->w, sArr->h};
        SDL_RenderCopy(renderer, tArr, NULL, &rArr);
        SDL_FreeSurface(sArr); SDL_DestroyTexture(tArr);
        
        SDL_Surface* sA = TTF_RenderText_Blended(font, esIngles ? "(A) Take Photo" : "(A) Tomar Foto", col);
        SDL_Texture* tA = SDL_CreateTextureFromSurface(renderer, sA);
        SDL_Rect rA = {980, 200, sA->w, sA->h};
        SDL_RenderCopy(renderer, tA, NULL, &rA);
        SDL_FreeSurface(sA); SDL_DestroyTexture(tA);

        SDL_RenderPresent(renderer);
    }
    CerrarEfectosContexto(&ctx);
    return resultado;
}

#endif