#ifndef EDITOR_H
#define EDITOR_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <vpad/input.h>
#include <vector>
#include <list>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <time.h>
#include <math.h> 

// --- CONSTANTES HD ---
const int GP_W = 1280;
const int GP_H = 720;
const int BARRA_W = 150;
const int LIENZO_W = GP_W - BARRA_W;
const int LIENZO_H = GP_H;

// Paleta
SDL_Color g_Paleta[] = {
    {0,0,0,255},       {255,255,255,255}, {128,128,128,255},
    {255,0,0,255},     {0,255,0,255},     {0,0,255,255},
    {255,255,0,255},   {0,255,255,255},   {255,0,255,255},
    {255,128,0,255},   {128,0,255,255},   {0,128,0,255},
    {139,69,19,255},   {255,192,203,255}, {0,128,128,255}
};

int g_lastX = -1; int g_lastY = -1;
SDL_Color g_brushColor = {255, 0, 0, 255}; 

const float ED_ADC_MIN_X = 100.0f; const float ED_ADC_MAX_X = 3950.0f;
const float ED_ADC_MIN_Y = 100.0f; const float ED_ADC_MAX_Y = 3900.0f;

struct FotoFile { std::string nombre; std::string ruta; SDL_Texture* thumb; SDL_Rect area; };

int MapEdit(float val, float min, float max, float outMax, bool inv) {
    float pct = (val - min) / (max - min);
    if (pct < 0) pct = 0; if (pct > 1) pct = 1;
    if (inv) pct = 1.0f - pct;
    return (int)(pct * outMax);
}

// Utiles
Uint8 clamp255(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (Uint8)v;
}

void DibujarBarraEditor(SDL_Renderer* renderer, TTF_Font* font, bool esSelector) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); 
    SDL_Rect rBarra = {0, 660, GP_W, 60}; 
    SDL_RenderFillRect(renderer, &rBarra);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_Color cTxt = {255, 255, 255, 255}; 
    char texto[200];
    
    // --- CAMBIO SOLICITADO: TEXTOS MAS LIMPIOS ---
    if (esSelector) sprintf(texto, "(A) Select   (Stick) Move");
    else sprintf(texto, "(Touch) Edit   (B) Exit");

    SDL_Surface* sT = TTF_RenderText_Blended(font, texto, cTxt);
    if(sT){
        int tX = (GP_W - sT->w) / 2;
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, sT);
        SDL_Rect r = {tX, 675, sT->w, sT->h}; SDL_RenderCopy(renderer, t, NULL, &r);
        SDL_FreeSurface(sT); SDL_DestroyTexture(t);
    }
}

// --- EL MINION (RAM WRITER) ---
void PutPixelRAM(SDL_Surface *surface, int x, int y, Uint32 pixel) {
    if (x < 0 || x >= surface->w || y < 0 || y >= surface->h) return;
    int bpp = surface->format->BytesPerPixel;
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
    *(Uint32 *)p = pixel;
}

// Dibuja en GPU (pantalla) y en RAM (Minion) a la vez
void DibujarTrazoDual(SDL_Renderer* renderer, SDL_Surface* surfaceRAM, int x1, int y1, int x2, int y2, int radio, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    Uint32 pixelRAM = SDL_MapRGBA(surfaceRAM->format, color.r, color.g, color.b, color.a);
    float distance = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
    if (distance == 0) distance = 1;
    for (float i = 0; i <= distance; i += 1.0f) {
        float t = i / distance;
        int cx = x1 + (int)((x2 - x1) * t);
        int cy = y1 + (int)((y2 - y1) * t);
        for (int w = 0; w < radio * 2; w++) {
            for (int h = 0; h < radio * 2; h++) {
                int dx = radio - w; int dy = radio - h;
                if ((dx*dx + dy*dy) <= (radio * radio)) {
                    SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
                    PutPixelRAM(surfaceRAM, cx + dx, cy + dy, pixelRAM);
                }
            }
        }
    }
}

// --- LOGICA DE EFECTOS ---
void AplicarEfectoColor(SDL_Surface* surface, int tipo) {
    if (!surface) return;
    if (SDL_LockSurface(surface) < 0) return;

    Uint32* pixels = (Uint32*)surface->pixels;
    int count = surface->w * surface->h;
    SDL_PixelFormat* fmt = surface->format;

    int brilloStep = 20;
    float contrasteFactor = 1.2f; 
    if (tipo == 4) contrasteFactor = 0.8f; 

    for (int i = 0; i < count; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(pixels[i], fmt, &r, &g, &b, &a);

        if (tipo == 1) { r = clamp255(r + brilloStep); g = clamp255(g + brilloStep); b = clamp255(b + brilloStep); }
        else if (tipo == 2) { r = clamp255(r - brilloStep); g = clamp255(g - brilloStep); b = clamp255(b - brilloStep); }
        else if (tipo == 3 || tipo == 4) { 
            r = clamp255((int)((r - 128) * contrasteFactor + 128));
            g = clamp255((int)((g - 128) * contrasteFactor + 128));
            b = clamp255((int)((b - 128) * contrasteFactor + 128));
        }
        else if (tipo == 5) { 
            Uint8 luma = (Uint8)(0.3*r + 0.59*g + 0.11*b);
            r = luma; g = luma; b = luma;
        }
        pixels[i] = SDL_MapRGBA(fmt, r, g, b, a);
    }
    SDL_UnlockSurface(surface);
}

// --- GUARDADO MINION MANUAL ---
void GuardarMinionManual(SDL_Renderer* renderer, SDL_Surface* surfaceRAM, TTF_Font* font, bool esIngles) {
    // Feedback
    SDL_SetRenderTarget(renderer, NULL);
    SDL_SetRenderDrawColor(renderer, 0,0,0,200);
    SDL_Rect rBox = {340, 310, 600, 100}; SDL_RenderFillRect(renderer, &rBox);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &rBox);
    
    char progressText[100];
    if(esIngles) sprintf(progressText, "Saving... (Safe Mode)"); else sprintf(progressText, "Guardando... (Modo Seguro)");
    
    SDL_Color cW = {255,255,255,255};
    SDL_Surface* sT = TTF_RenderText_Blended(font, progressText, cW);
    SDL_Texture* tT = SDL_CreateTextureFromSurface(renderer, sT);
    SDL_Rect rT = {640 - sT->w/2, 360 - sT->h/2, sT->w, sT->h};
    SDL_RenderCopy(renderer, tT, NULL, &rT); SDL_RenderPresent(renderer);
    SDL_FreeSurface(sT); SDL_DestroyTexture(tT);

    // BMP
    int w = surfaceRAM->w; int h = surfaceRAM->h;
    time_t t = time(NULL); int r = rand() % 1000;
    char filename[256]; sprintf(filename, "fs:/vol/external01/WiiUCamera Files/EDIT_%ld_%03d.bmp", t, r);
    FILE* f = fopen(filename, "wb");
    if (!f) return;

    unsigned char bmpFileHeader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
    unsigned char bmpInfoHeader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
    int padding = (4 - (w * 3) % 4) % 4;
    int filesize = 54 + (w * 3 + padding) * h;

    bmpFileHeader[2] = (unsigned char)(filesize); bmpFileHeader[3] = (unsigned char)(filesize>>8);
    bmpFileHeader[4] = (unsigned char)(filesize>>16); bmpFileHeader[5] = (unsigned char)(filesize>>24);
    bmpInfoHeader[4] = (unsigned char)(w); bmpInfoHeader[5] = (unsigned char)(w>>8);
    bmpInfoHeader[6] = (unsigned char)(w>>16); bmpInfoHeader[7] = (unsigned char)(w>>24);
    int negH = -h; 
    bmpInfoHeader[8] = (unsigned char)(negH); bmpInfoHeader[9] = (unsigned char)(negH>>8);
    bmpInfoHeader[10] = (unsigned char)(negH>>16); bmpInfoHeader[11] = (unsigned char)(negH>>24);

    fwrite(bmpFileHeader, 1, 14, f); fwrite(bmpInfoHeader, 1, 40, f);
    unsigned char pad[3] = {0,0,0};
    
    SDL_LockSurface(surfaceRAM);
    Uint8* pixels = (Uint8*)surfaceRAM->pixels;
    int pitch = surfaceRAM->pitch;
    SDL_PixelFormat* fmt = surfaceRAM->format;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            Uint32 pixel = *((Uint32*)(pixels + y * pitch + x * 4));
            Uint8 r, g, b, a;
            SDL_GetRGBA(pixel, fmt, &r, &g, &b, &a);
            fputc(b, f); fputc(g, f); fputc(r, f); 
        }
        fwrite(pad, 1, padding, f);
    }
    SDL_UnlockSurface(surfaceRAM);
    fclose(f);
    SDL_Delay(500); 
}

std::string SeleccionarFotoParaEditar(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    std::vector<FotoFile> fotos;
    DIR* dir = opendir("fs:/vol/external01/WiiUCamera Files");
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string n = ent->d_name; if (n.length() > 4) {
                std::string ext = n.substr(n.length()-4);
                if (ext == ".bmp" || ext == ".jpg" || ext == ".png") {
                    FotoFile f; f.nombre = n; f.ruta = "fs:/vol/external01/WiiUCamera Files/" + n; f.thumb = NULL; fotos.push_back(f);
                }
            }
        } closedir(dir);
    }
    
    bool seleccionando = true; std::string rutaElegida = ""; int scroll = 0; int touchStartY = -1;
    int indexSeleccionado = 0; int delayInput = 0; int columnas = 4;

    while (seleccionando) {
        SDL_Event event; while(SDL_PollEvent(&event));
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
        
        if (delayInput > 0) delayInput--;
        if (vpad.trigger & VPAD_BUTTON_HOME) { exit(0); }
        if (vpad.trigger & VPAD_BUTTON_B) seleccionando = false;
        
        if (delayInput == 0 && !fotos.empty()) {
            if (vpad.hold & VPAD_BUTTON_RIGHT) { if (indexSeleccionado < fotos.size() - 1) indexSeleccionado++; delayInput = 10; }
            if (vpad.hold & VPAD_BUTTON_LEFT) { if (indexSeleccionado > 0) indexSeleccionado--; delayInput = 10; }
            if (vpad.hold & VPAD_BUTTON_DOWN) { if (indexSeleccionado + columnas < fotos.size()) indexSeleccionado += columnas; delayInput = 10; }
            if (vpad.hold & VPAD_BUTTON_UP) { if (indexSeleccionado - columnas >= 0) indexSeleccionado -= columnas; delayInput = 10; }
            if (vpad.trigger & VPAD_BUTTON_A) { rutaElegida = fotos[indexSeleccionado].ruta; seleccionando = false; }
        }

        if (vpad.tpNormal.touched) {
            int tx = MapEdit(vpad.tpNormal.x, ED_ADC_MIN_X, ED_ADC_MAX_X, GP_W, false);
            int ty = MapEdit(vpad.tpNormal.y, ED_ADC_MIN_Y, ED_ADC_MAX_Y, GP_H, true);
            if (touchStartY == -1) touchStartY = ty;
            if (ty < 660 && delayInput == 0) {
                for (int i=0; i<fotos.size(); i++) {
                    if (tx >= fotos[i].area.x && tx <= (fotos[i].area.x + fotos[i].area.w) && ty >= fotos[i].area.y && ty <= (fotos[i].area.y + fotos[i].area.h)) {
                        indexSeleccionado = i; rutaElegida = fotos[i].ruta; seleccionando = false;
                    }
                }
            }
            if (touchStartY != -1) { int delta = ty - touchStartY; if (abs(delta) > 5) { scroll -= delta; if(scroll < 0) scroll = 0; touchStartY = ty; } }
        } else { touchStartY = -1; }
        
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderClear(renderer);
        SDL_Color colText = {255,255,255,255};
        SDL_Surface* sT = TTF_RenderText_Blended(font, esIngles ? "Select Photo" : "Elige Foto", colText);
        SDL_Texture* tT = SDL_CreateTextureFromSurface(renderer, sT);
        SDL_Rect rT = {40, 20, sT->w, sT->h}; SDL_RenderCopy(renderer, tT, NULL, &rT); SDL_FreeSurface(sT); SDL_DestroyTexture(tT);

        int w = 250; int h = 200; int gap = 20; int startY = 100;
        int filaSel = indexSeleccionado / columnas; int ySel = startY + (filaSel * (h+gap));
        if (ySel - scroll > 600) scroll = ySel - 500;
        if (ySel - scroll < 100) scroll = ySel - 100; if (scroll < 0) scroll = 0;

        for (int i=0; i<fotos.size(); i++) {
            int c = i % columnas; int r = i / columnas; 
            int x = 50 + (c * (w+gap)); int y = startY + (r * (h+gap)) - scroll;
            fotos[i].area = {x, y, w, h};
            if (y + h > 0 && y < 720) {
                if (!fotos[i].thumb) {
                    SDL_Surface* s = IMG_Load(fotos[i].ruta.c_str());
                    if (s) { SDL_Surface* s2 = SDL_CreateRGBSurface(0, 160, 120, 32, 0,0,0,0); SDL_BlitScaled(s, NULL, s2, NULL); fotos[i].thumb = SDL_CreateTextureFromSurface(renderer, s2); SDL_FreeSurface(s); SDL_FreeSurface(s2); }
                }
                if (i == indexSeleccionado) SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255); else SDL_SetRenderDrawColor(renderer, 100,100,100,255);
                SDL_RenderFillRect(renderer, &fotos[i].area);
                if (fotos[i].thumb) { SDL_Rect rImg = {x+5, y+5, w-10, h-10}; SDL_RenderCopy(renderer, fotos[i].thumb, NULL, &rImg); }
            }
        }
        DibujarBarraEditor(renderer, font, true);
        SDL_RenderPresent(renderer);
    }
    for(auto& f : fotos) if(f.thumb) SDL_DestroyTexture(f.thumb);
    return rutaElegida;
}

void EjecutarEditor(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    std::string ruta = SeleccionarFotoParaEditar(renderer, font, esIngles);
    if (ruta == "") return;

    SDL_Texture* iconBrush = IMG_LoadTexture(renderer, "/vol/content/icon_brush.png");
    SDL_Texture* iconFx    = IMG_LoadTexture(renderer, "/vol/content/icon_fx.png");
    SDL_Texture* iconUndo  = IMG_LoadTexture(renderer, "/vol/content/icon_undo.png");
    SDL_Texture* iconSave  = IMG_LoadTexture(renderer, "/vol/content/icon_save.png");

    SDL_Surface* surfOrig = IMG_Load(ruta.c_str()); if (!surfOrig) return;
    
    // --- MINION SETUP (RGBA8888 Forzado) ---
    SDL_Texture* texLienzo = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 1280, 720);
    SDL_Surface* surfMinion = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA8888);
    
    // Inicializar
    SDL_SetRenderTarget(renderer, texLienzo);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer); 
    float ratio = (float)surfOrig->w / (float)surfOrig->h;
    int drawW = LIENZO_W; int drawH = (int)(LIENZO_W / ratio);
    if (drawH > 720) { drawH = 720; drawW = (int)(720 * ratio); }
    SDL_Rect rFoto = { (LIENZO_W - drawW)/2, (720 - drawH)/2, drawW, drawH };
    
    SDL_Texture* tempTex = SDL_CreateTextureFromSurface(renderer, surfOrig);
    SDL_RenderCopy(renderer, tempTex, NULL, &rFoto);
    // Pintamos la foto original en el Minion (RAM)
    SDL_BlitScaled(surfOrig, NULL, surfMinion, &rFoto);
    
    SDL_DestroyTexture(tempTex); SDL_FreeSurface(surfOrig);
    SDL_SetRenderTarget(renderer, NULL); 

    bool editando = true; bool confirmandoSalida = false;
    bool herramientaPincel = true; 
    bool mostrarPaleta = false; 
    bool mostrarEfectos = false; 

    while (editando) {
        SDL_Event event; while(SDL_PollEvent(&event));
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (confirmandoSalida) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); SDL_Rect rDim = {0,0,GP_W,GP_H}; SDL_RenderFillRect(renderer, &rDim);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_Rect rBox = {340, 260, 600, 200}; SDL_RenderFillRect(renderer, &rBox);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &rBox);
            SDL_Color cW = {255,255,255,255};
            SDL_Surface* sT = TTF_RenderText_Blended(font, esIngles ? "Exit without saving?" : "Salir sin guardar?", cW);
            SDL_Texture* tT = SDL_CreateTextureFromSurface(renderer, sT); SDL_Rect rTxt = {640 - sT->w/2, 300, sT->w, sT->h};
            SDL_RenderCopy(renderer, tT, NULL, &rTxt); SDL_FreeSurface(sT); SDL_DestroyTexture(tT);
            SDL_Surface* sOp = TTF_RenderText_Blended(font, esIngles ? "(A) Yes   (B) No" : "(A) Si   (B) No", cW);
            SDL_Texture* tOp = SDL_CreateTextureFromSurface(renderer, sOp); SDL_Rect rOp = {640 - sOp->w/2, 380, sOp->w, sOp->h};
            SDL_RenderCopy(renderer, tOp, NULL, &rOp); SDL_FreeSurface(sOp); SDL_DestroyTexture(tOp);
            SDL_RenderPresent(renderer);
            if (vpad.trigger & VPAD_BUTTON_A) editando = false; 
            if (vpad.trigger & VPAD_BUTTON_B) confirmandoSalida = false; 
            continue; 
        }
        if (vpad.trigger & VPAD_BUTTON_HOME) { exit(0); }
        if (vpad.trigger & VPAD_BUTTON_B) confirmandoSalida = true;

        int tx = -100, ty = -100; bool tocando = false;
        if (vpad.tpNormal.touched) {
            tx = MapEdit(vpad.tpNormal.x, ED_ADC_MIN_X, ED_ADC_MAX_X, GP_W, false);
            ty = MapEdit(vpad.tpNormal.y, ED_ADC_MIN_Y, ED_ADC_MAX_Y, GP_H, true);
            tocando = true;
        } else { g_lastX = -1; g_lastY = -1; }

        if (tocando) {
            // 1. PALETA COLOR
            if (mostrarPaleta) {
                int palX = LIENZO_W - 220; int palY = 50;
                if (tx >= palX && tx <= palX + 200 && ty >= palY && ty <= palY + 270) {
                    int boxS = 40; int gap = 10;
                    for(int i=0; i<15; i++) {
                        int c = i % 3; int r = i / 3; int bx = palX + 20 + (c * (boxS+gap)); int by = palY + 20 + (r * (boxS+gap));
                        if (tx >= bx && tx <= bx+boxS && ty >= by && ty <= by+boxS) {
                            g_brushColor = g_Paleta[i]; mostrarPaleta = false; SDL_Delay(200); tocando = false;
                        }
                    }
                } else { mostrarPaleta = false; }
            }
            // 2. POPUP EFECTOS
            else if (mostrarEfectos) {
                int fxX = LIENZO_W - 280; int fxY = 100; 
                if (tx >= fxX && tx <= fxX + 260 && ty >= fxY && ty <= fxY + 400) {
                    int efectoElegido = 0;
                    // B+ B- C+ C- BW
                    if (ty > fxY+30 && ty < fxY+70) efectoElegido = (tx < fxX + 130) ? 2 : 1;
                    else if (ty > fxY+90 && ty < fxY+130) efectoElegido = (tx < fxX + 130) ? 4 : 3;
                    else if (ty > fxY+150 && ty < fxY+190) efectoElegido = 5;

                    if (efectoElegido > 0) {
                        AplicarEfectoColor(surfMinion, efectoElegido);
                        SDL_UpdateTexture(texLienzo, NULL, surfMinion->pixels, surfMinion->pitch);
                        mostrarEfectos = false; 
                        SDL_Delay(200); 
                        tocando = false;
                    }
                } else { mostrarEfectos = false; }
            }
            // 3. BARRA LATERAL
            else if (tx > LIENZO_W) {
                if (ty < 100) { herramientaPincel = true; mostrarPaleta = !mostrarPaleta; mostrarEfectos = false; SDL_Delay(200); }
                else if (ty > 120 && ty < 220) { mostrarEfectos = !mostrarEfectos; mostrarPaleta = false; SDL_Delay(200); }
                else if (ty > 240 && ty < 340) { 
                    // BORRADOR SUPREMO
                    SDL_Surface* surfOrigRe = IMG_Load(ruta.c_str());
                    SDL_Texture* tempTexRe = SDL_CreateTextureFromSurface(renderer, surfOrigRe);
                    SDL_SetRenderTarget(renderer, texLienzo); SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderClear(renderer); SDL_RenderCopy(renderer, tempTexRe, NULL, &rFoto);
                    SDL_SetRenderTarget(renderer, NULL); 
                    SDL_FillRect(surfMinion, NULL, 0xFF000000); SDL_BlitScaled(surfOrigRe, NULL, surfMinion, &rFoto);
                    SDL_DestroyTexture(tempTexRe); SDL_FreeSurface(surfOrigRe); SDL_Delay(200); 
                }
                else if (ty > 550) { GuardarMinionManual(renderer, surfMinion, font, esIngles); SDL_Delay(1000); editando = false; }
            }
            // 4. DIBUJAR
            else if (herramientaPincel && !mostrarPaleta && !mostrarEfectos) {
                if (g_lastX != -1) {
                    SDL_SetRenderTarget(renderer, texLienzo); 
                    DibujarTrazoDual(renderer, surfMinion, g_lastX, g_lastY, tx, ty, 5, g_brushColor); 
                    SDL_SetRenderTarget(renderer, NULL);
                }
                g_lastX = tx; g_lastY = ty;
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texLienzo, NULL, NULL);
        
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_Rect rBar = {LIENZO_W, 0, BARRA_W, 720}; SDL_RenderFillRect(renderer, &rBar); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        if (iconBrush) { SDL_Rect rB = {LIENZO_W + 20, 20, 70, 70}; SDL_RenderCopy(renderer, iconBrush, NULL, &rB); 
            SDL_SetRenderDrawColor(renderer, g_brushColor.r, g_brushColor.g, g_brushColor.b, 255); SDL_Rect rC = {rB.x+50, rB.y+50, 20,20}; SDL_RenderFillRect(renderer, &rC); }
        
        if (iconFx) { SDL_Rect rF = {LIENZO_W + 20, 120, 70, 70}; SDL_RenderCopy(renderer, iconFx, NULL, &rF); }
        
        if (iconUndo) { SDL_Rect rU = {LIENZO_W + 20, 240, 70, 70}; SDL_RenderCopy(renderer, iconUndo, NULL, &rU); }
        if (iconSave) { SDL_Rect rS = {LIENZO_W + 20, 550, 70, 70}; SDL_RenderCopy(renderer, iconSave, NULL, &rS); }

        if (mostrarPaleta) {
            int palX = LIENZO_W - 220; int palY = 50; SDL_Rect rPal = {palX, palY, 200, 270};
            SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255); SDL_RenderFillRect(renderer, &rPal); SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &rPal);
            int boxS = 40; int gap = 10;
            for(int i=0; i<15; i++) {
                int c = i % 3; int r = i / 3; int bx = palX + 20 + (c * (boxS+gap)); int by = palY + 20 + (r * (boxS+gap));
                SDL_Rect rBox = {bx, by, boxS, boxS}; SDL_SetRenderDrawColor(renderer, g_Paleta[i].r, g_Paleta[i].g, g_Paleta[i].b, 255); SDL_RenderFillRect(renderer, &rBox);
            }
        }
        
        if (mostrarEfectos) {
            int fxX = LIENZO_W - 280; int fxY = 100; 
            SDL_Rect rFx = {fxX, fxY, 260, 280};
            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderFillRect(renderer, &rFx); 
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); SDL_RenderDrawRect(renderer, &rFx);
            
            SDL_Color cW = {255,255,255,255};
            SDL_Surface* sT = TTF_RenderText_Blended(font, "Effects / Efectos", cW); SDL_Texture* tT = SDL_CreateTextureFromSurface(renderer, sT);
            SDL_Rect rTi = {fxX+10, fxY+5, sT->w, sT->h}; SDL_RenderCopy(renderer, tT, NULL, &rTi); SDL_FreeSurface(sT); SDL_DestroyTexture(tT);

            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_Rect b1 = {fxX+10, fxY+30, 80, 40}; SDL_RenderFillRect(renderer, &b1); 
            SDL_Rect b2 = {fxX+110, fxY+30, 80, 40}; SDL_RenderFillRect(renderer, &b2);
            SDL_Rect b3 = {fxX+10, fxY+90, 80, 40}; SDL_RenderFillRect(renderer, &b3);
            SDL_Rect b4 = {fxX+110, fxY+90, 80, 40}; SDL_RenderFillRect(renderer, &b4);
            SDL_Rect b5 = {fxX+10, fxY+150, 180, 40}; SDL_RenderFillRect(renderer, &b5);

            sT = TTF_RenderText_Blended(font, "Bri +", cW); tT = SDL_CreateTextureFromSurface(renderer, sT); SDL_Rect r1 = {b1.x+10, b1.y+5, sT->w, sT->h}; SDL_RenderCopy(renderer, tT, NULL, &r1); SDL_FreeSurface(sT); SDL_DestroyTexture(tT);
            sT = TTF_RenderText_Blended(font, "Bri -", cW); tT = SDL_CreateTextureFromSurface(renderer, sT); SDL_Rect r2 = {b2.x+10, b2.y+5, sT->w, sT->h}; SDL_RenderCopy(renderer, tT, NULL, &r2); SDL_FreeSurface(sT); SDL_DestroyTexture(tT);
            sT = TTF_RenderText_Blended(font, "Con +", cW); tT = SDL_CreateTextureFromSurface(renderer, sT); SDL_Rect r3 = {b3.x+10, b3.y+5, sT->w, sT->h}; SDL_RenderCopy(renderer, tT, NULL, &r3); SDL_FreeSurface(sT); SDL_DestroyTexture(tT);
            sT = TTF_RenderText_Blended(font, "Con -", cW); tT = SDL_CreateTextureFromSurface(renderer, sT); SDL_Rect r4 = {b4.x+10, b4.y+5, sT->w, sT->h}; SDL_RenderCopy(renderer, tT, NULL, &r4); SDL_FreeSurface(sT); SDL_DestroyTexture(tT);
            sT = TTF_RenderText_Blended(font, "B & W", cW); tT = SDL_CreateTextureFromSurface(renderer, sT); SDL_Rect r5 = {b5.x+60, b5.y+5, sT->w, sT->h}; SDL_RenderCopy(renderer, tT, NULL, &r5); SDL_FreeSurface(sT); SDL_DestroyTexture(tT);
        }

        DibujarBarraEditor(renderer, font, false);
        SDL_RenderPresent(renderer);
    }
    if (iconBrush) SDL_DestroyTexture(iconBrush); if (iconSave) SDL_DestroyTexture(iconSave); 
    if (iconUndo) SDL_DestroyTexture(iconUndo); if (iconFx) SDL_DestroyTexture(iconFx);
    SDL_DestroyTexture(texLienzo); SDL_FreeSurface(surfMinion);
}

#endif