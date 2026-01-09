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

#define VID_W 640
#define VID_H 480
#define VID_SIZE (VID_W * VID_H * 4)

struct ArchivoMedia {
    std::string nombre;
    std::string rutaCompleta;
    bool esVideo;
    SDL_Texture* texturaMiniatura; 
};

std::vector<ArchivoMedia> g_listaMedia;
int g_scrollGaleria = 0;
int g_itemSeleccionado = 0;

bool EsExtension(const std::string& nombre, const std::string& ext) {
    if (nombre.length() >= ext.length()) {
        return (0 == nombre.compare(nombre.length() - ext.length(), ext.length(), ext));
    }
    return false;
}

void VerFoto(SDL_Renderer* renderer, TTF_Font* font, const std::string& ruta, bool esIngles) {
    SDL_Surface* surf = IMG_Load(ruta.c_str());
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    bool viendo = true;
    while (viendo) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) viendo = false;
        }
        VPADStatus vpad; VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (vpad.trigger & VPAD_BUTTON_B) viendo = false;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        int w, h;
        SDL_QueryTexture(tex, NULL, NULL, &w, &h);
        float scale = 1.5f; 
        SDL_Rect rDest = { (int)(640 - (w*scale)/2), (int)(360 - (h*scale)/2), (int)(w*scale), (int)(h*scale) };
        SDL_RenderCopy(renderer, tex, NULL, &rDest);

        SDL_Color col = {255, 255, 255, 255};
        SDL_Surface* sTxt = TTF_RenderText_Blended(font, esIngles ? "Press B to return" : "Presiona B para regresar", col);
        if (sTxt) {
            SDL_Texture* tTxt = SDL_CreateTextureFromSurface(renderer, sTxt);
            SDL_Rect rTxt = { (1280 - sTxt->w)/2, 650, sTxt->w, sTxt->h };
            
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
            SDL_Rect rBack = {rTxt.x - 10, rTxt.y - 5, rTxt.w + 20, rTxt.h + 10};
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_RenderFillRect(renderer, &rBack);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            SDL_RenderCopy(renderer, tTxt, NULL, &rTxt);
            SDL_FreeSurface(sTxt); SDL_DestroyTexture(tTxt);
        }

        SDL_RenderPresent(renderer);
    }
    SDL_DestroyTexture(tex);
}

void ReproducirVideoAVI(SDL_Renderer* renderer, TTF_Font* font, const std::string& ruta, bool esIngles) {
    FILE* f = fopen(ruta.c_str(), "rb");
    if (!f) return;

    long moviOffset = 0;
    uint8_t buffer[1024];
    fread(buffer, 1, 1024, f);
    for(int i=0; i<1020; i++) {
        if(buffer[i] == 'm' && buffer[i+1] == 'o' && buffer[i+2] == 'v' && buffer[i+3] == 'i') {
            moviOffset = i + 4; 
            break;
        }
    }
    if (moviOffset == 0) moviOffset = 500; 

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    long dataSize = fileSize - moviOffset;
    int totalFrames = dataSize / (VID_SIZE + 8); 
    if (totalFrames <= 0) { fclose(f); return; }

    SDL_Texture* videoTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, VID_W, VID_H);
    uint32_t* pixelBuffer = (uint32_t*)malloc(VID_SIZE);
    
    int frameActual = 0;
    bool reproduciendo = true;
    bool pausado = false;

    while (reproduciendo) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) reproduciendo = false; }

        VPADStatus vpad; VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (vpad.trigger & VPAD_BUTTON_B) reproduciendo = false;
        if (vpad.trigger & VPAD_BUTTON_A) pausado = !pausado;

        if (vpad.trigger & VPAD_BUTTON_RIGHT) {
            frameActual += 30; 
            if (frameActual >= totalFrames) frameActual = totalFrames - 1;
        }
        if (vpad.trigger & VPAD_BUTTON_LEFT) {
            frameActual -= 30; 
            if (frameActual < 0) frameActual = 0;
        }

        if (!pausado) {
            frameActual++;
            if (frameActual >= totalFrames) {
                frameActual = 0; 
                pausado = true;
            }
        }

        long offsetFrame = moviOffset + (frameActual * (long)(VID_SIZE + 8)) + 8; 
        fseek(f, offsetFrame, SEEK_SET);
        fread(pixelBuffer, 1, VID_SIZE, f);

        for(int i=0; i<VID_W*VID_H; i++) {
            uint32_t p = pixelBuffer[i]; 
            uint32_t b = (p >> 24) & 0xFF;
            uint32_t g = (p >> 16) & 0xFF;
            uint32_t r = (p >> 8) & 0xFF;
            uint32_t a = p & 0xFF; 
            pixelBuffer[i] = (0xFF << 24) | (r << 16) | (g << 8) | b; 
        }

        SDL_UpdateTexture(videoTex, NULL, pixelBuffer, VID_W * 4);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_Rect rVid = { (1280 - 960)/2, (720 - 720)/2, 960, 720 }; 
        SDL_RenderCopy(renderer, videoTex, NULL, &rVid);

        int barraY = 600;
        int barraW = 800;
        int barraX = (1280 - barraW) / 2;
        
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 150);
        SDL_Rect rBarBg = {barraX, barraY, barraW, 10};
        SDL_RenderFillRect(renderer, &rBarBg);

        int progresoW = (frameActual * barraW) / totalFrames;
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 200);
        SDL_Rect rBarProg = {barraX, barraY, progresoW, 10};
        SDL_RenderFillRect(renderer, &rBarProg);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect rKnob = {barraX + progresoW - 8, barraY - 5, 16, 20};
        SDL_RenderFillRect(renderer, &rKnob);

        SDL_Color col = {255, 255, 255, 255};
        const char* txt = esIngles ? "(A) Pause  (< >) Seek  (B) Back" : "(A) Pausa  (< >) Mover  (B) Salir";
        SDL_Surface* sT = TTF_RenderText_Blended(font, txt, col);
        if (sT) {
            SDL_Texture* tT = SDL_CreateTextureFromSurface(renderer, sT);
            SDL_Rect rT = {(1280 - sT->w)/2, 630, sT->w, sT->h};
            
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0,0,0, 150);
            SDL_Rect rTBg = {rT.x-10, rT.y-5, rT.w+20, rT.h+10};
            SDL_RenderFillRect(renderer, &rTBg);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            SDL_RenderCopy(renderer, tT, NULL, &rT);
            SDL_FreeSurface(sT); SDL_DestroyTexture(tT);
        }

        SDL_RenderPresent(renderer);
        if(!pausado) SDL_Delay(33); 
    }

    free(pixelBuffer);
    SDL_DestroyTexture(videoTex);
    fclose(f);
}

void CargarListaMedia(SDL_Renderer* renderer) {
    for (auto& item : g_listaMedia) if (item.texturaMiniatura) SDL_DestroyTexture(item.texturaMiniatura);
    g_listaMedia.clear();

    const char* directorio = "fs:/vol/external01/WiiUCamera Files";
    DIR* dir = opendir(directorio);
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string nombre = ent->d_name;
            if (nombre == "." || nombre == "..") continue;
            ArchivoMedia item; item.nombre = nombre;
            item.rutaCompleta = std::string(directorio) + "/" + nombre;
            item.texturaMiniatura = NULL; item.esVideo = false;
            if (EsExtension(nombre, ".avi")) { item.esVideo = true; g_listaMedia.push_back(item); } 
            else if (EsExtension(nombre, ".bmp")) { g_listaMedia.push_back(item); }
        }
        closedir(dir);
    }
    std::sort(g_listaMedia.rbegin(), g_listaMedia.rend(), [](const ArchivoMedia& a, const ArchivoMedia& b) { return a.nombre < b.nombre; });
}

void CargarMiniatura(SDL_Renderer* renderer, ArchivoMedia* item) {
    if (item->texturaMiniatura != NULL) return; 
    if (!item->esVideo) {
        SDL_Surface* s = IMG_Load(item->rutaCompleta.c_str());
        if (s) {
            SDL_Surface* sMini = SDL_CreateRGBSurface(0, 160, 120, 32, 0,0,0,0);
            SDL_BlitScaled(s, NULL, sMini, NULL);
            item->texturaMiniatura = SDL_CreateTextureFromSurface(renderer, sMini);
            SDL_FreeSurface(s); SDL_FreeSurface(sMini);
        }
    }
}

void LiberarGaleria() {
    for (auto& item : g_listaMedia) if (item.texturaMiniatura) SDL_DestroyTexture(item.texturaMiniatura);
    g_listaMedia.clear();
}

void DibujarTextoGaleria(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int x, int y, SDL_Color col) {
    SDL_Surface* s = TTF_RenderText_Blended(font, texto, col);
    if(s) {
        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
        SDL_Rect r = {x, y, s->w, s->h};
        SDL_RenderCopy(renderer, t, NULL, &r);
        SDL_FreeSurface(s); SDL_DestroyTexture(t);
    }
}

int EjecutarGaleria(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    CargarListaMedia(renderer);
    
    bool enGaleria = true;
    int resultado = 0;
    int columnas = 3; int anchoItem = 250; int altoItem = 200;
    int margenX = 50; int margenY = 120; int espacio = 20; 
    int delayInput = 0; g_itemSeleccionado = 0; g_scrollGaleria = 0;

    while (enGaleria) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) { enGaleria = false; resultado = -1; } }

        VPADStatus vpad; VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (vpad.trigger & VPAD_BUTTON_B) { enGaleria = false; resultado = 1; }

        if (vpad.trigger & VPAD_BUTTON_A && !g_listaMedia.empty()) {
            ArchivoMedia& item = g_listaMedia[g_itemSeleccionado];
            if (item.esVideo) {
                ReproducirVideoAVI(renderer, font, item.rutaCompleta, esIngles);
            } else {
                VerFoto(renderer, font, item.rutaCompleta, esIngles);
            }
            SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255);
            SDL_RenderClear(renderer);
        }

        if (delayInput > 0) delayInput--;
        if (delayInput == 0 && !g_listaMedia.empty()) {
            if (vpad.hold & VPAD_BUTTON_RIGHT) { if (g_itemSeleccionado < g_listaMedia.size() - 1) g_itemSeleccionado++; delayInput = 10; }
            if (vpad.hold & VPAD_BUTTON_LEFT) { if (g_itemSeleccionado > 0) g_itemSeleccionado--; delayInput = 10; }
            if (vpad.hold & VPAD_BUTTON_DOWN) { if (g_itemSeleccionado + columnas < g_listaMedia.size()) g_itemSeleccionado += columnas; delayInput = 10; }
            if (vpad.hold & VPAD_BUTTON_UP) { if (g_itemSeleccionado - columnas >= 0) g_itemSeleccionado -= columnas; delayInput = 10; }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255);
        SDL_RenderClear(renderer);

        SDL_Color colTitulo = {255, 255, 0, 255};
        SDL_Surface* sTit = TTF_RenderText_Blended(font, esIngles ? "Gallery" : "Galeria", colTitulo);
        SDL_Texture* tTit = SDL_CreateTextureFromSurface(renderer, sTit);
        SDL_Rect rTit = {50, 30, sTit->w, sTit->h};
        SDL_RenderCopy(renderer, tTit, NULL, &rTit);
        SDL_FreeSurface(sTit); SDL_DestroyTexture(tTit);

        int panelX = 900; 
        SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
        SDL_Rect rPanel = {panelX, 100, 350, 500};
        SDL_RenderFillRect(renderer, &rPanel);
        
        SDL_Color cW = {255, 255, 255, 255};
        if (esIngles) {
            DibujarTextoGaleria(renderer, font, "Press A to View", panelX + 20, 150, cW);
            DibujarTextoGaleria(renderer, font, "Press B to Exit", panelX + 20, 200, cW);
        } else {
            DibujarTextoGaleria(renderer, font, "Presiona A para ver", panelX + 20, 150, cW);
            DibujarTextoGaleria(renderer, font, "Presiona B para salir", panelX + 20, 200, cW);
        }

        if (g_listaMedia.empty()) {
            DibujarTextoGaleria(renderer, font, "No files / Vacio", 400, 360, cW);
        } else {
            int filaSeleccion = g_itemSeleccionado / columnas;
            int ySeleccion = margenY + (filaSeleccion * (altoItem + espacio));
            if (ySeleccion + altoItem - g_scrollGaleria > 650) g_scrollGaleria = (ySeleccion + altoItem) - 650;
            if (ySeleccion - g_scrollGaleria < margenY) g_scrollGaleria = ySeleccion - margenY;

            for (int i = 0; i < g_listaMedia.size(); i++) {
                int col = i % columnas; int fila = i / columnas;
                int x = margenX + (col * (anchoItem + espacio));
                int y = margenY + (fila * (altoItem + espacio)) - g_scrollGaleria;

                if (y + altoItem > 0 && y < 720) {
                    SDL_Rect rItem = {x, y, anchoItem, altoItem};
                    CargarMiniatura(renderer, &g_listaMedia[i]);

                    if (i == g_itemSeleccionado) SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
                    else SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                    SDL_RenderFillRect(renderer, &rItem);

                    SDL_Rect rImg = {x + 5, y + 5, anchoItem - 10, altoItem - 40};
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderFillRect(renderer, &rImg);

                    if (g_listaMedia[i].esVideo) {
                        SDL_Color cV = {0, 255, 255, 255};
                        SDL_Surface* sV = TTF_RenderText_Blended(font, "[ VIDEO ]", cV);
                        SDL_Texture* tV = SDL_CreateTextureFromSurface(renderer, sV);
                        SDL_Rect rV = {x + (anchoItem-sV->w)/2, y + 60, sV->w, sV->h};
                        SDL_RenderCopy(renderer, tV, NULL, &rV);
                        SDL_FreeSurface(sV); SDL_DestroyTexture(tV);
                    } else if (g_listaMedia[i].texturaMiniatura) {
                        SDL_RenderCopy(renderer, g_listaMedia[i].texturaMiniatura, NULL, &rImg);
                    }

                    std::string nombreCorto = g_listaMedia[i].nombre.substr(0, 15);
                    SDL_Color cT = {255,255,255,255};
                    if (i == g_itemSeleccionado) cT = {0,0,0,255};
                    SDL_Surface* sN = TTF_RenderText_Solid(font, nombreCorto.c_str(), cT);
                    if (sN) {
                        SDL_Texture* tN = SDL_CreateTextureFromSurface(renderer, sN);
                        SDL_Rect rN = {x + 5, y + altoItem - 30, sN->w, sN->h};
                        SDL_RenderCopy(renderer, tN, NULL, &rN);
                        SDL_FreeSurface(sN); SDL_DestroyTexture(tN);
                    }
                }
            }
        }
        SDL_RenderPresent(renderer);
    }
    LiberarGaleria();
    return resultado;
}
#endif