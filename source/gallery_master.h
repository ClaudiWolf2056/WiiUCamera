#ifndef GALLERY_MASTER_H
#define GALLERY_MASTER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vpad/input.h>
#include <whb/proc.h> // Necesario para detectar salida HOME

// Retorna: 0=Fotos, 1=Screenshots, -1=Atras, -2=Salir App
int EjecutarGalleryMaster(SDL_Renderer* renderer, TTF_Font* fontGrande, TTF_Font* fontPequena, TTF_Font* fontMini, bool esIngles) {
    bool enMenu = true;
    int seleccion = 0;
    int delayInput = 10;
    int resultado = -1;

    const char* titulo = esIngles ? "Select Gallery" : "Seleccionar Galeria";
    const char* opEN[] = { "Photos & Videos (Camera)", "Screenshots (Wii U)" };
    const char* opES[] = { "Fotos y Videos (Camara)", "Screenshots (Wii U)" };

    SDL_Color colW = {255, 255, 255, 255};
    SDL_Color colY = {255, 255, 0, 255};

    // FIX: Agregado WHBProcIsRunning para evitar loop infinito
    while (enMenu && WHBProcIsRunning()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) return -2; // Codigo salida total
        }

        VPADStatus vpad; VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (delayInput > 0) delayInput--;

        if (delayInput == 0) {
            if (vpad.hold & VPAD_BUTTON_DOWN) { seleccion++; if (seleccion > 1) seleccion = 0; delayInput = 10; }
            if (vpad.hold & VPAD_BUTTON_UP) { seleccion--; if (seleccion < 0) seleccion = 1; delayInput = 10; }
            
            if (vpad.trigger & VPAD_BUTTON_B) { enMenu = false; resultado = -1; }
            if (vpad.trigger & VPAD_BUTTON_A) { enMenu = false; resultado = seleccion; }
            
            // Soporte Tactil Simple
            if (vpad.tpNormal.touched) {
                // Mapeo simple aproximado
                int ty = (int)((1.0f - (vpad.tpNormal.y - 100.0f) / 3800.0f) * 720.0f);
                if (ty > 600) { // Zona inferior (Atras)
                     enMenu = false; resultado = -1;
                } else if (ty > 350 && ty < 430) { // Opcion 1
                     seleccion = 0; if(vpad.trigger & VPAD_BUTTON_A) { enMenu=false; resultado=0; }
                } else if (ty > 430 && ty < 510) { // Opcion 2
                     seleccion = 1; if(vpad.trigger & VPAD_BUTTON_A) { enMenu=false; resultado=1; }
                }
            }
        }

        // DIBUJADO
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        // Titulo
        SDL_Surface* sTit = TTF_RenderText_Blended(fontGrande, titulo, colW);
        if (sTit) {
            SDL_Texture* tTit = SDL_CreateTextureFromSurface(renderer, sTit);
            SDL_Rect rTit = { (1280 - sTit->w) / 2, 150, sTit->w, sTit->h };
            SDL_RenderCopy(renderer, tTit, NULL, &rTit);
            SDL_FreeSurface(sTit); SDL_DestroyTexture(tTit);
        }

        // Opciones
        for (int i = 0; i < 2; i++) {
            SDL_Color c = (i == seleccion) ? colY : colW;
            const char* texto = esIngles ? opEN[i] : opES[i];
            
            SDL_Surface* sOp = TTF_RenderText_Blended(fontPequena, texto, c);
            if (sOp) {
                SDL_Texture* tOp = SDL_CreateTextureFromSurface(renderer, sOp);
                SDL_Rect rOp = { (1280 - sOp->w) / 2, 350 + (i * 80), sOp->w, sOp->h };
                SDL_RenderCopy(renderer, tOp, NULL, &rOp);
                SDL_FreeSurface(sOp); SDL_DestroyTexture(tOp);
            }
        }

        // Barra inferior
        const char* txtAtras = esIngles ? "(B) Back" : "(B) Atras";
        const char* txtSel = esIngles ? "(A) Select" : "(A) Seleccionar";
        
        // Dibujado manual para no depender de funciones de main
        SDL_Surface* sB = TTF_RenderText_Blended(fontMini, txtAtras, colW);
        if (sB) { SDL_Texture* tB = SDL_CreateTextureFromSurface(renderer, sB); SDL_Rect r = { 30, 670, sB->w, sB->h }; SDL_RenderCopy(renderer, tB, NULL, &r); SDL_FreeSurface(sB); SDL_DestroyTexture(tB); }
        
        SDL_Surface* sA = TTF_RenderText_Blended(fontMini, txtSel, colW);
        if (sA) { SDL_Texture* tA = SDL_CreateTextureFromSurface(renderer, sA); SDL_Rect r = { (1280 - sA->w) / 2, 670, sA->w, sA->h }; SDL_RenderCopy(renderer, tA, NULL, &r); SDL_FreeSurface(sA); SDL_DestroyTexture(tA); }

        SDL_RenderPresent(renderer);
    }

    if (!WHBProcIsRunning()) return -2; // Salida forzada
    return resultado;
}

#endif