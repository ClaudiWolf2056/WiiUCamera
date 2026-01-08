#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h> 
#include <vpad/input.h>
#include <coreinit/foreground.h>
#include <unistd.h>
#include <vector>
#include <string>

// INCLUIMOS NUESTROS MODULOS
#include "camara.h" 
#include "recorder.h" // <--- ¡NUEVO! Para el video AVI

const char* APP_VERSION = "v0.9.0 Video Beta";

enum EstadoApp {
    ESTADO_MENU_PRINCIPAL,
    ESTADO_SUBMENU_MODOS,
    ESTADO_UPDATES,
    ESTADO_AGRADECIMIENTOS,
    ESTADO_CAMARA
};

// --- AUDIO ---
Mix_Music* g_MusicaFondo = NULL;
Mix_Chunk* g_SfxMove = NULL;
Mix_Chunk* g_SfxSelect = NULL;

void IniciarAudio() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return;
    g_MusicaFondo = Mix_LoadMUS("/vol/content/music.mp3"); 
    g_SfxMove = Mix_LoadWAV("/vol/content/move.wav");
    g_SfxSelect = Mix_LoadWAV("/vol/content/select.wav");

    if (g_MusicaFondo) {
        Mix_VolumeMusic(50); 
        Mix_PlayMusic(g_MusicaFondo, -1); 
    }
    Mix_Volume(-1, 64); 
}

void CerrarAudio() {
    Mix_HaltMusic(); 
    Mix_HaltChannel(-1);
    if (g_SfxMove) Mix_FreeChunk(g_SfxMove);
    if (g_SfxSelect) Mix_FreeChunk(g_SfxSelect);
    if (g_MusicaFondo) Mix_FreeMusic(g_MusicaFondo);
    Mix_CloseAudio();
}

void ReproducirSonidoMover() { if (g_SfxMove) Mix_PlayChannel(-1, g_SfxMove, 0); }
void ReproducirSonidoSelect() { if (g_SfxSelect) Mix_PlayChannel(-1, g_SfxSelect, 0); }

// --- GRÁFICOS ---
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

// Cursor táctil (Cuadro rojo transparente)
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
    
    IniciarAudio();

    SDL_Window *window = SDL_CreateWindow("WiiUCamera", 0, 0, 1280, 720, 0);
    // VSync Activado para evitar rayas
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    TTF_Font* fuenteGrande = TTF_OpenFont("/vol/content/font.ttf", 64);
    TTF_Font* fuentePequena = TTF_OpenFont("/vol/content/font.ttf", 40);
    TTF_Font* fuenteMini = TTF_OpenFont("/vol/content/font.ttf", 24); 

    SDL_Texture* texBackground = CargarTextura(renderer, "/vol/content/background.png");
    
    SDL_Texture* texIniciar_ES = CargarTextura(renderer, "/vol/content/button_iniciar.png"); 
    SDL_Texture* texUpdates_ES = CargarTextura(renderer, "/vol/content/button_actualizaciones.png");
    SDL_Texture* texAgrade_ES = CargarTextura(renderer, "/vol/content/button_agradecimientos.png");
    SDL_Texture* texIdioma_ES  = CargarTextura(renderer, "/vol/content/button_idioma_es.png");
    
    SDL_Texture* texIniciar_EN = CargarTextura(renderer, "/vol/content/button_iniciar_en.png");
    SDL_Texture* texUpdates_EN = CargarTextura(renderer, "/vol/content/button_updates_en.png"); 
    SDL_Texture* texAgrade_EN = CargarTextura(renderer, "/vol/content/button_agradecimientos_en.png");
    SDL_Texture* texIdioma_EN  = CargarTextura(renderer, "/vol/content/button_language_en.png");

    SDL_Color colorBlanco = {255, 255, 255, 255};
    SDL_Color colorAmarillo = {255, 255, 0, 255};

    bool appRunning = true;
    int estado = ESTADO_MENU_PRINCIPAL;
    int seleccion = 0;
    bool esIngles = true;
    const int VELOCIDAD_CURSOR = 12;
    int delayInput = 0;
    
    SDL_Rect btnRects[4]; 
    int touchX = -100, touchY = -100;
    bool dedoPresionado = false;

    while (appRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                appRunning = false;
            }
            else if (event.type == SDL_MOUSEMOTION) {
                touchX = event.motion.x;
                touchY = event.motion.y;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                dedoPresionado = true;
                touchX = event.button.x;
                touchY = event.button.y;
            }
            else if (event.type == SDL_MOUSEBUTTONUP) {
                dedoPresionado = false;
            }
        }

        VPADStatus vpad; VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

        SDL_RenderClear(renderer);
        
        if (texBackground) SDL_RenderCopy(renderer, texBackground, NULL, NULL);
        else { SDL_SetRenderDrawColor(renderer, 0, 0, 100, 255); SDL_RenderClear(renderer); }

        if (delayInput > 0) delayInput--;

        // Inputs Físicos
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

            SDL_Texture* btn1 = esIngles ? texIniciar_EN : texIniciar_ES;
            SDL_Texture* btn2 = esIngles ? texUpdates_EN : texUpdates_ES;
            SDL_Texture* btn3 = esIngles ? texAgrade_EN : texAgrade_ES;
            SDL_Texture* btn4 = esIngles ? texIdioma_EN : texIdioma_ES;
            SDL_Texture* lista[] = { btn1, btn2, btn3, btn4 };
            
            int centrosX[] = { 480, 800, 480, 800 };
            int centrosY[] = { 350, 350, 500, 500 };

            for (int i = 0; i < 4; i++) {
                DibujarBotonCentrado(renderer, lista[i], centrosX[i], centrosY[i], (i == seleccion), &btnRects[i]);
                
                if (dedoPresionado && VerificarToqueBoton(touchX, touchY, btnRects[i].x, btnRects[i].y, btnRects[i].w, btnRects[i].h)) {
                    if (seleccion != i) { seleccion = i; ReproducirSonidoMover(); }
                    
                    if (delayInput == 0) {
                        ReproducirSonidoSelect();
                        if (seleccion == 0) { estado = ESTADO_SUBMENU_MODOS; seleccion = 0; }
                        else if (seleccion == 1) { estado = ESTADO_UPDATES; }
                        else if (seleccion == 2) { estado = ESTADO_AGRADECIMIENTOS; }
                        else if (seleccion == 3) { esIngles = !esIngles; }
                        dedoPresionado = false;
                        delayInput = 20;
                    }
                }
            }

            if (delayInput == 0) {
                if (moverDer)   { if (seleccion == 0) seleccion=1; else if(seleccion==2) seleccion=3; delayInput=VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (moverIzq)   { if (seleccion == 1) seleccion=0; else if(seleccion==3) seleccion=2; delayInput=VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (moverAbajo) { if (seleccion == 0) seleccion=2; else if(seleccion==1) seleccion=3; delayInput=VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (moverArriba){ if (seleccion == 2) seleccion=0; else if(seleccion==3) seleccion=1; delayInput=VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (huboMovimiento) ReproducirSonidoMover();

                if (botonA) {
                    ReproducirSonidoSelect();
                    if (seleccion == 0) { estado = ESTADO_SUBMENU_MODOS; seleccion = 0; }
                    else if (seleccion == 1) { estado = ESTADO_UPDATES; }
                    else if (seleccion == 2) { estado = ESTADO_AGRADECIMIENTOS; }
                    else if (seleccion == 3) { esIngles = !esIngles; }
                    delayInput = 20;
                }
            }

        } else if (estado == ESTADO_SUBMENU_MODOS) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Camera Mode" : "Modo de Camara", 120, colorBlanco);
            // 0: Fotos Normal, 1: Video Beta, 2: Efectos, 3: Pronto
            const char* opEN[] = { "Take photo (normal)", "Record video (Beta)", "Take photo (effects)", "Soon..." };
            const char* opES[] = { "Tomar foto (normal)", "Grabar video (Beta)", "Tomar foto (efectos)", "Pronto..." };
            
            int startY = 300;
            for (int i = 0; i < 4; i++) {
                SDL_Color col = (i == seleccion) ? colorAmarillo : colorBlanco;
                int textoY = startY + (i * 70);
                DibujarTextoCentrado(renderer, fuentePequena, esIngles ? opEN[i] : opES[i], textoY, col);

                if (dedoPresionado && VerificarToqueBoton(touchX, touchY, (1280-600)/2, textoY, 600, 50)) {
                     if (seleccion != i) { seleccion = i; ReproducirSonidoMover(); }
                     if (delayInput == 0) {
                        ReproducirSonidoSelect();
                        estado = ESTADO_CAMARA;
                        dedoPresionado = false;
                        delayInput = 30;
                     }
                }
            }

            if (delayInput == 0) {
                if (moverAbajo) { seleccion++; if (seleccion >= 4) seleccion = 0; delayInput = VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (moverArriba) { seleccion--; if (seleccion < 0) seleccion = 3; delayInput = VELOCIDAD_CURSOR; huboMovimiento=true; }
                if (huboMovimiento) ReproducirSonidoMover();

                if (botonB) { estado = ESTADO_MENU_PRINCIPAL; seleccion = 0; delayInput = 30; }
                if (botonA) { ReproducirSonidoSelect(); estado = ESTADO_CAMARA; delayInput = 30; }
            }

        } else if (estado == ESTADO_UPDATES || estado == ESTADO_AGRADECIMIENTOS) {
            bool isUpd = (estado == ESTADO_UPDATES);
            DibujarTextoCentrado(renderer, fuenteGrande, isUpd ? (esIngles?"Updates":"Actualizaciones") : (esIngles?"Credits":"Agradecimientos"), 120, colorBlanco);
            DibujarTextoCentrado(renderer, fuentePequena, isUpd ? (esIngles?"More updates soon...":"Mas actualizaciones pronto...") : (esIngles?"Credits coming soon":"Creditos proximamente"), 350, colorBlanco);
            DibujarTextoCentrado(renderer, fuentePequena, esIngles?"(Press B to return)":"(Presiona B para volver)", 600, colorBlanco);
            
            if (botonB || (dedoPresionado && delayInput == 0)) { 
                estado = ESTADO_MENU_PRINCIPAL; 
                seleccion = isUpd ? 1 : 2; 
                delayInput = 30; 
            }

        } else if (estado == ESTADO_CAMARA) {
            Mix_PauseMusic(); 
            dedoPresionado = false; // Reset táctil al entrar
            
            int res = 0;
            
            // --- DECISIÓN: ¿FOTO O VIDEO? ---
            if (seleccion == 1) { 
                // Opción 1 del menú es "Grabar video"
                res = EjecutarGrabadora(renderer, fuenteMini, esIngles);
            } else {
                // Opción 0, 2 o 3 por ahora van a la Cámara de Fotos normal
                res = EjecutarCamara(renderer, fuenteMini, esIngles);
            }

            Mix_ResumeMusic(); 
            SDL_RenderClear(renderer); 
            SDL_RenderPresent(renderer); 
            
            if (res == 1) estado = ESTADO_SUBMENU_MODOS; 
            else if (res == -1) appRunning = false; 
            delayInput = 30;
        }

        if (dedoPresionado) DibujarCursorTactil(renderer, touchX, touchY);

        SDL_RenderPresent(renderer);
    }

    CerrarAudio();
    if(fuenteGrande) TTF_CloseFont(fuenteGrande);
    if(fuentePequena) TTF_CloseFont(fuentePequena);
    if(fuenteMini) TTF_CloseFont(fuenteMini);
    SDL_DestroyTexture(texBackground);
    SDL_DestroyTexture(texIniciar_ES); SDL_DestroyTexture(texIniciar_EN);
    SDL_DestroyTexture(texUpdates_ES); SDL_DestroyTexture(texUpdates_EN);
    SDL_DestroyTexture(texAgrade_ES); SDL_DestroyTexture(texAgrade_EN);
    SDL_DestroyTexture(texIdioma_ES);  SDL_DestroyTexture(texIdioma_EN);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    VPADShutdown();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}