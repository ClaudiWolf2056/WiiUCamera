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
#include <string>
#include <math.h> 

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
    EFECTO_BOCETO,    // (Nuevo) Sketch / Edge Detect
    EFECTO_TERMICA,   
    EFECTO_GLITCH,    
    EFECTO_SOLAR,     
    EFECTO_ESPEJO,    
    EFECTO_PIXEL,     // (Nuevo) Mosaico 8-bit
    EFECTO_ONDAS,     // (Nuevo) Wavy / Borracho
    EFECTO_TOTAL      
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

// Lógica de píxeles
void ProcesarFrameEfectos(ContextoCamaraFx* ctx) {
    int offsetUV = CAM_PITCH * CAM_HEIGHT;
    
    // Variable de tiempo para efectos animados (Ondas)
    static float timeVal = 0.0f;
    timeVal += 0.2f; 

    for (int y = 0; y < CAM_HEIGHT; y += 2) {
        
        // --- CALCULO PARA EFECTO ONDAS ---
        int waveOffset = 0;
        if (ctx->efectoActual == EFECTO_ONDAS) {
            // Desplazamiento sinusoidal basado en la linea Y y el tiempo
            waveOffset = (int)(sin(y * 0.05f + timeVal) * 10.0f);
        }

        int fY1 = y * CAM_PITCH; int fY2 = (y + 1) * CAM_PITCH;
        int fUV = offsetUV + ((y / 2) * CAM_PITCH);
        int fOut1 = y * CAM_WIDTH; int fOut2 = (y + 1) * CAM_WIDTH;

        for (int x = 0; x < CAM_WIDTH; x += 2) {
            
            // --- MANIPULACION DE COORDENADAS DE LECTURA ---
            int readX = x;
            int readY1 = fY1; 
            int readY2 = fY2;
            int readUV_idx = fUV + x;

            // 1. ESPEJO
            if (ctx->efectoActual == EFECTO_ESPEJO) {
                if (x >= CAM_WIDTH / 2) {
                    readX = (CAM_WIDTH - 2) - x; 
                    readUV_idx = fUV + readX;
                    if (readUV_idx % 2 != 0) readUV_idx--; 
                }
            }
            // 2. PIXEL ART (MOSAICO)
            else if (ctx->efectoActual == EFECTO_PIXEL) {
                // Forzar lectura cada 8 pixeles
                int blockSize = 8;
                int pX = (x / blockSize) * blockSize;
                int pY = (y / blockSize) * blockSize;
                
                readX = pX;
                readY1 = pY * CAM_PITCH; // Sobreescribimos linea Y
                readY2 = pY * CAM_PITCH; 
                readUV_idx = offsetUV + ((pY / 2) * CAM_PITCH) + pX;
            }
            // 3. ONDAS
            else if (ctx->efectoActual == EFECTO_ONDAS) {
                readX = x + waveOffset;
                // Clamp para no salirnos de la imagen
                if(readX < 0) readX = 0;
                if(readX >= CAM_WIDTH - 2) readX = CAM_WIDTH - 2;
                // Asegurar paridad para UV
                if(readX % 2 != 0) readX--;
                readUV_idx = fUV + readX;
            }

            // LEER DATOS YUV
            int u = ctx->rawBuffer[readUV_idx] - 128; 
            int v = ctx->rawBuffer[readUV_idx + 1] - 128;
            
            // Si es PIXEL ART, usamos el mismo Y para todo el bloque 2x2 local
            // Si es NORMAL, leemos los 4 pixeles reales
            int ySamples[4];
            if (ctx->efectoActual == EFECTO_PIXEL) {
                int pixelVal = ctx->rawBuffer[readY1 + readX];
                ySamples[0]=pixelVal; ySamples[1]=pixelVal; ySamples[2]=pixelVal; ySamples[3]=pixelVal;
            } else {
                ySamples[0] = ctx->rawBuffer[readY1 + readX];
                ySamples[1] = ctx->rawBuffer[readY1 + readX + 1];
                ySamples[2] = ctx->rawBuffer[readY2 + readX];
                ySamples[3] = ctx->rawBuffer[readY2 + readX + 1];
            }
            
            int outIndices[4] = { fOut1 + x, fOut1 + x + 1, fOut2 + x, fOut2 + x + 1 };

            // --- PRE-CALCULO GLITCH ---
            int yGlitchR[4], yGlitchB[4];
            if (ctx->efectoActual == EFECTO_GLITCH) {
                int shift = 10;
                for(int k=0; k<4; k++) {
                    yGlitchR[k] = ySamples[k]; yGlitchB[k] = ySamples[k];
                    if (readX + shift < CAM_WIDTH) yGlitchR[k] = ctx->rawBuffer[readY1 + readX + shift]; 
                    if (readX - shift > 0)         yGlitchB[k] = ctx->rawBuffer[readY1 + readX - shift];
                }
            }

            // --- PRE-CALCULO BOCETO (EDGE DETECT) ---
            bool isEdge[4] = {false, false, false, false};
            if (ctx->efectoActual == EFECTO_BOCETO) {
                // Comparamos el brillo del pixel actual con el siguiente (+2 para saltar chroma)
                // Es un detector de bordes horizontal muy rapido
                for(int k=0; k<4; k++) {
                    int nextY = ySamples[k]; 
                    // Intentamos leer el siguiente pixel si no es borde de pantalla
                    if (readX + 2 < CAM_WIDTH) nextY = ctx->rawBuffer[readY1 + readX + 2];
                    
                    if (abs(ySamples[k] - nextY) > 20) isEdge[k] = true; // Umbral de borde
                }
            }

            for (int k = 0; k < 4; k++) {
                int r, g, b;

                if (ctx->efectoActual == EFECTO_BOCETO) {
                    // Si es borde -> Negro, Si no -> Blanco
                    int val = isEdge[k] ? 0 : 255;
                    r = val; g = val; b = val;
                }
                else {
                    // CONVERSION ESTANDAR
                    int yVal = ySamples[k];
                    int cR, cG, cB;

                    if (ctx->efectoActual == EFECTO_GLITCH) {
                        cR = yGlitchR[k] + ((351 * v) >> 8);
                        cG = yVal - (((86 * u) + (179 * v)) >> 8);
                        cB = yGlitchB[k] + ((444 * u) >> 8);
                    } else {
                        cR = yVal + ((351 * v) >> 8);
                        cG = yVal - (((86 * u) + (179 * v)) >> 8);
                        cB = yVal + ((444 * u) >> 8);
                    }
                    r = CLAMP(cR); g = CLAMP(cG); b = CLAMP(cB);
                }
                
                // --- POST-PROCESO RGB ---
                if (ctx->efectoActual == EFECTO_GRAYSCALE) {
                    int gray = (r * 30 + g * 59 + b * 11) / 100;
                    r = g = b = gray;
                } 
                else if (ctx->efectoActual == EFECTO_SEPIA) {
                    int tr = (r * 0.393) + (g * 0.769) + (b * 0.189);
                    int tg = (r * 0.349) + (g * 0.686) + (b * 0.168);
                    int tb = (r * 0.272) + (g * 0.534) + (b * 0.131);
                    r = CLAMP(tr); g = CLAMP(tg); b = CLAMP(tb);
                }
                else if (ctx->efectoActual == EFECTO_GAMEBOY) {
                    int gray = (r * 30 + g * 59 + b * 11) / 100;
                    if (gray < 64)       { r=15;  g=56;  b=15; }
                    else if (gray < 128) { r=48;  g=98;  b=48; }
                    else if (gray < 192) { r=139; g=172; b=15; }
                    else                 { r=155; g=188; b=15; }
                }
                else if (ctx->efectoActual == EFECTO_NEGATIVO) {
                    r = 255 - r; g = 255 - g; b = 255 - b;
                }
                else if (ctx->efectoActual == EFECTO_TERMICA) {
                    int gray = (r + g + b) / 3;
                    if (gray < 64) { r = 0; g = gray * 4; b = 255; } 
                    else if (gray < 128) { r = (gray-64) * 4; g = 255; b = 255 - ((gray-64)*4); } 
                    else if (gray < 192) { r = 255; g = (gray-128) * 4; b = 0; } 
                    else { r = 255; g = 255; b = (gray-192) * 4; }
                }
                else if (ctx->efectoActual == EFECTO_SOLAR) {
                    if (r > 127) r = 255 - r;
                    if (g > 127) g = 255 - g;
                    if (b > 127) b = 255 - b;
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

    // Variables Timer
    int timerMode = 0; // 0=OFF, 1=2s, 2=5s, 3=10s
    int timerValues[] = {0, 2, 5, 10};
    bool isCounting = false;
    Uint32 countStartTime = 0;
    int delayInput = 0;

    // Nombres de efectos (ACTUALIZADOS)
    const char* nombresFxEN[] = {
        "Normal", "Grayscale", "Sepia", "GameBoy", "Negative", 
        "Sketch (B&W)", "Thermal", "Glitch RGB", "Solarize", "Mirror",
        "Pixel Art", "Wavy (Drunk)"
    };
    const char* nombresFxES[] = {
        "Normal", "Escala Grises", "Sepia", "GameBoy", "Negativo", 
        "Boceto (B&N)", "Termica", "Glitch RGB", "Solarizar", "Espejo",
        "Pixel Art", "Ondas (Borracho)"
    };

    while (enCamara) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
             if (event.type == SDL_QUIT) { enCamara = false; resultado = -1; }
        }
        VPADStatus vpad; VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (delayInput > 0) delayInput--;
        if (delayCambio > 0) delayCambio--;

        // LOGICA INPUTS
        if (!isCounting) {
            if (vpad.trigger & VPAD_BUTTON_B) { enCamara = false; resultado = 1; }
            
            // TIMER (Y)
            if ((vpad.trigger & VPAD_BUTTON_Y) && delayInput == 0) {
                timerMode++; if (timerMode > 3) timerMode = 0;
                delayInput = 10;
            }

            // FOTO (A)
            if (vpad.trigger & VPAD_BUTTON_A) {
                if (timerMode == 0) {
                    GuardarFotoFxRapido(renderer, font, &ctx);
                    frameFlash = 5; 
                } else {
                    isCounting = true;
                    countStartTime = SDL_GetTicks();
                }
            }

            // CAMBIAR EFECTOS
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
        } else {
            if (vpad.trigger & VPAD_BUTTON_B) isCounting = false;
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
        
        // --- UI LATERAL ---
        const char* nombreFx = esIngles ? nombresFxEN[ctx.efectoActual] : nombresFxES[ctx.efectoActual];
        
        if (esIngles) {
             SDL_Surface* sT = TTF_RenderText_Blended(font, "Mode: Effects", col);
             SDL_Surface* sN = TTF_RenderText_Blended(font, nombreFx, {255, 255, 0, 255}); // Amarillo
             SDL_Surface* s1 = TTF_RenderText_Blended(font, "(< >) Change FX", col);
             SDL_Surface* s2 = TTF_RenderText_Blended(font, "(A) Take Photo", col);
             
             char bufTemp[32];
             if(timerMode == 0) sprintf(bufTemp, "(Y) Timer: OFF");
             else sprintf(bufTemp, "(Y) Timer: %ds", timerValues[timerMode]);
             SDL_Surface* sTime = TTF_RenderText_Blended(font, bufTemp, {0, 255, 255, 255});

             SDL_Surface* s3 = TTF_RenderText_Blended(font, "(B) Exit Mode", col);
             
             if(sT){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sT); SDL_Rect r={980,50,sT->w,sT->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sT); SDL_DestroyTexture(t); }
             if(sN){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sN); SDL_Rect r={980,90,sN->w,sN->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sN); SDL_DestroyTexture(t); }
             
             if(s1){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s1); SDL_Rect r={980,150,s1->w,s1->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s1); SDL_DestroyTexture(t); }
             if(s2){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s2); SDL_Rect r={980,200,s2->w,s2->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s2); SDL_DestroyTexture(t); }
             if(sTime){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sTime); SDL_Rect r={980,250,sTime->w,sTime->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sTime); SDL_DestroyTexture(t); }
             if(s3){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s3); SDL_Rect r={980,600,s3->w,s3->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s3); SDL_DestroyTexture(t); }
        } else {
             SDL_Surface* sT = TTF_RenderText_Blended(font, "Modo: Efectos", col);
             SDL_Surface* sN = TTF_RenderText_Blended(font, nombreFx, {255, 255, 0, 255});
             SDL_Surface* s1 = TTF_RenderText_Blended(font, "(< >) Cambiar FX", col);
             SDL_Surface* s2 = TTF_RenderText_Blended(font, "(A) Tomar Foto", col);
             
             char bufTemp[32];
             if(timerMode == 0) sprintf(bufTemp, "(Y) Temp.: OFF");
             else sprintf(bufTemp, "(Y) Temp.: %ds", timerValues[timerMode]);
             SDL_Surface* sTime = TTF_RenderText_Blended(font, bufTemp, {0, 255, 255, 255});

             SDL_Surface* s3 = TTF_RenderText_Blended(font, "(B) Salir Modo", col);

             if(sT){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sT); SDL_Rect r={980,50,sT->w,sT->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sT); SDL_DestroyTexture(t); }
             if(sN){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sN); SDL_Rect r={980,90,sN->w,sN->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sN); SDL_DestroyTexture(t); }

             if(s1){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s1); SDL_Rect r={980,150,s1->w,s1->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s1); SDL_DestroyTexture(t); }
             if(s2){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s2); SDL_Rect r={980,200,s2->w,s2->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s2); SDL_DestroyTexture(t); }
             if(sTime){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sTime); SDL_Rect r={980,250,sTime->w,sTime->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sTime); SDL_DestroyTexture(t); }
             if(s3){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s3); SDL_Rect r={980,600,s3->w,s3->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s3); SDL_DestroyTexture(t); }
        }

        if (isCounting) {
            Uint32 now = SDL_GetTicks();
            int elapsed = (now - countStartTime) / 1000;
            int remaining = timerValues[timerMode] - elapsed;

            if (remaining <= 0) {
                isCounting = false;
                GuardarFotoFxRapido(renderer, font, &ctx);
                frameFlash = 5;
            } else {
                char numBuf[4]; sprintf(numBuf, "%d", remaining);
                SDL_Surface* sNum = TTF_RenderText_Blended(font, numBuf, {255, 255, 0, 255});
                if (sNum) {
                    SDL_Texture* tNum = SDL_CreateTextureFromSurface(renderer, sNum);
                    SDL_Rect rNum = {(960 - 200)/2, (720 - 300)/2, 200, 300}; 
                    SDL_RenderCopy(renderer, tNum, NULL, &rNum);
                    SDL_FreeSurface(sNum); SDL_DestroyTexture(tNum);
                }
            }
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