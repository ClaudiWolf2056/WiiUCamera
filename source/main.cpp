#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h> 
#include <vpad/input.h>
#include <coreinit/foreground.h>
#include <whb/proc.h>       
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/gfx.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <stdlib.h> 

#include "camara.h" 
#include "recorder.h" 
#include "camera_effects.h" 
#include "gallery.h"
#include "editor.h"

const char* APP_VERSION = "v1.2.0"; 

enum EstadoApp {
    ESTADO_MENU_PRINCIPAL,
    ESTADO_SUBMENU_MODOS,
    ESTADO_UPDATES,
    ESTADO_AGRADECIMIENTOS,
    ESTADO_CAMARA,
    ESTADO_GALERIA,
    ESTADO_EDITOR
};

// Calibración Hardware
const float MAIN_ADC_MIN_X = 100.0f; const float MAIN_ADC_MAX_X = 3950.0f;
const float MAIN_ADC_MIN_Y = 100.0f; const float MAIN_ADC_MAX_Y = 3900.0f;
const float MAIN_APP_W = 1280.0f; const float MAIN_APP_H = 720.0f;

int MapearMain(float val, float min, float max, float outMax, bool inv) {
    float pct = (val - min) / (max - min);
    if (pct < 0) pct = 0; if (pct > 1) pct = 1;
    if (inv) pct = 1.0f - pct;
    return (int)(pct * outMax);
}

Mix_Music* g_MusicaFondo = NULL;
Mix_Chunk* g_SfxMove = NULL;
Mix_Chunk* g_SfxSelect = NULL;

void IniciarAudio() {
    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return;
    g_MusicaFondo = Mix_LoadMUS("/vol/content/music.mp3"); 
    g_SfxMove = Mix_LoadWAV("/vol/content/move.wav");
    g_SfxSelect = Mix_LoadWAV("/vol/content/select.wav");
    if (g_MusicaFondo) { Mix_VolumeMusic(50); Mix_PlayMusic(g_MusicaFondo, -1); }
    Mix_Volume(-1, 64); 
}

void CerrarAudio() {
    Mix_HaltMusic(); Mix_HaltChannel(-1);
    if (g_SfxMove) Mix_FreeChunk(g_SfxMove); if (g_SfxSelect) Mix_FreeChunk(g_SfxSelect);
    if (g_MusicaFondo) Mix_FreeMusic(g_MusicaFondo); Mix_CloseAudio(); Mix_Quit();
}

void ReproducirSonidoMover() { if (g_SfxMove) Mix_PlayChannel(-1, g_SfxMove, 0); }
void ReproducirSonidoSelect() { if (g_SfxSelect) Mix_PlayChannel(-1, g_SfxSelect, 0); }

SDL_Texture* CargarTextura(SDL_Renderer* renderer, const std::string& ruta) {
    SDL_Surface* superficie = IMG_Load(ruta.c_str()); if (!superficie) return nullptr;
    SDL_Texture* textura = SDL_CreateTextureFromSurface(renderer, superficie); SDL_FreeSurface(superficie);
    return textura;
}

void DibujarTextoCentrado(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surface = TTF_RenderText_Blended(font, texto, color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect rect = { (1280 - surface->w) / 2, y, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        SDL_FreeSurface(surface); SDL_DestroyTexture(texture);
    }
}

// BARRA INFERIOR GLOBAL (Ahora soporta texto central)
void DibujarBarraInferiorGlobal(SDL_Renderer* renderer, TTF_Font* font, const char* textoIzq, const char* textoDer, const char* textoCentro) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); 
    SDL_Rect rBarra = {0, 660, 1280, 60}; SDL_RenderFillRect(renderer, &rBarra); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_Color colorTexto = {255, 255, 255, 255}; 
    
    // Izquierda
    if (textoIzq && strlen(textoIzq) > 0) {
        SDL_Surface* s = TTF_RenderText_Blended(font, textoIzq, colorTexto);
        if(s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); SDL_Rect r = {30, 675, s->w, s->h}; SDL_RenderCopy(renderer, t, NULL, &r); SDL_FreeSurface(s); SDL_DestroyTexture(t); }
    }
    // Derecha
    if (textoDer && strlen(textoDer) > 0) {
        SDL_Surface* s = TTF_RenderText_Blended(font, textoDer, colorTexto);
        if(s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); SDL_Rect r = {1250 - s->w, 675, s->w, s->h}; SDL_RenderCopy(renderer, t, NULL, &r); SDL_FreeSurface(s); SDL_DestroyTexture(t); }
    }
    // Centro (Nuevo)
    if (textoCentro && strlen(textoCentro) > 0) {
        SDL_Surface* s = TTF_RenderText_Blended(font, textoCentro, colorTexto);
        if(s) { 
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); 
            SDL_Rect r = {(1280 - s->w)/2, 675, s->w, s->h}; // Centrado exacto
            SDL_RenderCopy(renderer, t, NULL, &r); 
            SDL_FreeSurface(s); SDL_DestroyTexture(t); 
        }
    }
}

void DibujarCursorTactil(SDL_Renderer* renderer, int x, int y) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 255, 255, 255, 150); 
    SDL_Rect rect = { x - 10, y - 10, 20, 20 }; SDL_RenderFillRect(renderer, &rect); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); 
}

bool VerificarToqueBoton(int touchX, int touchY, int btnX, int btnY, int btnW, int btnH) {
    int margen = 15; return (touchX >= (btnX - margen) && touchX <= (btnX + btnW + margen) && touchY >= (btnY - margen) && touchY <= (btnY + btnH + margen));
}

void DibujarBotonCentrado(SDL_Renderer* renderer, SDL_Texture* texturaBoton, int centroX, int centroY, bool seleccionado, SDL_Rect* outRect) {
    int w = 200, h = 100;
    if (texturaBoton) SDL_QueryTexture(texturaBoton, NULL, NULL, &w, &h);
    int x = centroX - (w / 2); int y = centroY - (h / 2);
    if (outRect) { outRect->x = x; outRect->y = y; outRect->w = w; outRect->h = h; }
    if (!texturaBoton) return;
    SDL_Rect rectBoton = { x, y, w, h };
    if (seleccionado) {
        SDL_SetRenderDrawColor(renderer, 255, 230, 0, 255);
        int margen = 6; SDL_Rect rectBorde = { x - margen, y - margen, w + (margen*2), h + (margen*2) }; SDL_RenderFillRect(renderer, &rectBorde);
    }
    SDL_RenderCopy(renderer, texturaBoton, NULL, &rectBoton);
}

int main(int argc, char **argv) {
    WHBProcInit(); WHBLogConsoleInit(); 
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO); TTF_Init(); IMG_Init(IMG_INIT_PNG); VPADInit(); 
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
    SDL_Texture* texEditor_ES  = CargarTextura(renderer, "/vol/content/button_editor.png");

    SDL_Texture* texIniciar_EN = CargarTextura(renderer, "/vol/content/button_iniciar_en.png");
    SDL_Texture* texUpdates_EN = CargarTextura(renderer, "/vol/content/button_updates_en.png"); 
    SDL_Texture* texAgrade_EN = CargarTextura(renderer, "/vol/content/button_agradecimientos_en.png");
    SDL_Texture* texIdioma_EN  = CargarTextura(renderer, "/vol/content/button_language_en.png");
    SDL_Texture* texGaleria_EN = CargarTextura(renderer, "/vol/content/button_gallery_en.png");
    SDL_Texture* texEditor_EN  = CargarTextura(renderer, "/vol/content/button_editor_en.png");

    SDL_Color colorBlanco = {255, 255, 255, 255};
    SDL_Color colorAmarillo = {255, 255, 0, 255};

    bool appRunning = true;
    int estado = ESTADO_MENU_PRINCIPAL;
    int seleccion = 0;
    bool esIngles = true;
    const int VELOCIDAD_CURSOR = 12;
    int delayInput = 0;
    SDL_Rect btnRects[6];
    int touchX = -100, touchY = -100;
    bool dedoPresionado = false;
    float scrollY = 0;
    int touchStartY = -1;

    while (appRunning && WHBProcIsRunning()) {
        SDL_Event event; while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) appRunning = false; }
        VPADStatus vpad; VPADReadError error; VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

        if (vpad.tpNormal.touched) {
            dedoPresionado = true;
            touchX = MapearMain(vpad.tpNormal.x, MAIN_ADC_MIN_X, MAIN_ADC_MAX_X, MAIN_APP_W, false);
            touchY = MapearMain(vpad.tpNormal.y, MAIN_ADC_MIN_Y, MAIN_ADC_MAX_Y, MAIN_APP_H, true); 
            if (touchStartY == -1) touchStartY = touchY;

            if (estado == ESTADO_AGRADECIMIENTOS && touchStartY != -1) {
                int delta = touchY - touchStartY; if (abs(delta) > 5) { scrollY -= delta; if (scrollY < 0) scrollY = 0; touchStartY = touchY; }
            }
            if (estado != ESTADO_MENU_PRINCIPAL && touchY > 600 && touchX > 1100 && delayInput == 0) {
                 if (estado == ESTADO_SUBMENU_MODOS || estado == ESTADO_UPDATES || estado == ESTADO_AGRADECIMIENTOS) {
                    estado = ESTADO_MENU_PRINCIPAL; delayInput = 30; ReproducirSonidoSelect(); touchStartY = -1;
                 }
            }
            if (estado == ESTADO_MENU_PRINCIPAL && delayInput == 0) {
                for(int i=0; i<6; i++) {
                    if (VerificarToqueBoton(touchX, touchY, btnRects[i].x, btnRects[i].y, btnRects[i].w, btnRects[i].h)) {
                         seleccion = i; ReproducirSonidoSelect();
                         if (i==0) estado=ESTADO_SUBMENU_MODOS;
                         else if (i==1) estado=ESTADO_EDITOR;
                         else if (i==2) estado=ESTADO_GALERIA;
                         else if (i==3) estado=ESTADO_UPDATES;
                         else if (i==4) { estado=ESTADO_AGRADECIMIENTOS; scrollY=0; }
                         else if (i==5) esIngles = !esIngles;
                         delayInput = 30; touchStartY = -1; 
                    }
                }
            }
            if (estado == ESTADO_SUBMENU_MODOS && delayInput == 0) {
                 int startY = 300;
                 for(int i=0; i<4; i++) {
                     if (VerificarToqueBoton(touchX, touchY, (1280-600)/2, startY + (i*70), 600, 50)) {
                         seleccion = i; ReproducirSonidoSelect(); estado = ESTADO_CAMARA; delayInput = 30; touchStartY = -1;
                     }
                 }
            }
        } else { dedoPresionado = false; touchStartY = -1; }

        SDL_RenderClear(renderer);
        if (texBackground) SDL_RenderCopy(renderer, texBackground, NULL, NULL);
        else { SDL_SetRenderDrawColor(renderer, 0, 0, 50, 255); SDL_RenderClear(renderer); }

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
            DibujarTextoCentrado(renderer, fuenteGrande, "WiiUCamera", 100, colorBlanco);
            SDL_Surface* sVer = TTF_RenderText_Blended(fuenteMini, APP_VERSION, colorBlanco);
            if(sVer){ SDL_Texture* tV = SDL_CreateTextureFromSurface(renderer, sVer); SDL_Rect rV={1250-sVer->w, 30, sVer->w, sVer->h}; SDL_RenderCopy(renderer,tV,NULL,&rV); SDL_FreeSurface(sVer); SDL_DestroyTexture(tV); }

            // Texto Tactil Activo
            DibujarTextoCentrado(renderer, fuenteMini, esIngles ? "Touch is working!" : "Ya funciona el tactil!", 180, colorAmarillo);

            SDL_Texture* bStart = esIngles ? texIniciar_EN : texIniciar_ES;
            SDL_Texture* bEdit  = esIngles ? texEditor_EN : texEditor_ES;
            SDL_Texture* bGal   = esIngles ? texGaleria_EN : texGaleria_ES;
            SDL_Texture* bUpd   = esIngles ? texUpdates_EN : texUpdates_ES;
            SDL_Texture* bCred  = esIngles ? texAgrade_EN : texAgrade_ES;
            SDL_Texture* bLang  = esIngles ? texIdioma_EN : texIdioma_ES;
            SDL_Texture* lista[] = { bStart, bEdit, bGal, bUpd, bCred, bLang };
            
            // GRID 2x3
            int cX[] = { 320, 640, 960,  320, 640, 960 };
            int cY[] = { 300, 300, 300,  450, 450, 450 };

            for (int i = 0; i < 6; i++) {
                DibujarBotonCentrado(renderer, lista[i], cX[i], cY[i], (i == seleccion), &btnRects[i]);
            }
            
            // Texto Info SD
            const char* txtSD = esIngles ? "Where are photos saved?: SD Card > WiiUCamera Files! :)" 
                                         : "Donde se guardan las fotos?: En tu SD en la carpeta de WiiUCamera Files! :)";
            DibujarTextoCentrado(renderer, fuenteMini, txtSD, 595, colorBlanco);

            // Texto Reporte Bugs (Mas arriba)
            const char* txtBug = esIngles ? "Found a bug? Report it at: claudiwolf2056@gmail.com" 
                                          : "Viste algun error? Reportalo en: claudiwolf2056@gmail.com";
            DibujarTextoCentrado(renderer, fuenteMini, txtBug, 625, colorBlanco);

            // Barra Inferior (Con Centro)
            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(D-Pad) Navigate", "(A) Select"); 
            else DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(Cruceta) Navegar", "(A) Seleccionar");

            if (delayInput == 0) {
                if (moverDer) { seleccion++; if(seleccion > 5) seleccion = 0; delayInput=VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (moverIzq) { seleccion--; if(seleccion < 0) seleccion = 5; delayInput=VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (moverAbajo && seleccion < 3) { seleccion += 3; delayInput=VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (moverArriba && seleccion >= 3) { seleccion -= 3; delayInput=VELOCIDAD_CURSOR; huboMovimiento=true; }
                
                if (huboMovimiento) ReproducirSonidoMover();
                if (botonA) {
                    ReproducirSonidoSelect();
                    if (seleccion == 0) estado = ESTADO_SUBMENU_MODOS;
                    else if (seleccion == 1) estado = ESTADO_EDITOR;
                    else if (seleccion == 2) estado = ESTADO_GALERIA;
                    else if (seleccion == 3) estado = ESTADO_UPDATES;
                    else if (seleccion == 4) { estado = ESTADO_AGRADECIMIENTOS; scrollY = 0; }
                    else if (seleccion == 5) esIngles = !esIngles;
                    delayInput = 20;
                }
            }

        } else if (estado == ESTADO_SUBMENU_MODOS) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Camera Mode" : "Modo de Camara", 100, colorBlanco);
            const char* opEN[] = { "Take photo (normal)", "Record video (AVI)", "Effects (Filters)", "Soon..." };
            const char* opES[] = { "Tomar foto (normal)", "Grabar video (AVI)", "Efectos (Filtros)", "Pronto..." };
            int startY = 300;
            for (int i = 0; i < 4; i++) {
                SDL_Color col = (i == seleccion) ? colorAmarillo : colorBlanco;
                DibujarTextoCentrado(renderer, fuentePequena, esIngles ? opEN[i] : opES[i], startY + (i*70), col);
            }
            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "(A) Confirm", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "(A) Confirmar", "(B) Atras", "");
            if (delayInput == 0) {
                if (moverAbajo) { seleccion++; if (seleccion>=4) seleccion=0; delayInput=VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (moverArriba) { seleccion--; if (seleccion<0) seleccion=3; delayInput=VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (huboMovimiento) ReproducirSonidoMover();
                if (botonB) { estado = ESTADO_MENU_PRINCIPAL; seleccion = 0; delayInput = 30; }
                if (botonA) { ReproducirSonidoSelect(); estado = ESTADO_CAMARA; delayInput = 30; }
            }

        } else if (estado == ESTADO_UPDATES) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Changelog" : "Novedades", 110, colorAmarillo);
            int startY = 220; int gap = 45; SDL_Color colTxt = colorBlanco;
            
            if(esIngles){ 
                DibujarTextoCentrado(renderer, fuentePequena, "v1.2.0 - Photo Editor", startY, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NEW: Effects Panel (Brightness, Contrast)", startY + gap*1.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- FIX: Corruption-Free Save ", startY + gap*2.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- FIX: Gallery Layout", startY + gap*3.5, colTxt);
            } else { 
                DibujarTextoCentrado(renderer, fuentePequena, "v1.2.0 - Editor de Fotos", startY, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NUEVO: Panel de Efectos (Brillo, Contraste)", startY + gap*1.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- FIX: Guardado sin corrupcion", startY + gap*2.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- FIX: Diseño de Galeria", startY + gap*3.5, colTxt);
            }
            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Atras", "");
            if (botonB && delayInput == 0) { estado = ESTADO_MENU_PRINCIPAL; delayInput = 30; }

        } else if (estado == ESTADO_AGRADECIMIENTOS) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Credits" : "Creditos", 120, colorAmarillo);
            SDL_Rect clipRect = { 200, 200, 880, 440 }; SDL_RenderSetClipRect(renderer, &clipRect);
            int contentY = 200 - scrollY;
            
            const char* linesEN[] = {
                "Created by: ClaudiWolf2056", "",
                "Special Thanks to:", "whateveritwas",
                "For providing part of his code", "for the exit logic of this app",
                "You can also find him on Github", "",
                "Wii U Community (Latam)",
                "- p-anthoX", "- JEAN_PRETENDO", "- Santix Aldama", "- Downyjarl", "- Keines", "",
                "Facebook Community:", "Da****, Ro****", "Ce***, and more <3", "",
                "Technical Help:", "ForTheUsers (4TU)", "My SD Card", "",
                "Libraries:", "SDL2 for Wii U - devkitPro", "",
                "See you soon! - ClaudiWolf2056"
            };

            const char* linesES[] = {
                "Creado por: ClaudiWolf2056", "",
                "Agradecimientos especiales a:", "whateveritwas",
                "Por brindar una parte de su codigo", "para la logica de cierre de esta app",
                "Tambien puedes encontrarlo en Github", "",
                "Comunidad Wii U (Latam)",
                "- p-anthoX", "- JEAN_PRETENDO", "- Santix Aldama", "- Downyjarl", "- Keines", "",
                "Comunidad de Facebook:", "Da****, Ro****", "Ce***, y muchos mas <3", "",
                "Ayuda Tecnica:", "ForTheUsers (4TU)", "Mi tarjeta SD xd", "",
                "Librerias:", "SDL2 (Wii U) - devkitPro", "",
                "Nos vemos pronto! - ClaudiWolf2056"
            };

            const int lineCount = 26; 
            const char** lineasAUsar = esIngles ? linesEN : linesES;
            
            for(int i=0; i<lineCount; i++) {
                SDL_Surface* s = TTF_RenderText_Blended(fuentePequena, lineasAUsar[i], colorBlanco);
                if(s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); SDL_Rect r = {(1280-s->w)/2, contentY+(i*50), s->w, s->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s); SDL_DestroyTexture(t); }
            }
            SDL_RenderSetClipRect(renderer, NULL); 
            if (moverAbajo) scrollY += 5; if (moverArriba && scrollY > 0) scrollY -= 5;
            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "(D-Pad/Touch) Scroll", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "(Cruceta/Touch) Desplazar", "(B) Atras", "");
            if (botonB && delayInput == 0) { estado = ESTADO_MENU_PRINCIPAL; delayInput = 30; }

        } else if (estado == ESTADO_GALERIA) {
            Mix_PauseMusic(); int res = EjecutarGaleria(renderer, fuenteMini, esIngles);
            if (res != -1) { Mix_ResumeMusic(); estado = ESTADO_MENU_PRINCIPAL; seleccion = 2; } else appRunning = false; 
            delayInput = 30;

        } else if (estado == ESTADO_EDITOR) {
            Mix_PauseMusic(); EjecutarEditor(renderer, fuenteMini, esIngles); Mix_ResumeMusic(); 
            estado = ESTADO_MENU_PRINCIPAL; seleccion = 1; delayInput = 30;

        } else if (estado == ESTADO_CAMARA) {
            Mix_PauseMusic(); dedoPresionado = false;
            int res = 0;
            if (seleccion == 0) res = EjecutarCamara(renderer, fuenteMini, esIngles);
            else if (seleccion == 1) res = EjecutarGrabadora(renderer, fuenteMini, esIngles);
            else if (seleccion == 2) res = EjecutarCamaraEfectos(renderer, fuenteMini, esIngles);
            else res = EjecutarCamara(renderer, fuenteMini, esIngles);
            if (res != -1) { Mix_ResumeMusic(); SDL_RenderClear(renderer); SDL_RenderPresent(renderer); if (res == 1) estado = ESTADO_SUBMENU_MODOS; } else appRunning = false; 
            delayInput = 30;
        }

        if (dedoPresionado) DibujarCursorTactil(renderer, touchX, touchY);
        SDL_RenderPresent(renderer);
    }

    CerrarAudio();
    if(fuenteGrande) TTF_CloseFont(fuenteGrande); if(fuentePequena) TTF_CloseFont(fuentePequena); if(fuenteMini) TTF_CloseFont(fuenteMini);
    SDL_DestroyTexture(texBackground);
    SDL_DestroyTexture(texIniciar_ES); SDL_DestroyTexture(texIniciar_EN);
    SDL_DestroyTexture(texUpdates_ES); SDL_DestroyTexture(texUpdates_EN);
    SDL_DestroyTexture(texAgrade_ES); SDL_DestroyTexture(texAgrade_EN);
    SDL_DestroyTexture(texIdioma_ES);  SDL_DestroyTexture(texIdioma_EN);
    SDL_DestroyTexture(texGaleria_ES); SDL_DestroyTexture(texGaleria_EN);
    SDL_DestroyTexture(texEditor_ES);  SDL_DestroyTexture(texEditor_EN);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    VPADShutdown(); IMG_Quit(); TTF_Quit(); SDL_Quit(); WHBLogConsoleFree(); WHBProcShutdown();
    return 0;
}