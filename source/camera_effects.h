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

enum TipoEfecto {
    EFECTO_NORMAL,
    EFECTO_GRAYSCALE,
    EFECTO_SEPIA,
    EFECTO_GAMEBOY,
    EFECTO_NEGATIVO,
    EFECTO_BOCETO,    
    EFECTO_TERMICA,   
    EFECTO_GLITCH,    
    EFECTO_SOLAR,     
    EFECTO_ESPEJO,    
    EFECTO_PIXEL,     
    EFECTO_ONDAS,     
    EFECTO_CUATRO,
    EFECTO_DIVIDIDA,
    EFECTO_RAYOS_X,
    EFECTO_NEON,
    EFECTO_POP_ART,
    EFECTO_GIRO,
    EFECTO_OJO_PEZ,
    EFECTO_BULTO,
    EFECTO_PELLIZQUE,
    EFECTO_TRAMO,
    EFECTO_LSD,
    EFECTO_ALIEN,
    EFECTO_ESPECTRO,
    EFECTO_POP_CABINA,
    EFECTO_TIRA_PELICULA,
    EFECTO_FANTASMA_COLOR,
    EFECTO_BARAJAR,
    EFECTO_TUNEL,
    EFECTO_PLANETA,
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

void EscribirShortLE_Fx(FILE* f, uint16_t valor) {
    uint16_t le = (valor >> 8) | (valor << 8);
    fwrite(&le, 2, 1, f);
}

void EscribirIntLE_Fx(FILE* f, uint32_t valor) {
    uint32_t le = ((valor >> 24) & 0xFF) | ((valor >> 8) & 0xFF00) | ((valor << 8) & 0xFF0000) | ((valor << 24) & 0xFF000000);
    fwrite(&le, 4, 1, f);
}

void ProcesarFrameEfectos(ContextoCamaraFx* ctx) {
    int offsetUV = CAM_PITCH * CAM_HEIGHT;
    static float timeVal = 0.0f;
    timeVal += 0.2f; 

    float cx = CAM_WIDTH / 2.0f;
    float cy = CAM_HEIGHT / 2.0f;

    for (int y = 0; y < CAM_HEIGHT; y += 2) {
        int waveOffset = 0;
        if (ctx->efectoActual == EFECTO_ONDAS) waveOffset = (int)(sin(y * 0.05f + timeVal) * 10.0f);

        int fY1 = y * CAM_PITCH; int fY2 = (y + 1) * CAM_PITCH;
        int fUV = offsetUV + ((y / 2) * CAM_PITCH);
        int fOut1 = y * CAM_WIDTH; int fOut2 = (y + 1) * CAM_WIDTH;

        for (int x = 0; x < CAM_WIDTH; x += 2) {
            int readX = x;
            int readY1 = fY1; 
            int readY2 = fY2;
            int readUV_idx = fUV + x;

            float dx = x - cx;
            float dy = y - cy;
            float r = sqrt(dx*dx + dy*dy);
            float a = atan2(dy, dx);
            int calcX = x;
            int calcY = y;
            bool doMathCoords = false;

            if (ctx->efectoActual == EFECTO_ESPEJO) {
                if (x >= CAM_WIDTH / 2) {
                    readX = (CAM_WIDTH - 2) - x; 
                    readUV_idx = fUV + readX;
                    if (readUV_idx % 2 != 0) readUV_idx--; 
                }
            }
            else if (ctx->efectoActual == EFECTO_PIXEL) {
                int blockSize = 8;
                int pX = (x / blockSize) * blockSize;
                int pY = (y / blockSize) * blockSize;
                readX = pX; readY1 = pY * CAM_PITCH; readY2 = pY * CAM_PITCH; 
                readUV_idx = offsetUV + ((pY / 2) * CAM_PITCH) + pX;
            }
            else if (ctx->efectoActual == EFECTO_ONDAS) {
                readX = x + waveOffset;
                if(readX < 0) readX = 0; if(readX >= CAM_WIDTH - 2) readX = CAM_WIDTH - 2;
                if(readX % 2 != 0) readX--; readUV_idx = fUV + readX;
            }
            else if (ctx->efectoActual == EFECTO_CUATRO) {
                int srcX = (x * 2) % CAM_WIDTH; int srcY = (y * 2) % CAM_HEIGHT;
                srcX = srcX & ~1; readX = srcX; readY1 = srcY * CAM_PITCH; readY2 = (srcY + 1) * CAM_PITCH;
                readUV_idx = offsetUV + ((srcY / 2) * CAM_PITCH) + srcX;
            }
            else if (ctx->efectoActual == EFECTO_DIVIDIDA) {
                int srcX = x; if (x >= CAM_WIDTH / 2) srcX = x - (CAM_WIDTH / 2);
                srcX = srcX & ~1; readX = srcX; readUV_idx = fUV + srcX;
            }
            else if (ctx->efectoActual == EFECTO_GIRO) {
                float na = a + r * 0.015f;
                calcX = cx + cos(na) * r; calcY = cy + sin(na) * r; doMathCoords = true;
            }
            else if (ctx->efectoActual == EFECTO_OJO_PEZ) {
                float nr = r * r / 200.0f;
                calcX = cx + cos(a) * nr; calcY = cy + sin(a) * nr; doMathCoords = true;
            }
            else if (ctx->efectoActual == EFECTO_BULTO) {
                float nr = r * (r / 300.0f); if(r < 10.0f) nr = r;
                calcX = cx + cos(a) * nr; calcY = cy + sin(a) * nr; doMathCoords = true;
            }
            else if (ctx->efectoActual == EFECTO_PELLIZQUE) {
                float nr = r + 5000.0f / (r + 10.0f);
                calcX = cx + cos(a) * nr; calcY = cy + sin(a) * nr; doMathCoords = true;
            }
            else if (ctx->efectoActual == EFECTO_TRAMO) {
                calcX = x; calcY = cy + dy * (fabs(dx) / 150.0f); doMathCoords = true;
            }
            else if (ctx->efectoActual == EFECTO_TUNEL) {
                float nr = 15000.0f / (r + 10.0f);
                calcX = cx + cos(a) * nr; calcY = cy + sin(a) * nr; doMathCoords = true;
            }
            else if (ctx->efectoActual == EFECTO_BARAJAR) {
                int slice = y / 30; calcX = x + ((slice % 2 == 0) ? 30 : -30); calcY = y; doMathCoords = true;
            }
            else if (ctx->efectoActual == EFECTO_TIRA_PELICULA) {
                calcX = (x * 4) % CAM_WIDTH; calcY = (y * 4) % CAM_HEIGHT; doMathCoords = true;
            }
            

            if (doMathCoords) {
                readX = calcX; int ry = calcY;
                if(readX < 0) readX = 0; if(readX > CAM_WIDTH-2) readX = CAM_WIDTH-2;
                if(ry < 0) ry = 0; if(ry > CAM_HEIGHT-2) ry = CAM_HEIGHT-2;
                readX &= ~1; readY1 = ry * CAM_PITCH; readY2 = (ry + 1) * CAM_PITCH;
                readUV_idx = offsetUV + ((ry / 2) * CAM_PITCH) + readX;
            }

            int u = ctx->rawBuffer[readUV_idx] - 128; 
            int v = ctx->rawBuffer[readUV_idx + 1] - 128;
            int ySamples[4];

            if (ctx->efectoActual == EFECTO_PIXEL) {
                int pixelVal = ctx->rawBuffer[readY1 + readX];
                ySamples[0]=pixelVal; ySamples[1]=pixelVal; ySamples[2]=pixelVal; ySamples[3]=pixelVal;
            } else {
                ySamples[0] = ctx->rawBuffer[readY1 + readX]; ySamples[1] = ctx->rawBuffer[readY1 + readX + 1];
                ySamples[2] = ctx->rawBuffer[readY2 + readX]; ySamples[3] = ctx->rawBuffer[readY2 + readX + 1];
            }
            
            int outIndices[4] = { fOut1 + x, fOut1 + x + 1, fOut2 + x, fOut2 + x + 1 };

            int yGlitchR[4], yGlitchB[4];
            if (ctx->efectoActual == EFECTO_GLITCH || ctx->efectoActual == EFECTO_FANTASMA_COLOR) {
                int shift = (ctx->efectoActual == EFECTO_GLITCH) ? 10 : 25;
                for(int k=0; k<4; k++) {
                    yGlitchR[k] = ySamples[k]; yGlitchB[k] = ySamples[k];
                    if (readX + shift < CAM_WIDTH) yGlitchR[k] = ctx->rawBuffer[readY1 + readX + shift]; 
                    if (readX - shift > 0)         yGlitchB[k] = ctx->rawBuffer[readY1 + readX - shift];
                }
            }

            bool isEdge[4] = {false, false, false, false};
            if (ctx->efectoActual == EFECTO_BOCETO || ctx->efectoActual == EFECTO_NEON) {
                for(int k=0; k<4; k++) {
                    int nextY = ySamples[k]; 
                    if (readX + 2 < CAM_WIDTH) nextY = ctx->rawBuffer[readY1 + readX + 2];
                    if (abs(ySamples[k] - nextY) > 20) isEdge[k] = true;
                }
            }

            for (int k = 0; k < 4; k++) {
                int r, g, b;

                if (ctx->efectoActual == EFECTO_BOCETO) {
                    int val = isEdge[k] ? 0 : 255; r = val; g = val; b = val;
                }
                else if (ctx->efectoActual == EFECTO_NEON) {
                    if (isEdge[k]) { r = 0; g = 255; b = 255; } else { r = 0; g = 0; b = 0; }
                }
                else {
                    int yVal = ySamples[k]; int cR, cG, cB;

                    if (ctx->efectoActual == EFECTO_GLITCH || ctx->efectoActual == EFECTO_FANTASMA_COLOR) {
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
                
                if (ctx->efectoActual == EFECTO_GRAYSCALE) {
                    int gray = (r * 30 + g * 59 + b * 11) / 100; r = g = b = gray;
                } 
                else if (ctx->efectoActual == EFECTO_SEPIA) {
                    int tr = (r * 0.393) + (g * 0.769) + (b * 0.189);
                    int tg = (r * 0.349) + (g * 0.686) + (b * 0.168);
                    int tb = (r * 0.272) + (g * 0.534) + (b * 0.131);
                    r = CLAMP(tr); g = CLAMP(tg); b = CLAMP(tb);
                }
                else if (ctx->efectoActual == EFECTO_GAMEBOY) {
                    int gray = (r * 30 + g * 59 + b * 11) / 100;
                    if (gray < 64) { r=15; g=56; b=15; } else if (gray < 128) { r=48; g=98; b=48; } else if (gray < 192) { r=139; g=172; b=15; } else { r=155; g=188; b=15; }
                }
                else if (ctx->efectoActual == EFECTO_NEGATIVO) {
                    r = 255 - r; g = 255 - g; b = 255 - b;
                }
                else if (ctx->efectoActual == EFECTO_TERMICA) {
                    int gray = (r + g + b) / 3;
                    if (gray < 64) { r = 0; g = gray * 4; b = 255; } else if (gray < 128) { r = (gray-64) * 4; g = 255; b = 255 - ((gray-64)*4); } else if (gray < 192) { r = 255; g = (gray-128) * 4; b = 0; } else { r = 255; g = 255; b = (gray-192) * 4; }
                }
                else if (ctx->efectoActual == EFECTO_SOLAR) {
                    if (r > 127) r = 255 - r; if (g > 127) g = 255 - g; if (b > 127) b = 255 - b;
                }
                else if (ctx->efectoActual == EFECTO_RAYOS_X) {
                    r = 255 - r; g = 255 - g; b = 255 - b;
                    r = CLAMP(r - 50); g = CLAMP(g + 20); b = CLAMP(b + 50); 
                }
                else if (ctx->efectoActual == EFECTO_POP_ART) {
                    int quadX = x / (CAM_WIDTH / 2); int quadY = y / (CAM_HEIGHT / 2);
                    int gray = (r * 30 + g * 59 + b * 11) / 100;
                    if (quadX == 0 && quadY == 0) { r = CLAMP(gray + 50); g = 0; b = 0; }
                    else if (quadX == 1 && quadY == 0) { r = 0; g = CLAMP(gray + 50); b = 0; }
                    else if (quadX == 0 && quadY == 1) { r = 0; g = 0; b = CLAMP(gray + 50); }
                    else { r = CLAMP(gray + 50); g = CLAMP(gray + 50); b = 0; }
                }
                else if (ctx->efectoActual == EFECTO_LSD) {
                    if(r < 128) r = 0; else r = 255;
                    if(g < 128) g = 0; else g = 255;
                    if(b < 128) b = 0; else b = 255;
                }
                else if (ctx->efectoActual == EFECTO_ALIEN) {
                    int tr = g; int tg = b; int tb = r;
                    r = CLAMP(tr - 20); g = CLAMP(tg + 40); b = CLAMP(tb + 40);
                }
                else if (ctx->efectoActual == EFECTO_ESPECTRO) {
                    int luma = (r + g + b) / 3;
                    if(luma < 85) { r = 255; g = luma*3; b = 0; }
                    else if(luma < 170) { r = 255 - (luma-85)*3; g = 255; b = (luma-85)*3; }
                    else { r = 0; g = 255 - (luma-170)*3; b = 255; }
                }
                else if (ctx->efectoActual == EFECTO_POP_CABINA) {
                    int quadX = x / (CAM_WIDTH / 2); int quadY = y / (CAM_HEIGHT / 2);
                    int gray = (r * 30 + g * 59 + b * 11) / 100;
                    if (quadX == 0 && quadY == 0) { r = CLAMP(gray+100); g = CLAMP(gray); b = CLAMP(gray); }
                    else if (quadX == 1 && quadY == 0) { r = CLAMP(gray); g = CLAMP(gray+100); b = CLAMP(gray); }
                    else if (quadX == 0 && quadY == 1) { r = CLAMP(gray); g = CLAMP(gray); b = CLAMP(gray+100); }
                    else { r = CLAMP(gray+100); g = CLAMP(gray+100); b = CLAMP(gray); }
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
        uint32_t width = CAM_WIDTH; uint32_t height = CAM_HEIGHT; uint32_t imageSize = width * height * 4; uint32_t fileSize = 54 + imageSize;
        fwrite("BM", 1, 2, f); EscribirIntLE_Fx(f, fileSize); EscribirIntLE_Fx(f, 0); EscribirIntLE_Fx(f, 54);        
        EscribirIntLE_Fx(f, 40); EscribirIntLE_Fx(f, width); EscribirIntLE_Fx(f, -((int)height)); EscribirShortLE_Fx(f, 1); EscribirShortLE_Fx(f, 32); EscribirIntLE_Fx(f, 0); EscribirIntLE_Fx(f, imageSize); EscribirIntLE_Fx(f, 2835); EscribirIntLE_Fx(f, 2835); EscribirIntLE_Fx(f, 0); EscribirIntLE_Fx(f, 0);         

        uint32_t* tempBuffer = (uint32_t*)malloc(imageSize);
        if (tempBuffer) {
            for (int i = 0; i < (width * height); i++) {
                uint32_t pixel = ctx->cleanBuffer[i];
                uint32_t r = (pixel >> 24) & 0xFF; uint32_t g = (pixel >> 16) & 0xFF; uint32_t b = (pixel >> 8) & 0xFF; uint32_t a = pixel & 0xFF;
                tempBuffer[i] = (b << 24) | (g << 16) | (r << 8) | a;
            }
            fwrite(tempBuffer, 1, imageSize, f); free(tempBuffer);
        }
        fclose(f);
    }
}

ContextoCamaraFx IniciarCamaraFxContexto(SDL_Renderer* renderer) {
    ContextoCamaraFx ctx; memset(&ctx, 0, sizeof(ContextoCamaraFx));
    CAMError err; CAMStreamInfo info;
    info.type = CAMERA_STREAM_TYPE_1; info.width = CAM_WIDTH; info.height = CAM_HEIGHT;
    int memSize = CAMGetMemReq(&info); ctx.workMem = memalign(256, memSize);
    CAMSetupInfo setup; memset(&setup, 0, sizeof(CAMSetupInfo));
    setup.streamInfo = info; setup.workMem.pMem = ctx.workMem; setup.workMem.size = memSize; setup.eventHandler = CallbackCamaraFx; setup.mode.fps = CAMERA_FPS_30;

    ctx.handle = CAMInit(0, &setup, &err);
    if (err == CAMERA_ERROR_OK) {
        ctx.exito = true; CAMOpen(ctx.handle);
        ctx.rawBuffer = (uint8_t*)memalign(256, CAMERA_YUV_BUFFER_SIZE); ctx.cleanBuffer = (uint32_t*)memalign(256, CAM_WIDTH * CAM_HEIGHT * 4);
        memset(&ctx.surface, 0, sizeof(CAMSurface));
        ctx.surface.width = CAM_WIDTH; ctx.surface.height = CAM_HEIGHT; ctx.surface.pitch = CAM_PITCH; ctx.surface.alignment = CAMERA_YUV_BUFFER_ALIGNMENT; ctx.surface.surfaceSize = CAMERA_YUV_BUFFER_SIZE; ctx.surface.surfaceBuffer = ctx.rawBuffer;
        CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        ctx.textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, CAM_WIDTH, CAM_HEIGHT);
        ctx.efectoActual = EFECTO_NORMAL;
    }
    return ctx;
}

void CerrarCamaraFxContexto(ContextoCamaraFx* ctx) {
    if (ctx->exito) { CAMClose(ctx->handle); CAMExit(ctx->handle); }
    if (ctx->textura) SDL_DestroyTexture(ctx->textura);
    if (ctx->rawBuffer) free(ctx->rawBuffer);
    if (ctx->cleanBuffer) free(ctx->cleanBuffer);
    if (ctx->workMem) free(ctx->workMem);
    ctx->exito = false;
}

int EjecutarCamaraEfectos(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    ContextoCamaraFx ctx = IniciarCamaraFxContexto(renderer);
    if (!ctx.exito) { CerrarCamaraFxContexto(&ctx); return 0; }

    bool enCamara = true; int resultado = 0; SDL_Rect rectCamara = {0, 0, 960, 720}; 
    camFxFrameListo = false; int frameFlash = 0; int delayCambio = 0;
    int timerMode = 0; int timerValues[] = {0, 2, 5, 10}; bool isCounting = false; Uint32 countStartTime = 0; int delayInput = 0;
    bool menuAbierto = false; int menuSelection = 0;

    const char* nombresFxEN[] = {
        "Normal", "Grayscale", "Sepia", "GameBoy", "Negative", 
        "Sketch", "Thermal", "Glitch", "Solarize", "Mirror",
        "Pixel Art", "Wavy", "Four Split", "Split Screen", "X-Ray",
        "Neon", "Pop Art", "Swirl", "Fisheye", "Bulge", "Pinch", "Stretch",
        "LSD", "Alien", "Spectrum", "Pop Booth", "Film Strip",
        "Chromatic", "Shuffle", "Tunnel", "Tiny Planet"
    };
    const char* nombresFxES[] = {
        "Normal", "Escala Grises", "Sepia", "GameBoy", "Negativo", 
        "Boceto", "Termica", "Glitch", "Solarizar", "Espejo",
        "Pixel Art", "Ondas", "Camara Cuatro", "Dividida", "Rayos X",
        "Neon", "Pop Art", "Giro", "Ojo de Pez", "Bulto", "Pellizque", "Tramo",
        "LSD", "Alien", "Espectro", "Pop Cabina", "Tira Pelicula",
        "Fantasma", "Barajar", "Tunel", "Mini Planeta"
    };

    auto MapearTouchFx = [](float val, float min, float max, float outMax, bool inv) {
        float pct = (val - min) / (max - min); if (pct < 0) pct = 0; if (pct > 1) pct = 1; if (inv) pct = 1.0f - pct; return (int)(pct * outMax);
    };

    while (enCamara) {
        SDL_Event event; while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) { enCamara = false; resultado = -1; } }
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (delayInput > 0) delayInput--;
        if (delayCambio > 0) delayCambio--;

        if (menuAbierto) {
            if (vpad.trigger & VPAD_BUTTON_B) menuAbierto = false;
            
            if ((vpad.trigger & VPAD_BUTTON_RIGHT) && delayInput == 0) { menuSelection++; if(menuSelection >= EFECTO_TOTAL) menuSelection = 0; delayInput = 10; }
            if ((vpad.trigger & VPAD_BUTTON_LEFT) && delayInput == 0) { menuSelection--; if(menuSelection < 0) menuSelection = EFECTO_TOTAL - 1; delayInput = 10; }
            if ((vpad.trigger & VPAD_BUTTON_DOWN) && delayInput == 0) { menuSelection += 5; if(menuSelection >= EFECTO_TOTAL) menuSelection = EFECTO_TOTAL - 1; delayInput = 10; }
            if ((vpad.trigger & VPAD_BUTTON_UP) && delayInput == 0) { menuSelection -= 5; if(menuSelection < 0) menuSelection = 0; delayInput = 10; }

            if ((vpad.trigger & VPAD_BUTTON_A) && delayInput == 0) {
                ctx.efectoActual = menuSelection; menuAbierto = false; delayInput = 15;
            }

            if (vpad.tpNormal.touched && delayInput == 0) {
                int tX = MapearTouchFx(vpad.tpNormal.x, 100.0f, 3950.0f, 1280.0f, false); int tY = MapearTouchFx(vpad.tpNormal.y, 100.0f, 3900.0f, 720.0f, true);
                int cols = 5; int bW = 180; int bH = 40; int startX = 150; int startY = 120; int padX = 15; int padY = 15;
                for (int i = 0; i < EFECTO_TOTAL; i++) {
                    int col = i % cols; int row = i / cols;
                    int x = startX + col * (bW + padX); int y = startY + row * (bH + padY);
                    if (tX >= x && tX <= x + bW && tY >= y && tY <= y + bH) { ctx.efectoActual = i; menuSelection = i; menuAbierto = false; delayInput = 15; break; }
                }
                if (tX >= 1000 && tX <= 1200 && tY >= 650 && tY <= 700) { menuAbierto = false; delayInput = 15; }
            }
        } 
        else if (!isCounting) {
            if (vpad.trigger & VPAD_BUTTON_B) { enCamara = false; resultado = 1; }
            if ((vpad.trigger & VPAD_BUTTON_Y) && delayInput == 0) { timerMode++; if (timerMode > 3) timerMode = 0; delayInput = 10; }
            if (vpad.trigger & VPAD_BUTTON_A) {
                if (timerMode == 0) { GuardarFotoFxRapido(renderer, font, &ctx); frameFlash = 5; } 
                else { isCounting = true; countStartTime = SDL_GetTicks(); }
            }
            if (delayCambio == 0) {
                if (vpad.hold & VPAD_BUTTON_RIGHT) { ctx.efectoActual++; if (ctx.efectoActual >= EFECTO_TOTAL) ctx.efectoActual = 0; delayCambio = 15; }
                if (vpad.hold & VPAD_BUTTON_LEFT) { ctx.efectoActual--; if (ctx.efectoActual < 0) ctx.efectoActual = EFECTO_TOTAL - 1; delayCambio = 15; }
            }
            if (vpad.tpNormal.touched && delayInput == 0) {
                int tX = MapearTouchFx(vpad.tpNormal.x, 100.0f, 3950.0f, 1280.0f, false); int tY = MapearTouchFx(vpad.tpNormal.y, 100.0f, 3900.0f, 720.0f, true);
                if (tX >= 980 && tX <= 1250 && tY >= 350 && tY <= 410) { menuAbierto = true; menuSelection = ctx.efectoActual; delayInput = 15; }
            }
        } else {
            if (vpad.trigger & VPAD_BUTTON_B) isCounting = false;
        }

        if (ctx.exito && camFxFrameListo) {
            DCInvalidateRange(ctx.rawBuffer, CAMERA_YUV_BUFFER_SIZE); ProcesarFrameEfectos(&ctx);
            SDL_UpdateTexture(ctx.textura, NULL, ctx.cleanBuffer, CAM_WIDTH * 4); camFxFrameListo = false; CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
        if (ctx.textura) SDL_RenderCopy(renderer, ctx.textura, NULL, &rectCamara);

        if (menuAbierto) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 20, 20, 30, 220); SDL_RenderFillRect(renderer, NULL); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_Color colW = {255, 255, 255, 255};
            SDL_Surface* sTitle = TTF_RenderText_Blended(font, esIngles ? "Select Camera Effect" : "Seleccionar Efecto", colW);
            if(sTitle){ SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, sTitle); SDL_Rect r = {(1280 - sTitle->w)/2, 40, sTitle->w, sTitle->h}; SDL_RenderCopy(renderer, t, NULL, &r); SDL_FreeSurface(sTitle); SDL_DestroyTexture(t); }

            int cols = 5; int bW = 180; int bH = 40; int startX = 150; int startY = 120; int padX = 15; int padY = 15;
            for (int i = 0; i < EFECTO_TOTAL; i++) {
                int col = i % cols; int row = i / cols; int x = startX + col * (bW + padX); int y = startY + row * (bH + padY);
                SDL_Rect br = {x, y, bW, bH};
                if (i == menuSelection) SDL_SetRenderDrawColor(renderer, 232, 127, 250, 255); else SDL_SetRenderDrawColor(renderer, 60, 60, 65, 255);
                SDL_RenderFillRect(renderer, &br); SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &br);
                const char* nTexto = esIngles ? nombresFxEN[i] : nombresFxES[i];
                SDL_Surface* sTx = TTF_RenderText_Blended(font, nTexto, colW);
                if(sTx) { SDL_Texture* tTx = SDL_CreateTextureFromSurface(renderer, sTx); SDL_Rect rTx = {x + (bW - sTx->w)/2, y + (bH - sTx->h)/2, sTx->w, sTx->h}; SDL_RenderCopy(renderer, tTx, NULL, &rTx); SDL_FreeSurface(sTx); SDL_DestroyTexture(tTx); }
            }

            SDL_Rect brEx = {1000, 650, 200, 50}; SDL_SetRenderDrawColor(renderer, 150, 50, 50, 255); SDL_RenderFillRect(renderer, &brEx); SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &brEx);
            SDL_Surface* sEx = TTF_RenderText_Blended(font, esIngles ? "(B) Close" : "(B) Cerrar", colW);
            if(sEx) { SDL_Texture* tEx = SDL_CreateTextureFromSurface(renderer, sEx); SDL_Rect rEx = {1000 + (200 - sEx->w)/2, 650 + (50 - sEx->h)/2, sEx->w, sEx->h}; SDL_RenderCopy(renderer, tEx, NULL, &rEx); SDL_FreeSurface(sEx); SDL_DestroyTexture(tEx); }
        } 
        else {
            SDL_Color col = {255, 255, 255, 255};
            const char* nombreFx = esIngles ? nombresFxEN[ctx.efectoActual] : nombresFxES[ctx.efectoActual];
            
            if (esIngles) {
                 SDL_Surface* sT = TTF_RenderText_Blended(font, "Mode: Effects", col); SDL_Surface* sN = TTF_RenderText_Blended(font, nombreFx, {255, 255, 0, 255}); 
                 SDL_Surface* s1 = TTF_RenderText_Blended(font, "(< >) Change FX", col); SDL_Surface* s2 = TTF_RenderText_Blended(font, "(A) Take Photo", col);
                 char bufTemp[32]; if(timerMode == 0) sprintf(bufTemp, "(Y) Timer: OFF"); else sprintf(bufTemp, "(Y) Timer: %ds", timerValues[timerMode]);
                 SDL_Surface* sTime = TTF_RenderText_Blended(font, bufTemp, {0, 255, 255, 255}); SDL_Surface* s3 = TTF_RenderText_Blended(font, "(B) Exit Mode", col);
                 if(sT){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sT); SDL_Rect r={980,50,sT->w,sT->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sT); SDL_DestroyTexture(t); }
                 if(sN){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sN); SDL_Rect r={980,90,sN->w,sN->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sN); SDL_DestroyTexture(t); }
                 if(s1){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s1); SDL_Rect r={980,150,s1->w,s1->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s1); SDL_DestroyTexture(t); }
                 if(s2){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s2); SDL_Rect r={980,200,s2->w,s2->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s2); SDL_DestroyTexture(t); }
                 if(sTime){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sTime); SDL_Rect r={980,250,sTime->w,sTime->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sTime); SDL_DestroyTexture(t); }
                 if(s3){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s3); SDL_Rect r={980,600,s3->w,s3->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s3); SDL_DestroyTexture(t); }
                 
                 SDL_Rect btnMenu = {980, 350, 270, 60}; SDL_SetRenderDrawColor(renderer, 60, 60, 65, 255); SDL_RenderFillRect(renderer, &btnMenu); SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &btnMenu);
                 SDL_Surface* sM = TTF_RenderText_Blended(font, "Menu FX", col);
                 if(sM){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sM); SDL_Rect r={980 + (270 - sM->w)/2, 350 + (60 - sM->h)/2, sM->w, sM->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sM); SDL_DestroyTexture(t); }
            } else {
                 SDL_Surface* sT = TTF_RenderText_Blended(font, "Modo: Efectos", col); SDL_Surface* sN = TTF_RenderText_Blended(font, nombreFx, {255, 255, 0, 255});
                 SDL_Surface* s1 = TTF_RenderText_Blended(font, "(< >) Cambiar FX", col); SDL_Surface* s2 = TTF_RenderText_Blended(font, "(A) Tomar Foto", col);
                 char bufTemp[32]; if(timerMode == 0) sprintf(bufTemp, "(Y) Temp.: OFF"); else sprintf(bufTemp, "(Y) Temp.: %ds", timerValues[timerMode]);
                 SDL_Surface* sTime = TTF_RenderText_Blended(font, bufTemp, {0, 255, 255, 255}); SDL_Surface* s3 = TTF_RenderText_Blended(font, "(B) Salir Modo", col);

                 if(sT){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sT); SDL_Rect r={980,50,sT->w,sT->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sT); SDL_DestroyTexture(t); }
                 if(sN){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sN); SDL_Rect r={980,90,sN->w,sN->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sN); SDL_DestroyTexture(t); }
                 if(s1){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s1); SDL_Rect r={980,150,s1->w,s1->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s1); SDL_DestroyTexture(t); }
                 if(s2){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s2); SDL_Rect r={980,200,s2->w,s2->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s2); SDL_DestroyTexture(t); }
                 if(sTime){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sTime); SDL_Rect r={980,250,sTime->w,sTime->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sTime); SDL_DestroyTexture(t); }
                 if(s3){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s3); SDL_Rect r={980,600,s3->w,s3->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s3); SDL_DestroyTexture(t); }
                 
                 SDL_Rect btnMenu = {980, 350, 270, 60}; SDL_SetRenderDrawColor(renderer, 60, 60, 65, 255); SDL_RenderFillRect(renderer, &btnMenu); SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &btnMenu);
                 SDL_Surface* sM = TTF_RenderText_Blended(font, "Menu Efectos", col);
                 if(sM){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sM); SDL_Rect r={980 + (270 - sM->w)/2, 350 + (60 - sM->h)/2, sM->w, sM->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sM); SDL_DestroyTexture(t); }
            }

            if (isCounting) {
                Uint32 now = SDL_GetTicks(); int elapsed = (now - countStartTime) / 1000; int remaining = timerValues[timerMode] - elapsed;
                if (remaining <= 0) {
                    isCounting = false; GuardarFotoFxRapido(renderer, font, &ctx); frameFlash = 5;
                } else {
                    char numBuf[4]; sprintf(numBuf, "%d", remaining);
                    SDL_Surface* sNum = TTF_RenderText_Blended(font, numBuf, {255, 255, 0, 255});
                    if (sNum) { SDL_Texture* tNum = SDL_CreateTextureFromSurface(renderer, sNum); SDL_Rect rNum = {(960 - 200)/2, (720 - 300)/2, 200, 300}; SDL_RenderCopy(renderer, tNum, NULL, &rNum); SDL_FreeSurface(sNum); SDL_DestroyTexture(tNum); }
                }
            }

            if (frameFlash > 0) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 255, 255, 255, 150); SDL_RenderFillRect(renderer, NULL); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); frameFlash--;
            }
        }
        SDL_RenderPresent(renderer);
    }
    CerrarCamaraFxContexto(&ctx);
    return resultado;
}

#endif