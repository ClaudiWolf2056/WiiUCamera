#ifndef PUZZLE_H
#define PUZZLE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <vpad/input.h>
#include <vector>
#include <string>
#include <algorithm>
#include <time.h>
#include <dirent.h>

struct PiezaPuzzle {
    int idCorrecto;
    int idActual;
    SDL_Rect rectDestino;
    SDL_Rect rectOrigen;
};

// Mini explorador con aspecto limpio
inline std::string ElegirFotoParaPuzzle(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    std::vector<std::string> listaFotos;
    DIR* dir = opendir("fs:/vol/external01/WiiUCamera Files");
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string n = ent->d_name;
            if (n.length() > 4) {
                std::string ext = n.substr(n.length()-4);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".jpg" || ext == ".png" || ext == ".bmp") {
                    listaFotos.push_back("fs:/vol/external01/WiiUCamera Files/" + n);
                }
            }
        }
        closedir(dir);
    }
    
    // Sort descending by name so newest are likely first
    std::sort(listaFotos.begin(), listaFotos.end(), std::greater<std::string>());

    if (listaFotos.empty()) return "";

    int seleccion = 0;
    int delay = 0;
    bool eligiendo = true;
    std::string elegida = "";

    while (eligiendo && WHBProcIsRunning()) {
        SDL_Event ev; while (SDL_PollEvent(&ev)) {}
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
        
        if (delay > 0) delay--;

        if (delay == 0) {
            if (vpad.trigger & VPAD_BUTTON_B) eligiendo = false;
            if (vpad.trigger & VPAD_BUTTON_A) { elegida = listaFotos[seleccion]; eligiendo = false; }
            if (vpad.trigger & VPAD_BUTTON_DOWN) { seleccion++; if (seleccion >= (int)listaFotos.size()) seleccion = 0; delay = 10; }
            if (vpad.trigger & VPAD_BUTTON_UP) { seleccion--; if (seleccion < 0) seleccion = (int)listaFotos.size() - 1; delay = 10; }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255);
        SDL_RenderClear(renderer);

        SDL_Color colW = {255,255,255,255};
        SDL_Surface* sTit = TTF_RenderText_Blended(font, esIngles ? "Select a Photo to build" : "Selecciona una Foto para armar", {255,255,0,255});
        if (sTit) {
            SDL_Texture* tTit = SDL_CreateTextureFromSurface(renderer, sTit);
            SDL_Rect rTit = {(1280 - sTit->w)/2, 60, sTit->w, sTit->h};
            SDL_RenderCopy(renderer, tTit, NULL, &rTit);
            SDL_FreeSurface(sTit); SDL_DestroyTexture(tTit);
        }

        int startY = 160;
        int maxMostrar = 8;
        
        for (int i = 0; i < maxMostrar; i++) {
            int idx = seleccion - (maxMostrar / 2) + i;
            if (idx >= 0 && idx < (int)listaFotos.size()) {
                std::string nombre = listaFotos[idx].substr(listaFotos[idx].find_last_of("/") + 1);
                SDL_Color c = (idx == seleccion) ? SDL_Color{0,255,0,255} : colW;
                
                SDL_Rect bgList = { (1280 - 600) / 2, startY + (i * 50), 600, 45 };
                if (idx == seleccion) {
                    SDL_SetRenderDrawColor(renderer, 50, 50, 150, 255);
                    SDL_RenderFillRect(renderer, &bgList);
                }

                SDL_Surface* sTxt = TTF_RenderText_Blended(font, nombre.c_str(), c);
                if (sTxt) {
                    SDL_Texture* tTxt = SDL_CreateTextureFromSurface(renderer, sTxt);
                    SDL_Rect rTxt = {(1280 - sTxt->w)/2, startY + (i * 50) + (45 - sTxt->h)/2, sTxt->w, sTxt->h};
                    SDL_RenderCopy(renderer, tTxt, NULL, &rTxt);
                    SDL_FreeSurface(sTxt); SDL_DestroyTexture(tTxt);
                }
            }
        }
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); 
        SDL_Rect bar = {0, 660, 1280, 60}; 
        SDL_RenderFillRect(renderer, &bar);
        
        SDL_Surface* sBot = TTF_RenderText_Blended(font, esIngles ? "(A) Confirm  (B) Back" : "(A) Confirmar  (B) Atrás", {255, 255, 255, 255});
        if (sBot) {
            SDL_Texture* tBot = SDL_CreateTextureFromSurface(renderer, sBot);
            SDL_Rect rBot = {(1280 - sBot->w)/2, 675, sBot->w, sBot->h};
            SDL_RenderCopy(renderer, tBot, NULL, &rBot);
            SDL_FreeSurface(sBot); SDL_DestroyTexture(tBot);
        }

        SDL_RenderPresent(renderer);
    }
    return elegida;
}

// Lógica principal del Rompecabezas
inline void EjecutarPuzzle(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    std::string rutaFoto = ElegirFotoParaPuzzle(renderer, font, esIngles);
    if (rutaFoto == "") return;

    SDL_Surface* sup = IMG_Load(rutaFoto.c_str());
    if (!sup) return;
    SDL_Texture* texFoto = SDL_CreateTextureFromSurface(renderer, sup);
    int imgW = sup->w; int imgH = sup->h;
    SDL_FreeSurface(sup);

    const int GRID = 3; 
    std::vector<PiezaPuzzle> piezas;
    
    int boardSize = 540; 
    int pieceScrnW = boardSize / GRID;
    int pieceScrnH = boardSize / GRID;
    int pieceImgW = imgW / GRID;
    int pieceImgH = imgH / GRID;
    
    // Posicionar tablero a la izquierda
    int startX = 140;
    int startY = (720 - boardSize) / 2;
    
    // Posicionar imagen referencial a la derecha
    int refSize = 360;
    int refX = 820;
    int refY = (720 - refSize) / 2;

    for (int y = 0; y < GRID; y++) {
        for (int x = 0; x < GRID; x++) {
            PiezaPuzzle p;
            p.idCorrecto = y * GRID + x;
            p.idActual = p.idCorrecto;
            p.rectOrigen = {x * pieceImgW, y * pieceImgH, pieceImgW, pieceImgH};
            piezas.push_back(p);
        }
    }

    srand((unsigned int)time(NULL));
    for (int i = 0; i < 20; i++) {
        int a = rand() % (GRID * GRID);
        int b = rand() % (GRID * GRID);
        std::swap(piezas[a].idActual, piezas[b].idActual);
    }

    bool jugando = true;
    int piezaSeleccionada = -1;
    int delay = 0;
    
    Uint32 tiempoInicio = SDL_GetTicks();
    bool completado = false;

    while (jugando && WHBProcIsRunning()) {
        SDL_Event ev; while (SDL_PollEvent(&ev)) {}
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
        
        if (delay > 0) delay--;

        if (!completado) {
            if (vpad.tpNormal.touched && delay == 0) {
                float pctX = (vpad.tpNormal.x - 100.0f) / (3950.0f - 100.0f);
                float pctY = 1.0f - ((vpad.tpNormal.y - 100.0f) / (3900.0f - 100.0f));
                int tx = (int)(pctX * 1280.0f);
                int ty = (int)(pctY * 720.0f);

                if (tx >= startX && tx <= startX + boardSize && ty >= startY && ty <= startY + boardSize) {
                    int col = (tx - startX) / pieceScrnW;
                    int row = (ty - startY) / pieceScrnH;
                    int idTocado = row * GRID + col;

                    int indexTocado = -1;
                    for (size_t i = 0; i < piezas.size(); i++) {
                        if (piezas[i].idActual == idTocado) { indexTocado = (int)i; break; }
                    }

                    if (piezaSeleccionada == -1) {
                        piezaSeleccionada = indexTocado;
                        delay = 15;
                    } else {
                        std::swap(piezas[piezaSeleccionada].idActual, piezas[indexTocado].idActual);
                        piezaSeleccionada = -1;
                        delay = 15;

                        completado = true;
                        for (size_t i = 0; i < piezas.size(); i++) {
                            if (piezas[i].idActual != piezas[i].idCorrecto) { completado = false; break; }
                        }
                    }
                }
            }
        }

        if (vpad.trigger & VPAD_BUTTON_B) jugando = false;

        SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
        SDL_RenderClear(renderer);
        
        // Dibujar imagen de referencia a la derecha con un marco suave
        SDL_Rect rRef = {refX, refY, refSize, refSize};
        SDL_Rect rRefBorder = {refX - 4, refY - 4, refSize + 8, refSize + 8};
        SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
        SDL_RenderFillRect(renderer, &rRefBorder);
        SDL_RenderCopy(renderer, texFoto, NULL, &rRef);

        // Gap / separación entre piezas para simular cuadrados sueltos
        int gap = 3; 

        for (size_t i = 0; i < piezas.size(); i++) {
            int curCol = piezas[i].idActual % GRID;
            int curRow = piezas[i].idActual / GRID;
            
            // Área delimitadora interactiva (invisible)
            piezas[i].rectDestino = {startX + curCol * pieceScrnW, startY + curRow * pieceScrnH, pieceScrnW, pieceScrnH};
            
            // Área visual acortada por el margen 
            SDL_Rect rectVisual = {
                startX + curCol * pieceScrnW + gap,
                startY + curRow * pieceScrnH + gap,
                pieceScrnW - gap * 2,
                pieceScrnH - gap * 2
            };
            
            SDL_RenderCopy(renderer, texFoto, &piezas[i].rectOrigen, &rectVisual);
            
            if ((int)i == piezaSeleccionada) {
                SDL_SetRenderDrawColor(renderer, 255, 230, 0, 255);
                SDL_RenderDrawRect(renderer, &rectVisual);
                SDL_Rect rOut = {rectVisual.x-1, rectVisual.y-1, rectVisual.w+2, rectVisual.h+2};
                SDL_RenderDrawRect(renderer, &rOut);
            }
        }

        Uint32 tiempoActual = completado ? tiempoInicio : SDL_GetTicks();
        int segundos = (int)(tiempoActual - tiempoInicio) / 1000;
        char bufTiempo[64];
        sprintf(bufTiempo, "Time: %02d:%02d", segundos / 60, segundos % 60);
        
        SDL_Surface* sTime = TTF_RenderText_Blended(font, bufTiempo, {255,255,255,255});
        if (sTime) {
            SDL_Texture* tTime = SDL_CreateTextureFromSurface(renderer, sTime);
            SDL_Rect rTime = {40, 40, sTime->w, sTime->h};
            SDL_RenderCopy(renderer, tTime, NULL, &rTime);
            SDL_FreeSurface(sTime); SDL_DestroyTexture(tTime);
        }

        if (completado) {
            SDL_Surface* sWin = TTF_RenderText_Blended(font, esIngles ? "PUZZLE CLEARED!" : "¡ROMPECABEZAS ARMADO!", {0,255,0,255});
            if (sWin) {
                SDL_Texture* tWin = SDL_CreateTextureFromSurface(renderer, sWin);
                SDL_Rect rWin = {(1280 - sWin->w)/2, 40, sWin->w, sWin->h};
                SDL_RenderCopy(renderer, tWin, NULL, &rWin);
                SDL_FreeSurface(sWin); SDL_DestroyTexture(tWin);
            }
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texFoto);
}

#endif