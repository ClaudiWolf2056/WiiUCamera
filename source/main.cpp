#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h> 
#include <vpad/input.h>
#include <coreinit/foreground.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <stdlib.h> 

#include "camara.h" 
#include "recorder.h" 
#include "camera_effects.h" 
#include "gallery.h" 

const char* APP_VERSION = "v1.1.6";

enum EstadoApp {
    ESTADO_MENU_PRINCIPAL,
    ESTADO_SUBMENU_MODOS,
    ESTADO_UPDATES,
    ESTADO_AGRADECIMIENTOS,
    ESTADO_CAMARA,
    ESTADO_GALERIA
};

Mix_Music* g_MusicaFondo = NULL;
Mix_Chunk* g_SfxMove = NULL;
Mix_Chunk* g_SfxSelect = NULL;

void IniciarAudio() {
    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return;
    g_MusicaFondo = Mix_LoadMUS("/vol/content/music.mp3"); 
    g_SfxMove = Mix_LoadWAV("/vol/content/move.wav");
    g_SfxSelect = Mix_LoadWAV("/vol/content/select.wav");

    if (g_MusicaFondo) {
        Mix_VolumeMusic(50); 
        Mix_PlayMusic(g_MusicaFondo, -1); 
    }
    Mix_Volume(-1, 64); 
}

void ReproducirSonidoMover() { if (g_SfxMove) Mix_PlayChannel(-1, g_SfxMove, 0); }
void ReproducirSonidoSelect() { if (g_SfxSelect) Mix_PlayChannel(-1, g_SfxSelect, 0); }

SDL_Texture* CargarTextura(SDL_Renderer* renderer, const std::string& ruta) {
    SDL_Surface* superficie = IMG_Load(ruta.c_str());
    if (!superficie) return nullptr;
    SDL_Texture* textura = SDL_CreateTextureFromSurface(renderer, superficie);
    SDL_FreeSurface(superficie);
    return textura;
}

void DibujarTextoCentrado(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surface = TTF_RenderText_Blended(font, texto, color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect rect = { (1280 - surface->w) / 2, y, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }
}

void DibujarTextoConScroll(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int x, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surface = TTF_RenderText_Blended(font, texto, color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect rect = { x, y, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }
}

void DibujarCursorTactil(SDL_Renderer* renderer, int x, int y) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); 
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 150); 
    SDL_Rect rect = { x - 15, y - 15, 30, 30 }; 
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); 
}

bool VerificarToqueBoton(int touchX, int touchY, int btnX, int btnY, int btnW, int btnH) {
    return (touchX >= btnX && touchX <= (btnX + btnW) &&
            touchY >= btnY && touchY <= (btnY + btnH));
}

void DibujarBotonCentrado(SDL_Renderer* renderer, SDL_Texture* texturaBoton, int centroX, int centroY, bool seleccionado, SDL_Rect* outRect) {
    if (!texturaBoton) return;
    int w, h;
    SDL_QueryTexture(texturaBoton, NULL, NULL, &w, &h);
    int x = centroX - (w / 2);
    int y = centroY - (h / 2);
    if (outRect) { outRect->x = x; outRect->y = y; outRect->w = w; outRect->h = h; }

    SDL_Rect rectBoton = { x, y, w, h };

    if (seleccionado) {
        SDL_SetRenderDrawColor(renderer, 255, 230, 0, 255);
        int margen = 6;
        SDL_Rect rectBorde = { x - margen, y - margen, w + (margen*2), h + (margen*2) };
        SDL_RenderFillRect(renderer, &rectBorde);
    }
    SDL_RenderCopy(renderer, texturaBoton, NULL, &rectBoton);
}

int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO); 
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);
    VPADInit(); 
    
    mkdir("fs:/vol/external01/WiiUCamera Files", 0777);

    IniciarAudio();

    SDL_Window *window = SDL_CreateWindow("WiiUCamera", 0, 0, 1280, 720, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    TTF_Font* fuenteGrande = TTF_OpenFont("/vol/content/font.ttf", 64);
    TTF_Font* fuentePequena = TTF_OpenFont("/vol/content/font.ttf", 40);
    TTF_Font* fuenteMini = TTF_OpenFont("/vol/content/font.ttf", 24); 

    SDL_Texture* texBackground = CargarTextura(renderer, "/vol/content/background.png");
    SDL_Texture* texIniciar_ES = CargarTextura(renderer, "/vol/content/button_iniciar.png"); 
    SDL_Texture* texUpdates_ES = CargarTextura(renderer, "/vol/content/button_actualizaciones.png");
    SDL_Texture* texAgrade_ES = CargarTextura(renderer, "/vol/content/button_agradecimientos.png");
    SDL_Texture* texIdioma_ES  = CargarTextura(renderer, "/vol/content/button_idioma_es.png");
    SDL_Texture* texGaleria_ES = CargarTextura(renderer, "/vol/content/button_gallery.png");

    SDL_Texture* texIniciar_EN = CargarTextura(renderer, "/vol/content/button_iniciar_en.png");
    SDL_Texture* texUpdates_EN = CargarTextura(renderer, "/vol/content/button_updates_en.png"); 
    SDL_Texture* texAgrade_EN = CargarTextura(renderer, "/vol/content/button_agradecimientos_en.png");
    SDL_Texture* texIdioma_EN  = CargarTextura(renderer, "/vol/content/button_language_en.png");
    SDL_Texture* texGaleria_EN = CargarTextura(renderer, "/vol/content/button_gallery_en.png");

    SDL_Color colorBlanco = {255, 255, 255, 255};
    SDL_Color colorAmarillo = {255, 255, 0, 255};

    bool appRunning = true;
    int estado = ESTADO_MENU_PRINCIPAL;
    int seleccion = 0;
    bool esIngles = true;
    const int VELOCIDAD_CURSOR = 12;
    int delayInput = 0;
    SDL_Rect btnRects[5]; 
    int touchX = -100, touchY = -100;
    bool dedoPresionado = false;
    
    float scrollY = 0;

    while (appRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                appRunning = false; // Romper bucle y salir
            }
            else if (event.type == SDL_MOUSEMOTION) { touchX = event.motion.x; touchY = event.motion.y; }
            else if (event.type == SDL_MOUSEBUTTONDOWN) { dedoPresionado = true; touchX = event.button.x; touchY = event.button.y; }
            else if (event.type == SDL_MOUSEBUTTONUP) { dedoPresionado = false; }
        }

        VPADStatus vpad; VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

        SDL_RenderClear(renderer);
        if (texBackground) SDL_RenderCopy(renderer, texBackground, NULL, NULL);
        else { SDL_SetRenderDrawColor(renderer, 0, 0, 100, 255); SDL_RenderClear(renderer); }

        if (delayInput > 0) delayInput--;

        float stickY = vpad.leftStick.y;
        bool moverAbajo = (vpad.hold & VPAD_BUTTON_DOWN) || (stickY < -0.5f);
        bool moverArriba = (vpad.hold & VPAD_BUTTON_UP) || (stickY > 0.5f);
        bool moverDer   = (vpad.hold & VPAD_BUTTON_RIGHT) || (vpad.leftStick.x > 0.5f);
        bool moverIzq   = (vpad.hold & VPAD_BUTTON_LEFT) || (vpad.leftStick.x < -0.5f);
        bool botonA = (vpad.trigger & VPAD_BUTTON_A);
        bool botonB = (vpad.trigger & VPAD_BUTTON_B);
        bool huboMovimiento = false;

        if (estado == ESTADO_MENU_PRINCIPAL) {
            DibujarTextoCentrado(renderer, fuenteGrande, "WiiUCamera", 120, colorBlanco);
            DibujarTextoCentrado(renderer, fuenteMini, APP_VERSION, 680, colorBlanco);

            SDL_Texture* bStart = esIngles ? texIniciar_EN : texIniciar_ES;
            SDL_Texture* bGal = esIngles ? texGaleria_EN : texGaleria_ES;
            SDL_Texture* bUpd = esIngles ? texUpdates_EN : texUpdates_ES;
            SDL_Texture* bCred = esIngles ? texAgrade_EN : texAgrade_ES;
            SDL_Texture* bLang = esIngles ? texIdioma_EN : texIdioma_ES;
            SDL_Texture* lista[] = { bStart, bGal, bUpd, bCred, bLang };
            
            int cX[] = { 640, 400, 880, 400, 880 };
            int cY[] = { 300, 420, 420, 540, 540 };

            for (int i = 0; i < 5; i++) {
                DibujarBotonCentrado(renderer, lista[i], cX[i], cY[i], (i == seleccion), &btnRects[i]);
                if (dedoPresionado && VerificarToqueBoton(touchX, touchY, btnRects[i].x, btnRects[i].y, btnRects[i].w, btnRects[i].h)) {
                    if (seleccion != i) { seleccion = i; ReproducirSonidoMover(); }
                    if (delayInput == 0) {
                        ReproducirSonidoSelect();
                        if (seleccion == 0) estado = ESTADO_SUBMENU_MODOS;
                        else if (seleccion == 1) estado = ESTADO_GALERIA;
                        else if (seleccion == 2) estado = ESTADO_UPDATES;
                        else if (seleccion == 3) { estado = ESTADO_AGRADECIMIENTOS; scrollY = 0; }
                        else if (seleccion == 4) esIngles = !esIngles;
                        dedoPresionado = false; delayInput = 20;
                    }
                }
            }

            if (delayInput == 0) {
                if (moverAbajo) {
                    if (seleccion == 0) seleccion = 1; else if (seleccion == 1) seleccion = 3; else if (seleccion == 2) seleccion = 4;
                    delayInput = VELOCIDAD_CURSOR; huboMovimiento = true;
                }
                if (moverArriba) {
                    if (seleccion == 3) seleccion = 1; else if (seleccion == 4) seleccion = 2; else if (seleccion == 1 || seleccion == 2) seleccion = 0;
                    delayInput = VELOCIDAD_CURSOR; huboMovimiento = true;
                }
                if (moverDer) { if (seleccion == 1) seleccion = 2; else if (seleccion == 3) seleccion = 4; delayInput = VELOCIDAD_CURSOR; huboMovimiento = true; }
                if (moverIzq) { if (seleccion == 2) seleccion = 1; else if (seleccion == 4) seleccion = 3; delayInput = VELOCIDAD_CURSOR; huboMovimiento = true; }

                if (huboMovimiento) ReproducirSonidoMover();
                if (botonA) {
                    ReproducirSonidoSelect();
                    if (seleccion == 0) estado = ESTADO_SUBMENU_MODOS;
                    else if (seleccion == 1) estado = ESTADO_GALERIA;
                    else if (seleccion == 2) estado = ESTADO_UPDATES;
                    else if (seleccion == 3) { estado = ESTADO_AGRADECIMIENTOS; scrollY = 0; }
                    else if (seleccion == 4) esIngles = !esIngles;
                    seleccion = 0; delayInput = 20;
                }
            }

        } else if (estado == ESTADO_SUBMENU_MODOS) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Camera Mode" : "Modo de Camara", 100, colorBlanco);
            const char* opEN[] = { "Take photo (normal)", "Record video (AVI)", "Effects (Filters)", "Soon..." };
            const char* opES[] = { "Tomar foto (normal)", "Grabar video (AVI)", "Efectos (Filtros)", "Pronto..." };
            int startY = 300;
            for (int i = 0; i < 4; i++) {
                SDL_Color col = (i == seleccion) ? colorAmarillo : colorBlanco;
                int textoY = startY + (i * 70);
                DibujarTextoCentrado(renderer, fuentePequena, esIngles ? opEN[i] : opES[i], textoY, col);
                if (dedoPresionado && VerificarToqueBoton(touchX, touchY, (1280-600)/2, textoY, 600, 50)) {
                     if (seleccion != i) { seleccion = i; ReproducirSonidoMover(); }
                     if (delayInput == 0) { ReproducirSonidoSelect(); estado = ESTADO_CAMARA; dedoPresionado = false; delayInput = 30; }
                }
            }
            if (delayInput == 0) {
                if (moverAbajo) { seleccion++; if (seleccion >= 4) seleccion = 0; delayInput = VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (moverArriba) { seleccion--; if (seleccion < 0) seleccion = 3; delayInput = VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (huboMovimiento) ReproducirSonidoMover();
                if (botonB) { estado = ESTADO_MENU_PRINCIPAL; seleccion = 0; delayInput = 30; }
                if (botonA) { ReproducirSonidoSelect(); estado = ESTADO_CAMARA; delayInput = 30; }
            }

        } else if (estado == ESTADO_UPDATES) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Changelog" : "Novedades", 110, colorAmarillo);
            int startY = 220; int gap = 45; SDL_Color colTxt = colorBlanco;
            
            if (esIngles) {
                DibujarTextoCentrado(renderer, fuentePequena, "v1.1.6 - Stable Beta", startY, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- FIX: Inverted video orientation in Gallery", startY + gap*1.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- FIX: App exit hang loop", startY + gap*2.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NEW: Added instructions to camera modes", startY + gap*3.5, colTxt);
            } else {
                DibujarTextoCentrado(renderer, fuentePequena, "v1.1.6 - Beta Estable", startY, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- FIX: Orientacion de video invertida en Galeria", startY + gap*1.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- FIX: Cuelgue al salir de la app", startY + gap*2.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NUEVO: Instrucciones en modos de camara", startY + gap*3.5, colTxt);
            }
            DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "(Press B to return)" : "(Presiona B para volver)", 600, colorAmarillo);
            if (botonB) { estado = ESTADO_MENU_PRINCIPAL; delayInput = 30; }

        } else if (estado == ESTADO_AGRADECIMIENTOS) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Credits" : "Creditos", 120, colorAmarillo);
            
            SDL_Rect clipRect = { 200, 200, 880, 380 }; 
            SDL_RenderSetClipRect(renderer, &clipRect);

            int contentY = 200 - scrollY;
            
            const char* linesEN[] = {
                "Created by: ClaudiWolf2056", "",
                "Special Thanks to:", "Wii U Community (Latam)",
                "- p-anthoX", "- JEAN_PRETENDO", "- Santix Aldama", "",
                "Facebook Users:", "Da****, Ro****", "Ce***, Ta**", "",
                "Technical Help:", "ForTheUsers (4TU)", "My SD Card", "",
                "Libraries:", "SDL2 for Wii U - devkitPro", "",
                "See you soon! - ClaudiWolf2056"
            };

            const char* linesES[] = {
                "Creado por: ClaudiWolf2056", "",
                "Agradecimientos a:", "Comunidad Wii U (Latam)",
                "- p-anthoX", "- JEAN_PRETENDO", "- Santix Aldama", "",
                "Interaccion de Facebook:", "Da****, Ro****", "Ce***, Ta**", "",
                "Ayuda Tecnica:", "ForTheUsers (4TU)", "Mi tarjeta SD xd", "",
                "Librerias:", "SDL2 (Wii U) - devkitPro", "",
                "Nos vemos pronto! - ClaudiWolf2056"
            };

            const int lineCount = 20; 
            const char** lineasAUsar = esIngles ? linesEN : linesES;

            for(int i=0; i<lineCount; i++) {
                DibujarTextoConScroll(renderer, fuentePequena, lineasAUsar[i], 300, contentY + (i*50), colorBlanco);
            }

            SDL_RenderSetClipRect(renderer, NULL); 

            if (moverAbajo) scrollY += 5;
            if (moverArriba && scrollY > 0) scrollY -= 5;
            
            int indicadorY = 200 + (scrollY / 3); 
            if(indicadorY > 550) indicadorY = 550;
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_Rect scrollBg = { 1100, 200, 10, 380 }; SDL_RenderFillRect(renderer, &scrollBg);
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_Rect scrollInd = { 1100, indicadorY, 10, 30 }; SDL_RenderFillRect(renderer, &scrollInd);

            DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "(Use D-Pad to Scroll - B to return)" : "(Usa Cruceta para bajar - B volver)", 620, colorAmarillo);
            if (botonB) { estado = ESTADO_MENU_PRINCIPAL; delayInput = 30; }

        } else if (estado == ESTADO_GALERIA) {
            Mix_PauseMusic();
            int res = EjecutarGaleria(renderer, fuenteMini, esIngles);
            if (res != -1) {
                Mix_ResumeMusic(); 
                estado = ESTADO_MENU_PRINCIPAL;
                seleccion = 1; 
            } else {
                appRunning = false; 
            }
            delayInput = 30;

        } else if (estado == ESTADO_CAMARA) {
            Mix_PauseMusic(); 
            dedoPresionado = false;
            
            int res = 0;
            if (seleccion == 0) { res = EjecutarCamara(renderer, fuenteMini, esIngles); } 
            else if (seleccion == 1) { res = EjecutarGrabadora(renderer, fuenteMini, esIngles); } 
            else if (seleccion == 2) { res = EjecutarCamaraEfectos(renderer, fuenteMini, esIngles); }
            else { res = EjecutarCamara(renderer, fuenteMini, esIngles); } 

            if (res != -1) {
                Mix_ResumeMusic(); 
                SDL_RenderClear(renderer); 
                SDL_RenderPresent(renderer); 
                if (res == 1) estado = ESTADO_SUBMENU_MODOS; 
            } else {
                appRunning = false; 
            }
            delayInput = 30;
        }

        if (dedoPresionado) DibujarCursorTactil(renderer, touchX, touchY);
        SDL_RenderPresent(renderer);
    }

    
    VPADShutdown();
    exit(0); 
    
    return 0;
}