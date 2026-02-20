#ifndef EDITOR_H
#define EDITOR_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <vpad/input.h>
#include <whb/proc.h> 
#include <vector>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <time.h>
#include <math.h> 
#include <map>

int EjecutarGaleria(SDL_Renderer* renderer, TTF_Font* font, bool esIngles, int tipoGaleria, Mix_Music* musicPtr, bool musicEnabled);

struct EditorFile { std::string nombre; std::string ruta; };
const std::string CONTENT_PATH = "fs:/vol/content/"; 

std::vector<EditorFile> EscanearTodoParaEditor() {
    std::vector<EditorFile> lista;
    std::vector<std::string> rutas = { "fs:/vol/external01/WiiUCamera Files", "fs:/vol/external01/wiiu/screenshots" };
    auto EscanearRec = [&](auto&& self, std::string ruta) -> void {
        DIR* dir = opendir(ruta.c_str()); if (!dir) return;
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string n = ent->d_name; if (n == "." || n == "..") continue;
            std::string full = ruta + "/" + n; struct stat st;
            if (stat(full.c_str(), &st) == 0) {
                if (S_ISDIR(st.st_mode)) { self(self, full); }
                else if (n.length() > 4) {
                    std::string ext = n.substr(n.length()-4); for(auto& c:ext) c=tolower(c);
                    if(ext==".jpg"||ext==".png"||ext==".bmp") lista.push_back({n, full});
                }
            }
        }
        closedir(dir);
    };
    for(const auto& r : rutas) EscanearRec(EscanearRec, r);
    std::sort(lista.begin(), lista.end(), [](const EditorFile& a, const EditorFile& b){ return a.nombre > b.nombre; });
    return lista;
}

Uint32 ObtenerPixel(SDL_Surface* surface, int x, int y) {
    if(x<0 || y<0 || x>=surface->w || y>=surface->h) return 0;
    int bpp = surface->format->BytesPerPixel;
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
    switch(bpp) {
        case 1: return *p; case 2: return *(Uint16 *)p; case 3: if(SDL_BYTEORDER==SDL_BIG_ENDIAN) return p[0]<<16|p[1]<<8|p[2]; else return p[0]|p[1]<<8|p[2]<<16; case 4: return *(Uint32 *)p; default: return 0;
    }
}
void PonerPixel(SDL_Surface* surface, int x, int y, Uint32 pixel) {
    if(x<0 || y<0 || x>=surface->w || y>=surface->h) return;
    int bpp = surface->format->BytesPerPixel;
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
    switch(bpp) {
        case 1: *p=pixel; break; case 2: *(Uint16 *)p=pixel; break;
        case 3: if(SDL_BYTEORDER==SDL_BIG_ENDIAN){p[0]=(pixel>>16)&0xff;p[1]=(pixel>>8)&0xff;p[2]=pixel&0xff;}else{p[0]=pixel&0xff;p[1]=(pixel>>8)&0xff;p[2]=(pixel>>16)&0xff;} break;
        case 4: *(Uint32 *)p=pixel; break;
    }
}
void PintarEnSurface(SDL_Surface* s, int cx, int cy, int radio, SDL_Color col) {
    Uint32 pixel = SDL_MapRGBA(s->format, col.r, col.g, col.b, col.a);
    for(int y = -radio; y <= radio; y++) {
        for(int x = -radio; x <= radio; x++) {
            if(x*x + y*y <= radio*radio) PonerPixel(s, cx+x, cy+y, pixel);
        }
    }
}

SDL_Color HSVtoRGB(float h, float s, float v) {
    float c = v * s; float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1)); float m = v - c;
    float r = 0, g = 0, b = 0;
    if(h >= 0 && h < 60) { r=c; g=x; b=0; } else if(h >= 60 && h < 120) { r=x; g=c; b=0; } else if(h >= 120 && h < 180) { r=0; g=c; b=x; } else if(h >= 180 && h < 240) { r=0; g=x; b=c; } else if(h >= 240 && h < 300) { r=x; g=0; b=c; } else { r=c; g=0; b=x; }
    return (SDL_Color){(Uint8)((r+m)*255), (Uint8)((g+m)*255), (Uint8)((b+m)*255), 255};
}

SDL_Texture* CrearTexturaEspectro(SDL_Renderer* ren, int w, int h) {
    SDL_Surface* s = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    for(int i=0; i<w; i++) {
        float hue = ((float)i / w) * 360.0f;
        SDL_Color c = HSVtoRGB(hue, 1.0f, 1.0f);
        Uint32 p = SDL_MapRGBA(s->format, c.r, c.g, c.b, 255);
        for(int y=0; y<h; y++) PonerPixel(s, i, y, p);
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s); return t;
}

void ActualizarCuadradoSV(SDL_Renderer* ren, SDL_Texture* tex, float hue, int w, int h) {
    void* pixels; int pitch;
    SDL_LockTexture(tex, NULL, &pixels, &pitch);
    Uint32* p32 = (Uint32*)pixels;
    for(int y=0; y<h; y++) {
        for(int x=0; x<w; x++) {
            float sat = (float)x / (float)w;
            float val = 1.0f - ((float)y / (float)h);
            SDL_Color c = HSVtoRGB(hue, sat, val);
            p32[y * (pitch/4) + x] = (c.r << 24) | (c.g << 16) | (c.b << 8) | 255;
        }
    }
    SDL_UnlockTexture(tex);
}

// --- EDITOR LOGICA ---
bool EjecutarLogicaDibujo(SDL_Renderer* renderer, TTF_Font* font, std::string ruta, bool esIngles) {
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderClear(renderer);
    if(font) {
        SDL_Surface* sL = TTF_RenderText_Blended(font, esIngles ? "Loading Image..." : "Cargando Imagen...", {255,255,255,255});
        if(sL) { SDL_Texture* tL = SDL_CreateTextureFromSurface(renderer, sL); SDL_Rect rL = {(1280-sL->w)/2, (720-sL->h)/2, sL->w, sL->h}; SDL_RenderCopy(renderer, tL, NULL, &rL); SDL_FreeSurface(sL); SDL_DestroyTexture(tL); }
    }
    SDL_RenderPresent(renderer); 
    
    SDL_Surface* sB = IMG_Load((CONTENT_PATH + "icon_brush.png").c_str());
    SDL_Surface* sU = IMG_Load((CONTENT_PATH + "icon_undo.png").c_str());
    SDL_Surface* sS = IMG_Load((CONTENT_PATH + "icon_save.png").c_str());
    SDL_Texture* tBrushIcon = sB ? SDL_CreateTextureFromSurface(renderer, sB) : NULL;
    SDL_Texture* tUndoIcon = sU ? SDL_CreateTextureFromSurface(renderer, sU) : NULL;
    SDL_Texture* tSaveIcon = sS ? SDL_CreateTextureFromSurface(renderer, sS) : NULL;
    if(sB) SDL_FreeSurface(sB); if(sU) SDL_FreeSurface(sU); if(sS) SDL_FreeSurface(sS);

    SDL_Surface* supRaw = IMG_Load(ruta.c_str()); if(!supRaw) return false; 
    SDL_Surface* supBase = SDL_ConvertSurfaceFormat(supRaw, SDL_PIXELFORMAT_RGBA8888, 0); 
    SDL_Surface* supWork = SDL_ConvertSurfaceFormat(supRaw, SDL_PIXELFORMAT_RGBA8888, 0); 
    SDL_FreeSurface(supRaw);
    SDL_Texture* texWork = SDL_CreateTextureFromSurface(renderer, supWork);

    int SV_SIZE = 280; int BAR_H = 50;    
    int BOX_W = SV_SIZE + 200; int BOX_H = SV_SIZE + BAR_H + 100;

    SDL_Texture* texGradient = CrearTexturaEspectro(renderer, SV_SIZE, BAR_H);
    SDL_Texture* texSquareSV = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, SV_SIZE, SV_SIZE);
    
    bool editando = true;
    bool guardadoExitoso = false;
    bool menuColorOpen = false;
    bool menuSaveOpen = false;
    
    float curHue = 0.0f; float curSat = 1.0f; float curVal = 1.0f; 
    SDL_Color colorActual = {255, 0, 0, 255};
    ActualizarCuadradoSV(renderer, texSquareSV, curHue, SV_SIZE, SV_SIZE);

    int pincelSize = 5;
    int lastX = -1, lastY = -1;
    bool necesitaUpdate = false;
    int delay = 0;

    const int BAR_W = 150;
    const int LIENZO_W = 1280 - BAR_W;
    const int LIENZO_H = 720;
    
    int btnX = LIENZO_W + (BAR_W - 64)/2; 
    SDL_Rect btnBrush = {btnX, 40, 64, 64};
    SDL_Rect btnUndo  = {btnX, 130, 64, 64};
    SDL_Rect btnSave  = {btnX, 220, 64, 64};

    // --- FIX LOOP INFINITO: Comprobar WHBProcIsRunning() ---
    while(editando && WHBProcIsRunning()) {
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
        SDL_Event ev; while(SDL_PollEvent(&ev)) { if(ev.type == SDL_QUIT) editando = false; }
        if(delay > 0) delay--;

        if(vpad.tpNormal.touched) {
            int tx = (int)((vpad.tpNormal.x - 100.0f) / 3850.0f * 1280.0f);
            int ty = (int)((1.0f - (vpad.tpNormal.y - 100.0f) / 3800.0f) * 720.0f);

            if(menuColorOpen) {
                int sx = (1280 - BOX_W)/2; int sy = (720 - BOX_H)/2;
                int pad = 25;
                SDL_Rect rSquare = {sx + pad, sy + 50, SV_SIZE, SV_SIZE};
                SDL_Rect rBarra  = {sx + pad, sy + 50 + SV_SIZE + 20, SV_SIZE, BAR_H};
                int rx = sx + pad + SV_SIZE + 20; int ry = sy + 50;
                SDL_Rect rUp = {rx + 35, ry + 80, 50, 50}; 
                SDL_Rect rDown = {rx + 35, ry + 180, 50, 50}; 

                if (tx >= rBarra.x && tx <= rBarra.x + rBarra.w && ty >= rBarra.y && ty <= rBarra.y + rBarra.h) {
                     float pct = (float)(tx - rBarra.x) / (float)rBarra.w; if(pct<0) pct=0; if(pct>1) pct=1;
                     curHue = pct * 360.0f;
                     ActualizarCuadradoSV(renderer, texSquareSV, curHue, SV_SIZE, SV_SIZE);
                     colorActual = HSVtoRGB(curHue, curSat, curVal);
                }
                else if (tx >= rSquare.x && tx <= rSquare.x + rSquare.w && ty >= rSquare.y && ty <= rSquare.y + rSquare.h) {
                     curSat = (float)(tx - rSquare.x) / (float)rSquare.w;
                     curVal = 1.0f - ((float)(ty - rSquare.y) / (float)rSquare.h);
                     if(curSat<0) curSat=0; if(curSat>1) curSat=1; if(curVal<0) curVal=0; if(curVal>1) curVal=1;
                     colorActual = HSVtoRGB(curHue, curSat, curVal);
                }
                else if (delay == 0 && tx >= rUp.x && tx <= rUp.x + rUp.w && ty >= rUp.y && ty <= rUp.y + rUp.h) {
                    if(pincelSize < 50) pincelSize++; delay = 5;
                }
                else if (delay == 0 && tx >= rDown.x && tx <= rDown.x + rDown.w && ty >= rDown.y && ty <= rDown.y + rDown.h) {
                    if(pincelSize > 1) pincelSize--; delay = 5;
                }
                else if (delay == 0 && (tx < sx || tx > sx+BOX_W || ty < sy || ty > sy+BOX_H)) {
                    menuColorOpen = false; delay = 20;
                }
            }
            else if(menuSaveOpen) {
                 if(delay == 0 && (tx < 340 || tx > 940 || ty < 260 || ty > 460)) { menuSaveOpen = false; delay = 20; }
            }
            else {
                if(tx < LIENZO_W) {
                    float scale = std::min((float)LIENZO_W/supWork->w, (float)LIENZO_H/supWork->h);
                    int nw = supWork->w * scale; int nh = supWork->h * scale;
                    int ox = (LIENZO_W - nw)/2; int oy = (LIENZO_H - nh)/2;
                    int ix = (tx - ox) / scale; int iy = (ty - oy) / scale;

                    if(ix >= 0 && ix < supWork->w && iy >= 0 && iy < supWork->h) {
                        if(lastX != -1) {
                            float d = sqrt(pow(ix-lastX,2) + pow(iy-lastY,2));
                            float a = atan2(iy-lastY, ix-lastX);
                            for(float i=0; i<d; i+=1.0f) PintarEnSurface(supWork, lastX+cos(a)*i, lastY+sin(a)*i, pincelSize, colorActual);
                        }
                        PintarEnSurface(supWork, ix, iy, pincelSize, colorActual);
                        necesitaUpdate = true; lastX = ix; lastY = iy;
                    }
                }
                else if(delay == 0) {
                    lastX = -1;
                    if(ty >= btnBrush.y && ty <= btnBrush.y+btnBrush.h) { menuColorOpen = true; delay = 20; }
                    else if(ty >= btnUndo.y && ty <= btnUndo.y+btnUndo.h) { SDL_BlitSurface(supBase, NULL, supWork, NULL); necesitaUpdate = true; delay = 20; }
                    else if(ty >= btnSave.y && ty <= btnSave.y+btnSave.h) { menuSaveOpen = true; delay = 20; }
                }
            }
        } else { lastX = -1; }

        if(necesitaUpdate) { SDL_UpdateTexture(texWork, NULL, supWork->pixels, supWork->pitch); necesitaUpdate = false; }

        if(delay == 0) {
            if(menuSaveOpen) {
                if(vpad.trigger & VPAD_BUTTON_A) { 
                    for(int i=0; i<=90; i+=10) {
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 0,0,0,180); SDL_Rect rDim={0,0,1280,720}; SDL_RenderFillRect(renderer, &rDim);
                        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                        SDL_Rect rBox = {340, 260, 600, 200}; SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &rBox); SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255); SDL_RenderDrawRect(renderer, &rBox);
                        char buf[64]; snprintf(buf, sizeof(buf), esIngles ? "Saving... %d%%" : "Guardando... %d%%", i);
                        if(font) { SDL_Surface* sP = TTF_RenderText_Blended(font, buf, {255,255,255,255}); if(sP){ SDL_Texture* tP=SDL_CreateTextureFromSurface(renderer,sP); SDL_Rect rP={(1280-sP->w)/2, 320, sP->w, sP->h}; SDL_RenderCopy(renderer,tP,NULL,&rP); SDL_FreeSurface(sP); SDL_DestroyTexture(tP); } }
                        SDL_Rect rBarBG = {390, 380, 500, 30}; SDL_SetRenderDrawColor(renderer, 50,50,50,255); SDL_RenderFillRect(renderer, &rBarBG);
                        int wFill = (int)(500.0f * (i/100.0f)); SDL_Rect rFill = {390, 380, wFill, 30}; SDL_SetRenderDrawColor(renderer, 0,255,0,255); SDL_RenderFillRect(renderer, &rFill);
                        SDL_RenderPresent(renderer); SDL_Delay(20);
                    }
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 0,0,0,180); SDL_Rect rDim={0,0,1280,720}; SDL_RenderFillRect(renderer, &rDim); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                    SDL_Rect rBox = {340, 260, 600, 200}; SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &rBox); SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255); SDL_RenderDrawRect(renderer, &rBox);
                    if(font) {
                        SDL_Surface* sP = TTF_RenderText_Blended(font, esIngles ? "Writing to SD..." : "Escribiendo en SD...", {255,255,0,255});
                        if(sP){ SDL_Texture* tP=SDL_CreateTextureFromSurface(renderer,sP); SDL_Rect rP={(1280-sP->w)/2, 320, sP->w, sP->h}; SDL_RenderCopy(renderer,tP,NULL,&rP); SDL_FreeSurface(sP); SDL_DestroyTexture(tP); }
                    }
                    SDL_Rect rBarBG = {390, 380, 500, 30}; SDL_SetRenderDrawColor(renderer, 50,50,50,255); SDL_RenderFillRect(renderer, &rBarBG);
                    SDL_Rect rFill = {390, 380, 450, 30}; SDL_SetRenderDrawColor(renderer, 200,200,0,255); SDL_RenderFillRect(renderer, &rFill); 
                    SDL_RenderPresent(renderer);

                    time_t t = time(NULL); struct tm *tm = localtime(&t);
                    char n[100]; sprintf(n, "fs:/vol/external01/WiiUCamera Files/EDIT_%02d%02d%02d.bmp", tm->tm_hour, tm->tm_min, tm->tm_sec);
                    SDL_SaveBMP(supWork, n); 
                    
                    SDL_SetRenderDrawColor(renderer, 0,255,0,255); SDL_Rect rF={390, 380, 500, 30}; SDL_RenderFillRect(renderer, &rF);
                    SDL_RenderPresent(renderer); SDL_Delay(200);
                    menuSaveOpen = false; guardadoExitoso = true; editando = false;
                }
                if(vpad.trigger & VPAD_BUTTON_B) { menuSaveOpen = false; delay = 20; }
            } else if (menuColorOpen) {
                if(vpad.trigger & VPAD_BUTTON_B) { menuColorOpen = false; delay = 20; }
            } else {
                if(vpad.trigger & VPAD_BUTTON_B) {
                    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderClear(renderer);
                    if(font) {
                        const char* msg = esIngles ? "Exiting... Wait, do not press anything" : "Saliendo... Espere un momento, no presione nada";
                        SDL_Surface* sL = TTF_RenderText_Blended(font, msg, {255,255,255,255});
                        if(sL) { SDL_Texture* tL = SDL_CreateTextureFromSurface(renderer, sL); SDL_Rect rL = {(1280-sL->w)/2, (720-sL->h)/2, sL->w, sL->h}; SDL_RenderCopy(renderer, tL, NULL, &rL); SDL_FreeSurface(sL); SDL_DestroyTexture(tL); }
                    }
                    SDL_RenderPresent(renderer); SDL_Delay(1500); 
                    editando = false; 
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderClear(renderer);
        float sc = std::min((float)LIENZO_W/supWork->w, (float)LIENZO_H/supWork->h);
        int dw = supWork->w * sc; int dh = supWork->h * sc;
        SDL_Rect rDest = {(LIENZO_W - dw)/2, (LIENZO_H - dh)/2, dw, dh};
        SDL_RenderCopy(renderer, texWork, NULL, &rDest);

        SDL_Rect rBar = {LIENZO_W, 0, BAR_W, 720};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderFillRect(renderer, &rBar);
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_RenderDrawLine(renderer, LIENZO_W, 0, LIENZO_W, 720);

        if(tBrushIcon) SDL_RenderCopy(renderer, tBrushIcon, NULL, &btnBrush); else { SDL_SetRenderDrawColor(renderer, 100,100,100,255); SDL_RenderFillRect(renderer, &btnBrush); }
        SDL_Rect rColInd = {btnBrush.x + 44, btnBrush.y + 44, 16, 16};
        SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderFillRect(renderer, &rColInd);
        SDL_Rect rColIn = {rColInd.x+1, rColInd.y+1, 14, 14};
        SDL_SetRenderDrawColor(renderer, colorActual.r, colorActual.g, colorActual.b, 255); SDL_RenderFillRect(renderer, &rColIn);

        if(tUndoIcon) SDL_RenderCopy(renderer, tUndoIcon, NULL, &btnUndo); else { SDL_SetRenderDrawColor(renderer, 150,50,50,255); SDL_RenderFillRect(renderer, &btnUndo); }
        if(tSaveIcon) SDL_RenderCopy(renderer, tSaveIcon, NULL, &btnSave); else { SDL_SetRenderDrawColor(renderer, 50,150,50,255); SDL_RenderFillRect(renderer, &btnSave); }

        if(font) {
            SDL_Surface* sT = TTF_RenderText_Blended(font, "(B) Exit", {150,150,150,255});
            if(sT){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer, sT); SDL_Rect r={1165, 680, sT->w, sT->h}; SDL_RenderCopy(renderer, t, NULL, &r); SDL_FreeSurface(sT); SDL_DestroyTexture(t); }
        }

        if(menuColorOpen) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 0,0,0,180); SDL_Rect rDim={0,0,1280,720}; SDL_RenderFillRect(renderer, &rDim); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            int sx = (1280 - BOX_W)/2; int sy = (720 - BOX_H)/2;
            SDL_Rect rBox = {sx, sy, BOX_W, BOX_H};
            SDL_SetRenderDrawColor(renderer, 50,50,50,255); SDL_RenderFillRect(renderer, &rBox); SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &rBox);
            if(font) {
                SDL_Surface* sT = TTF_RenderText_Blended(font, esIngles ? "Select Color & Size" : "Selecciona Color y Grosor", {255,255,255,255});
                if(sT) { SDL_Texture* tT = SDL_CreateTextureFromSurface(renderer, sT); SDL_Rect rT = {sx + (BOX_W - sT->w)/2, sy + 10, sT->w, sT->h}; SDL_RenderCopy(renderer, tT, NULL, &rT); SDL_FreeSurface(sT); SDL_DestroyTexture(tT); }
            }
            int pad = 25;
            SDL_Rect rSquare = {sx + pad, sy + 50, SV_SIZE, SV_SIZE}; SDL_RenderCopy(renderer, texSquareSV, NULL, &rSquare);
            int curX = rSquare.x + (int)(curSat * SV_SIZE); int curY = rSquare.y + (int)((1.0f - curVal) * SV_SIZE);
            SDL_Rect rCurB = {curX-6, curY-6, 12, 12}; SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &rCurB);
            SDL_Rect rCurW = {curX-5, curY-5, 10, 10}; SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &rCurW);
            SDL_Rect rBarra = {sx + pad, sy + 50 + SV_SIZE + 20, SV_SIZE, BAR_H}; SDL_RenderCopy(renderer, texGradient, NULL, &rBarra);
            int barX = rBarra.x + (int)((curHue/360.0f) * SV_SIZE);
            SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_Rect rInd={barX-2, rBarra.y-4, 4, BAR_H+8}; SDL_RenderFillRect(renderer, &rInd);
            int rx = sx + pad + SV_SIZE + 20; int ry = sy + 50;
            int circleCenterY = ry + 40; int circleCenterX = rx + 60;
            SDL_Rect rPreview = {rx, ry, 120, 80}; SDL_SetRenderDrawColor(renderer, 30,30,30,255); SDL_RenderFillRect(renderer, &rPreview);
            for(int r=0; r<=pincelSize; r++) { for(int y=-r; y<=r; y++) for(int x=-r; x<=r; x++) if(x*x+y*y <= r*r) { SDL_SetRenderDrawColor(renderer, colorActual.r, colorActual.g, colorActual.b, 255); SDL_RenderDrawPoint(renderer, circleCenterX+x, circleCenterY+y); } }
            SDL_Rect rUp = {rx + 35, ry + 90, 50, 50}; SDL_Rect rDown = {rx + 35, ry + 190, 50, 50};
            SDL_SetRenderDrawColor(renderer, 80,80,80,255); SDL_RenderFillRect(renderer, &rUp); SDL_RenderFillRect(renderer, &rDown);
            if(font) {
                SDL_Color cW = {255,255,255,255};
                SDL_Surface* sA = TTF_RenderText_Blended(font, "^", cW); if(sA){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sA); SDL_Rect r={rUp.x+15, rUp.y+5, sA->w, sA->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sA); SDL_DestroyTexture(t); }
                sA = TTF_RenderText_Blended(font, "v", cW); if(sA){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sA); SDL_Rect r={rDown.x+18, rDown.y+5, sA->w, sA->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sA); SDL_DestroyTexture(t); }
                char numBuf[16]; snprintf(numBuf, sizeof(numBuf), "%d", pincelSize);
                sA = TTF_RenderText_Blended(font, numBuf, cW); if(sA){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sA); SDL_Rect r={rx+35 + (50-sA->w)/2, ry + 150, sA->w, sA->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sA); SDL_DestroyTexture(t); }
            }
        }

        if(menuSaveOpen) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 0,0,0,180); SDL_Rect rDim={0,0,1280,720}; SDL_RenderFillRect(renderer, &rDim); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_Rect rBox = {340, 260, 600, 200}; SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &rBox); SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255); SDL_RenderDrawRect(renderer, &rBox);
            if(font) {
                SDL_Surface* s1 = TTF_RenderText_Blended(font, esIngles?"Save Changes?":"Guardar Cambios?", {255,255,255,255}); SDL_Texture* t1 = SDL_CreateTextureFromSurface(renderer, s1); SDL_Rect r1={(1280-s1->w)/2, 300, s1->w, s1->h}; SDL_RenderCopy(renderer, t1, NULL, &r1); SDL_FreeSurface(s1); SDL_DestroyTexture(t1);
                SDL_Surface* s2 = TTF_RenderText_Blended(font, esIngles?"(A) YES    (B) NO":"(A) SI    (B) NO", {0,255,0,255}); SDL_Texture* t2 = SDL_CreateTextureFromSurface(renderer, s2); SDL_Rect r2={(1280-s2->w)/2, 380, s2->w, s2->h}; SDL_RenderCopy(renderer, t2, NULL, &r2); SDL_FreeSurface(s2); SDL_DestroyTexture(t2);
            }
        }

        SDL_RenderPresent(renderer);
    }

    if(tBrushIcon) SDL_DestroyTexture(tBrushIcon); if(tUndoIcon) SDL_DestroyTexture(tUndoIcon); if(tSaveIcon) SDL_DestroyTexture(tSaveIcon);
    SDL_DestroyTexture(texGradient); SDL_DestroyTexture(texSquareSV);
    SDL_DestroyTexture(texWork); SDL_FreeSurface(supBase); SDL_FreeSurface(supWork);
    
    return guardadoExitoso;
}

// --- SELECTOR (GRID) ---
int EjecutarEditor(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    std::vector<EditorFile> archivos = EscanearTodoParaEditor();
    const int COLS=4, CELL_W=280, CELL_H=210, GAP=20, MARGIN_X=50, MARGIN_TOP=80;
    int sel=0; float scroll=0; bool salir=false; int delay=0;
    std::map<int, SDL_Texture*> cache;
    int tStartY = -1; 

    // FIX: Agregado WHBProcIsRunning()
    while(!salir && WHBProcIsRunning()) {
        VPADStatus vpad; VPADReadError e; VPADRead(VPAD_CHAN_0, &vpad, 1, &e);
        SDL_Event ev; while(SDL_PollEvent(&ev)){ if(ev.type==SDL_QUIT) salir=true; }
        if(delay>0) delay--;
        int tot = archivos.size();

        if(vpad.tpNormal.touched) {
            int ty = (int)((1.0f - (vpad.tpNormal.y - 100.0f) / 3800.0f) * 720.0f);
            if(tStartY == -1) tStartY = ty;
            else { int delta = ty - tStartY; if(abs(delta) > 5) { scroll -= delta; tStartY = ty; } }
        } else { tStartY = -1; }

        if(delay == 0 && tot > 0) {
            if(((vpad.hold&VPAD_BUTTON_RIGHT)||vpad.leftStick.x>0.5) && sel<tot-1){ sel++; delay=8; } 
            if(((vpad.hold&VPAD_BUTTON_LEFT)||vpad.leftStick.x<-0.5) && sel>0){ sel--; delay=8; }
            if(((vpad.hold&VPAD_BUTTON_DOWN)||vpad.leftStick.y<-0.5) && sel+COLS<tot){ sel+=COLS; delay=8; } 
            if(((vpad.hold&VPAD_BUTTON_UP)||vpad.leftStick.y>0.5) && sel-COLS>=0){ sel-=COLS; delay=8; }
            
            if(vpad.trigger & VPAD_BUTTON_A) {
                for(auto const& [k,v]:cache) SDL_DestroyTexture(v); cache.clear();
                
                // NOTA: Se llama a la funcion de main (sin ambiguedad)
                bool guardado = EjecutarLogicaDibujo(renderer, font, archivos[sel].ruta, esIngles);
                if(guardado) {
                    // Llamada al simbolo global
                    EjecutarGaleria(renderer, font, esIngles, 0, NULL, false); 
                    salir = true; 
                } else {
                    delay = 30;
                }
            }
        }
        if(vpad.trigger & VPAD_BUTTON_B) salir = true;

        int maxH = (tot/COLS + 1) * (CELL_H + GAP) + MARGIN_TOP;
        if(scroll < 0) scroll = 0; if(scroll > maxH - 600) scroll = maxH - 600; if(scroll < 0) scroll = 0;

        if(tot>0) { int rs=sel/COLS; int yt=MARGIN_TOP+rs*(CELL_H+GAP); int yb=yt+CELL_H; if(yt<scroll+MARGIN_TOP) scroll=yt-MARGIN_TOP; if(yb>scroll+600) scroll=yb-600; }
        int minI=(scroll/(CELL_H+GAP))*COLS; int maxI=minI + (720/(CELL_H+GAP) + 2)*COLS;
        for(auto it=cache.begin(); it!=cache.end(); ) { if(it->first < minI || it->first > maxI) { SDL_DestroyTexture(it->second); it=cache.erase(it); } else ++it; }
        for(int i=minI; i<maxI; i++) { if(i>=0 && i<tot && cache.find(i)==cache.end()) { SDL_Surface* s=IMG_Load(archivos[i].ruta.c_str()); if(s){ cache[i]=SDL_CreateTextureFromSurface(renderer,s); SDL_FreeSurface(s); } } }

        SDL_SetRenderDrawColor(renderer,20,20,25,255); SDL_RenderClear(renderer);
        for(int i=minI; i<maxI; i++) {
            if(i>=tot) break; int c=i%COLS; int r=i/COLS; int x=MARGIN_X+c*(CELL_W+GAP); int y=MARGIN_TOP+r*(CELL_H+GAP)-(int)scroll; if(y+CELL_H<0 || y>720) continue;
            SDL_Rect cr={x,y,CELL_W,CELL_H}; SDL_SetRenderDrawColor(renderer,40,40,40,255); SDL_RenderFillRect(renderer,&cr);
            if(cache.count(i)) { SDL_Rect pr={x+5,y+5,CELL_W-10,CELL_H-10}; SDL_RenderCopy(renderer,cache[i],NULL,&pr); }
            if(i==sel) { SDL_SetRenderDrawColor(renderer,255,230,0,255); SDL_Rect b1={x-4,y-4,CELL_W+8,CELL_H+8}; SDL_RenderDrawRect(renderer,&b1); }
        }

        SDL_SetRenderDrawColor(renderer,30,30,35,255); SDL_Rect rh={0,0,1280,70}; SDL_RenderFillRect(renderer,&rh);
        const char* tT = esIngles ? "Editor - Select Image" : "Editor - Seleccionar Imagen";
        SDL_Surface* st=TTF_RenderText_Blended(font,tT,{255,255,255,255}); if(st){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,st); SDL_Rect r={50,20,st->w,st->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(st); SDL_DestroyTexture(t); }
        
        SDL_Rect rb={0,620,1280,100}; SDL_SetRenderDrawColor(renderer,0,0,0,220); SDL_RenderFillRect(renderer,&rb);
        SDL_Surface* sb1=TTF_RenderText_Blended(font, esIngles?"(A) Edit":"(A) Editar", {0,255,0,255});
        if(sb1){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sb1); SDL_Rect r={50,650,sb1->w,sb1->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sb1); SDL_DestroyTexture(t); }
        SDL_Surface* sb2=TTF_RenderText_Blended(font, "(B) Back", {200,200,200,255});
        if(sb2){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sb2); SDL_Rect r={1100,650,sb2->w,sb2->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sb2); SDL_DestroyTexture(t); }

        SDL_RenderPresent(renderer);
    }
    for(auto const& [k,v]:cache) SDL_DestroyTexture(v); 
    return 0;
}
#endif