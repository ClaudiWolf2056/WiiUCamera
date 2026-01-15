#ifndef GALLERY_H
#define GALLERY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <vector>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdio> 

#define VID_W 640
#define VID_H 480
#define VID_SIZE (VID_W * VID_H * 4)

// Constantes HD
const float G_APP_W = 1280.0f; const float G_APP_H = 720.0f;

// Mapeo Tactil
const float G_ADC_MIN_X = 100.0f; const float G_ADC_MAX_X = 3950.0f;
const float G_ADC_MIN_Y = 100.0f; const float G_ADC_MAX_Y = 3900.0f;

int MapCoord(float val, float min, float max, float outMax, bool inv) {
    float pct = (val - min) / (max - min);
    if (pct < 0) pct = 0; if (pct > 1) pct = 1;
    if (inv) pct = 1.0f - pct;
    return (int)(pct * outMax);
}

struct ArchivoMedia { std::string nombre; std::string rutaCompleta; bool esVideo; SDL_Texture* texturaMiniatura; SDL_Rect rectArea; };

std::vector<ArchivoMedia> g_listaMedia;
int g_scrollGaleria = 0; int g_itemSeleccionado = 0;

bool EsExtension(const std::string& nombre, const std::string& ext) {
    if (nombre.length() >= ext.length()) { return (0 == nombre.compare(nombre.length() - ext.length(), ext.length(), ext)); }
    return false;
}

bool TocoRect(int tx, int ty, SDL_Rect r) { return (tx >= r.x && tx <= (r.x + r.w) && ty >= r.y && ty <= (r.y + r.h)); }

void DibujarBarraInferior(SDL_Renderer* renderer, TTF_Font* font, const char* textoIzq, const char* textoDer) {
    // Barra mas alta y oscura para que se lea bien
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 0, 0, 0, 240); 
    SDL_Rect rBarra = {0, 640, 1280, 80}; SDL_RenderFillRect(renderer, &rBarra); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    
    SDL_Color colorTexto = {255, 255, 255, 255}; 
    if (textoIzq && font) {
        SDL_Surface* sIzq = TTF_RenderText_Blended(font, textoIzq, colorTexto);
        if (sIzq) { SDL_Texture* tIzq = SDL_CreateTextureFromSurface(renderer, sIzq); SDL_Rect rIzq = {40, 665, sIzq->w, sIzq->h}; SDL_RenderCopy(renderer, tIzq, NULL, &rIzq); SDL_FreeSurface(sIzq); SDL_DestroyTexture(tIzq); }
    }
    if (textoDer && font) {
        SDL_Surface* sDer = TTF_RenderText_Blended(font, textoDer, colorTexto);
        if (sDer) { SDL_Texture* tDer = SDL_CreateTextureFromSurface(renderer, sDer); SDL_Rect rDer = {1240 - sDer->w, 665, sDer->w, sDer->h}; SDL_RenderCopy(renderer, tDer, NULL, &rDer); SDL_FreeSurface(sDer); SDL_DestroyTexture(tDer); }
    }
}

void VerFoto(SDL_Renderer* renderer, TTF_Font* font, const std::string& ruta, bool esIngles) {
    SDL_Surface* surf = IMG_Load(ruta.c_str()); if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf); SDL_FreeSurface(surf);
    bool viendo = true;
    while (viendo) {
        SDL_Event event; while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) viendo = false; }
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
        if (vpad.trigger & VPAD_BUTTON_B) viendo = false;
        if (vpad.tpNormal.touched) {
            int tx = MapCoord(vpad.tpNormal.x, G_ADC_MIN_X, G_ADC_MAX_X, G_APP_W, false);
            int ty = MapCoord(vpad.tpNormal.y, G_ADC_MIN_Y, G_ADC_MAX_Y, G_APP_H, true);
            if (ty > 640 && tx > 1100) viendo = false;
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
        
        // Ajuste perfecto a pantalla (Fit)
        int w, h; SDL_QueryTexture(tex, NULL, NULL, &w, &h); 
        float ratio = (float)w / (float)h;
        int drawW = 1280; int drawH = (int)(1280 / ratio);
        if(drawH > 720) { drawH = 720; drawW = (int)(720 * ratio); }
        SDL_Rect rDest = { (1280 - drawW)/2, (720 - drawH)/2, drawW, drawH };
        SDL_RenderCopy(renderer, tex, NULL, &rDest);
        
        if(esIngles) DibujarBarraInferior(renderer, font, "Viewing Photo", "(B) Back"); else DibujarBarraInferior(renderer, font, "Viendo Foto", "(B) Volver");
        SDL_RenderPresent(renderer);
    }
    SDL_DestroyTexture(tex);
}

void ReproducirVideoAVI(SDL_Renderer* renderer, TTF_Font* font, const std::string& ruta, bool esIngles) {
    FILE* f = fopen(ruta.c_str(), "rb"); if (!f) return;
    long moviOffset = 0; uint8_t buffer[1024]; fread(buffer, 1, 1024, f);
    for(int i=0; i<1020; i++) { if(buffer[i] == 'm' && buffer[i+1] == 'o' && buffer[i+2] == 'v' && buffer[i+3] == 'i') { moviOffset = i + 4; break; } }
    if (moviOffset == 0) moviOffset = 500; fseek(f, 0, SEEK_END); long fileSize = ftell(f); long dataSize = fileSize - moviOffset; int totalFrames = dataSize / (VID_SIZE + 8); if (totalFrames <= 0) { fclose(f); return; }
    SDL_Texture* videoTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, VID_W, VID_H); uint32_t* pixelBuffer = (uint32_t*)malloc(VID_SIZE);
    int frameActual = 0; bool reproduciendo = true; bool pausado = false; int delayVideo = 10; 
    while (reproduciendo) {
        SDL_Event event; while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) reproduciendo = false; }
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
        if (delayVideo > 0) delayVideo--;
        if (delayVideo == 0) {
            if (vpad.trigger & VPAD_BUTTON_B) reproduciendo = false; if (vpad.trigger & VPAD_BUTTON_A) pausado = !pausado;
            if (vpad.trigger & VPAD_BUTTON_RIGHT) { frameActual += 30; if (frameActual >= totalFrames) frameActual = totalFrames - 1; }
            if (vpad.trigger & VPAD_BUTTON_LEFT) { frameActual -= 30; if (frameActual < 0) frameActual = 0; }
        }
        if (vpad.tpNormal.touched && delayVideo == 0) {
            int tx = MapCoord(vpad.tpNormal.x, G_ADC_MIN_X, G_ADC_MAX_X, G_APP_W, false);
            int ty = MapCoord(vpad.tpNormal.y, G_ADC_MIN_Y, G_ADC_MAX_Y, G_APP_H, true);
            if (ty > 640 && tx > 1100) reproduciendo = false; else if (ty > 600 && ty < 640) frameActual = ((float)tx / 1280.0f) * totalFrames; else { pausado = !pausado; delayVideo = 15; }
        }
        if (!pausado) { frameActual++; if (frameActual >= totalFrames) { frameActual = 0; pausado = true; } }
        long offsetFrame = moviOffset + (frameActual * (long)(VID_SIZE + 8)) + 8; fseek(f, offsetFrame, SEEK_SET); fread(pixelBuffer, 1, VID_SIZE, f);
        for(int i=0; i<VID_W*VID_H; i++) { uint32_t p = pixelBuffer[i]; uint32_t b = (p >> 24) & 0xFF; uint32_t g = (p >> 16) & 0xFF; uint32_t r = (p >> 8) & 0xFF; pixelBuffer[i] = (0xFF << 24) | (b << 16) | (g << 8) | r; }
        SDL_UpdateTexture(videoTex, NULL, pixelBuffer, VID_W * 4); SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
        SDL_Rect rVid = { (1280 - 960)/2, (720 - 720)/2, 960, 720 }; SDL_RenderCopyEx(renderer, videoTex, NULL, &rVid, 0, NULL, SDL_FLIP_VERTICAL);
        if(esIngles) DibujarBarraInferior(renderer, font, "(A) Pause/Play   (< >) Seek", "(B) Exit"); else DibujarBarraInferior(renderer, font, "(A) Pausa/Rep   (< >) Mover", "(B) Salir");
        SDL_RenderPresent(renderer); if(!pausado) SDL_Delay(33); 
    }
    free(pixelBuffer); SDL_DestroyTexture(videoTex); fclose(f);
}

void CargarListaMedia(SDL_Renderer* renderer) {
    for (auto& item : g_listaMedia) if (item.texturaMiniatura) SDL_DestroyTexture(item.texturaMiniatura);
    g_listaMedia.clear();
    const char* directorio = "fs:/vol/external01/WiiUCamera Files"; DIR* dir = opendir(directorio);
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string nombre = ent->d_name; if (nombre == "." || nombre == "..") continue;
            ArchivoMedia item; item.nombre = nombre; item.rutaCompleta = std::string(directorio) + "/" + nombre;
            item.texturaMiniatura = NULL; item.esVideo = false;
            if (EsExtension(nombre, ".avi")) { item.esVideo = true; g_listaMedia.push_back(item); } else if (EsExtension(nombre, ".bmp")) { g_listaMedia.push_back(item); }
        } closedir(dir);
    } std::sort(g_listaMedia.rbegin(), g_listaMedia.rend(), [](const ArchivoMedia& a, const ArchivoMedia& b) { return a.nombre < b.nombre; });
}

void CargarMiniatura(SDL_Renderer* renderer, ArchivoMedia* item) {
    if (item->texturaMiniatura != NULL) return; 
    if (!item->esVideo) { SDL_Surface* s = IMG_Load(item->rutaCompleta.c_str()); if (s) { SDL_Surface* sMini = SDL_CreateRGBSurface(0, 160, 120, 32, 0,0,0,0); SDL_BlitScaled(s, NULL, sMini, NULL); item->texturaMiniatura = SDL_CreateTextureFromSurface(renderer, sMini); SDL_FreeSurface(s); SDL_FreeSurface(sMini); } }
}

void LiberarGaleria() { for (auto& item : g_listaMedia) if (item.texturaMiniatura) SDL_DestroyTexture(item.texturaMiniatura); g_listaMedia.clear(); }

int EjecutarGaleria(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    CargarListaMedia(renderer); 
    bool enGaleria = true; int resultado = 0; 
    
    // --- GRID AJUSTADO (PARA NO TAPAR) ---
    int columnas = 3; 
    int anchoItem = 300; 
    int altoItem = 220; // Un poco mas corto
    int margenX = 150; 
    int margenY = 100; 
    int espacio = 30; 
    // -------------------------------------

    int delayInput = 30; g_itemSeleccionado = 0; g_scrollGaleria = 0; int touchStartY = -1;
    bool confirmandoBorrar = false; 

    while (enGaleria) {
        SDL_Event event; while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) { enGaleria = false; resultado = -1; } }
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (confirmandoBorrar) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); SDL_Rect rDim = {0,0,1280,720}; SDL_RenderFillRect(renderer, &rDim);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_Rect rBox = {340, 260, 600, 200}; SDL_RenderFillRect(renderer, &rBox);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &rBox);
            
            SDL_Color cW = {255,255,255,255};
            SDL_Surface* sT = TTF_RenderText_Blended(font, esIngles ? "Delete this file?" : "Eliminar archivo?", cW);
            SDL_Texture* tT = SDL_CreateTextureFromSurface(renderer, sT); SDL_Rect rTxt = {640 - sT->w/2, 300, sT->w, sT->h};
            SDL_RenderCopy(renderer, tT, NULL, &rTxt); SDL_FreeSurface(sT); SDL_DestroyTexture(tT);
            
            SDL_Surface* sOp = TTF_RenderText_Blended(font, esIngles ? "(A) Yes   (B) No" : "(A) Si   (B) No", cW);
            SDL_Texture* tOp = SDL_CreateTextureFromSurface(renderer, sOp); SDL_Rect rOp = {640 - sOp->w/2, 380, sOp->w, sOp->h};
            SDL_RenderCopy(renderer, tOp, NULL, &rOp); SDL_FreeSurface(sOp); SDL_DestroyTexture(tOp);

            SDL_RenderPresent(renderer);
            if (vpad.trigger & VPAD_BUTTON_A) {
                if (!g_listaMedia.empty()) {
                    remove(g_listaMedia[g_itemSeleccionado].rutaCompleta.c_str());
                    CargarListaMedia(renderer); 
                    if (g_itemSeleccionado >= g_listaMedia.size()) g_itemSeleccionado = 0; 
                }
                confirmandoBorrar = false; delayInput = 20;
            }
            if (vpad.trigger & VPAD_BUTTON_B) { confirmandoBorrar = false; delayInput = 20; }
            continue; 
        }

        if (vpad.tpNormal.touched) {
            int tx = MapCoord(vpad.tpNormal.x, G_ADC_MIN_X, G_ADC_MAX_X, G_APP_W, false);
            int ty = MapCoord(vpad.tpNormal.y, G_ADC_MIN_Y, G_ADC_MAX_Y, G_APP_H, true);
            if (touchStartY == -1) touchStartY = ty;
            if (ty > 640 && tx > 1100) { enGaleria = false; resultado = 1; }
            if (ty < 640 && delayInput == 0) {
                 for (int i=0; i < g_listaMedia.size(); i++) {
                    if (TocoRect(tx, ty, g_listaMedia[i].rectArea)) {
                         g_itemSeleccionado = i; ArchivoMedia& item = g_listaMedia[g_itemSeleccionado];
                         if (item.esVideo) ReproducirVideoAVI(renderer, font, item.rutaCompleta, esIngles); else VerFoto(renderer, font, item.rutaCompleta, esIngles);
                         SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255); SDL_RenderClear(renderer); delayInput = 20; touchStartY = -1; break;
                    }
                 }
            }
            if (touchStartY != -1) { int deltaY = ty - touchStartY; if (abs(deltaY) > 5) { g_scrollGaleria -= deltaY; touchStartY = ty; } }
        } else { touchStartY = -1; }
        
        if (delayInput > 0) delayInput--;
        else {
            if (vpad.trigger & VPAD_BUTTON_B) { enGaleria = false; resultado = 1; }
            if (vpad.trigger & VPAD_BUTTON_X && !g_listaMedia.empty()) { confirmandoBorrar = true; delayInput = 20; }
            if (vpad.trigger & VPAD_BUTTON_A && !g_listaMedia.empty()) {
                ArchivoMedia& item = g_listaMedia[g_itemSeleccionado];
                if (item.esVideo) ReproducirVideoAVI(renderer, font, item.rutaCompleta, esIngles); else VerFoto(renderer, font, item.rutaCompleta, esIngles);
                SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255); SDL_RenderClear(renderer); delayInput = 20; 
            }
            if (!g_listaMedia.empty()) {
                if (vpad.hold & VPAD_BUTTON_RIGHT) { if (g_itemSeleccionado < g_listaMedia.size() - 1) g_itemSeleccionado++; delayInput = 10; }
                if (vpad.hold & VPAD_BUTTON_LEFT) { if (g_itemSeleccionado > 0) g_itemSeleccionado--; delayInput = 10; }
                if (vpad.hold & VPAD_BUTTON_DOWN) { if (g_itemSeleccionado + columnas < g_listaMedia.size()) g_itemSeleccionado += columnas; delayInput = 10; }
                if (vpad.hold & VPAD_BUTTON_UP) { if (g_itemSeleccionado - columnas >= 0) g_itemSeleccionado -= columnas; delayInput = 10; }
            }
        }
        SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255); SDL_RenderClear(renderer);
        SDL_Color colTitulo = {255, 255, 0, 255}; SDL_Surface* sTit = TTF_RenderText_Blended(font, esIngles ? "Gallery" : "Galeria", colTitulo); SDL_Texture* tTit = SDL_CreateTextureFromSurface(renderer, sTit); SDL_Rect rTit = {50, 30, sTit->w, sTit->h}; SDL_RenderCopy(renderer, tTit, NULL, &rTit); SDL_FreeSurface(sTit); SDL_DestroyTexture(tTit);

        if (g_listaMedia.empty()) { SDL_Color cW = {150, 150, 150, 255}; SDL_Surface* s = TTF_RenderText_Blended(font, ":(", cW); SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); SDL_Rect r = {620, 300, s->w, s->h}; SDL_RenderCopy(renderer, t, NULL, &r); SDL_FreeSurface(s); SDL_DestroyTexture(t); } 
        else {
            int filaSeleccion = g_itemSeleccionado / columnas; int ySeleccion = margenY + (filaSeleccion * (altoItem + espacio));
            if (ySeleccion + altoItem - g_scrollGaleria > 630) g_scrollGaleria = (ySeleccion + altoItem) - 630;
            if (ySeleccion - g_scrollGaleria < margenY) g_scrollGaleria = ySeleccion - margenY;
            for (int i = 0; i < g_listaMedia.size(); i++) {
                int col = i % columnas; int fila = i / columnas; int x = margenX + (col * (anchoItem + espacio)); int y = margenY + (fila * (altoItem + espacio)) - g_scrollGaleria;
                g_listaMedia[i].rectArea = {x, y, anchoItem, altoItem};
                if (y + altoItem > 0 && y < 640) { // Solo dibujar si no choca con la barra
                    SDL_Rect rItem = {x, y, anchoItem, altoItem}; CargarMiniatura(renderer, &g_listaMedia[i]);
                    if (i == g_itemSeleccionado) SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255); else SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &rItem);
                    SDL_Rect rImg = {x + 5, y + 5, anchoItem - 10, altoItem - 40}; SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderFillRect(renderer, &rImg);
                    if (g_listaMedia[i].esVideo) { SDL_Color cV = {0, 255, 255, 255}; SDL_Surface* sV = TTF_RenderText_Blended(font, "[ VIDEO ]", cV); SDL_Texture* tV = SDL_CreateTextureFromSurface(renderer, sV); SDL_Rect rV = {x + (anchoItem-sV->w)/2, y + 60, sV->w, sV->h}; SDL_RenderCopy(renderer, tV, NULL, &rV); SDL_FreeSurface(sV); SDL_DestroyTexture(tV); } 
                    else if (g_listaMedia[i].texturaMiniatura) { SDL_RenderCopy(renderer, g_listaMedia[i].texturaMiniatura, NULL, &rImg); }
                    std::string nombreCorto = g_listaMedia[i].nombre.substr(0, 12); SDL_Color cT = {255,255,255,255}; if (i == g_itemSeleccionado) cT = {0,0,0,255}; SDL_Surface* sN = TTF_RenderText_Solid(font, nombreCorto.c_str(), cT); if (sN) { SDL_Texture* tN = SDL_CreateTextureFromSurface(renderer, sN); SDL_Rect rN = {x + 5, y + altoItem - 30, sN->w, sN->h}; SDL_RenderCopy(renderer, tN, NULL, &rN); SDL_FreeSurface(sN); SDL_DestroyTexture(tN); }
                }
            }
        }
        
        if (esIngles) { if (g_listaMedia.empty()) DibujarBarraInferior(renderer, font, "No files found", "(B) Back"); else DibujarBarraInferior(renderer, font, "(A) View   (X) Delete", "(B) Back"); } 
        else { if (g_listaMedia.empty()) DibujarBarraInferior(renderer, font, "No hay archivos", "(B) Volver"); else DibujarBarraInferior(renderer, font, "(A) Ver   (X) Eliminar", "(B) Volver"); }
        
        SDL_RenderPresent(renderer);
    }
    LiberarGaleria(); return resultado;
}
#endif