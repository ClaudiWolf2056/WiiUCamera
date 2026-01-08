#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <vpad/input.h>
#include <unistd.h>
#include <vector>
#include <string>
#include "camara.h" // Usamos el nuevo camara.h corregido

const char* APP_VERSION = "v0.5.0 Beta";

enum EstadoApp {
    ESTADO_MENU_PRINCIPAL,
    ESTADO_SUBMENU_MODOS,
    ESTADO_UPDATES,
    ESTADO_AGRADECIMIENTOS,
    ESTADO_CAMARA
};

SDL_Texture* CargarTextura(SDL_Renderer* renderer, const std::string& ruta) {
    SDL_Surface* superficie = IMG_Load(ruta.c_str());
    if (!superficie) return nullptr;
    SDL_Texture* textura = SDL_CreateTextureFromSurface(renderer, superficie);
    SDL_FreeSurface(superficie);
    return textura;
}

void DibujarTexto(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int x, int y, SDL_Color color) {
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

void DibujarTextoCentrado(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int y, SDL_Color color) {
    if (!font) return;
    int w, h;
    TTF_SizeText(font, texto, &w, &h);
    DibujarTexto(renderer, font, texto, (1280 - w) / 2, y, color);
}

void DibujarBoton(SDL_Renderer* renderer, SDL_Texture* textura, int x, int y, bool seleccionado) {
    if (!textura) return;
    int w, h;
    SDL_QueryTexture(textura, NULL, NULL, &w, &h);
    SDL_Rect rectBoton = { x, y, w, h };

    if (seleccionado) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_Rect rectBorde = { x - 6, y - 6, w + 12, h + 12 };
        SDL_RenderFillRect(renderer, &rectBorde);
    }
    SDL_RenderCopy(renderer, textura, NULL, &rectBoton);
}

int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);
    VPADInit();

    SDL_Window *window = SDL_CreateWindow("WiiUCamera", 0, 0, 1280, 720, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* fuenteGrande = TTF_OpenFont("/vol/content/font.ttf", 64);
    TTF_Font* fuentePequena = TTF_OpenFont("/vol/content/font.ttf", 40);
    TTF_Font* fuenteMini = TTF_OpenFont("/vol/content/font.ttf", 24); 

    // --- CARGA DE RECURSOS CORREGIDA ---
    SDL_Texture* texBackground = CargarTextura(renderer, "/vol/content/background.png");

    // ESPAÑOL (Nombres basados en lo que subiste)
    SDL_Texture* texIniciar_ES = CargarTextura(renderer, "/vol/content/button_iniciar.png"); 
    SDL_Texture* texUpdates_ES = CargarTextura(renderer, "/vol/content/button_actualizaciones.png");
    SDL_Texture* texAgrade_ES = CargarTextura(renderer, "/vol/content/button_agradecimientos.png");
    SDL_Texture* texIdioma_ES  = CargarTextura(renderer, "/vol/content/button_idioma_es.png");
    
    // INGLÉS (Asegúrate de tener estos archivos)
    SDL_Texture* texIniciar_EN = CargarTextura(renderer, "/vol/content/button_iniciar_en.png");
    SDL_Texture* texUpdates_EN = CargarTextura(renderer, "/vol/content/button_updates_en.png");
    SDL_Texture* texAgrade_EN = CargarTextura(renderer, "/vol/content/button_agradecimientos_en.png");
    SDL_Texture* texIdioma_EN  = CargarTextura(renderer, "/vol/content/button_language_en.png");

    SDL_Color colorBlanco = {255, 255, 255, 255};
    SDL_Color colorAmarillo = {255, 255, 0, 255};

    bool appRunning = true;
    int estado = ESTADO_MENU_PRINCIPAL;
    int seleccion = 0;
    
    bool esIngles = true; // Por defecto Inglés

    const int VELOCIDAD_CURSOR = 12;
    int delayInput = 0;

    while (appRunning) {
        VPADStatus vpad;
        VPADReadError error;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &error);

        SDL_RenderClear(renderer);

        // FONDO
        if (texBackground) SDL_RenderCopy(renderer, texBackground, NULL, NULL);
        else { SDL_SetRenderDrawColor(renderer, 0, 0, 100, 255); SDL_RenderClear(renderer); }

        if (delayInput > 0) delayInput--;

        // Inputs
        float stickY = vpad.leftStick.y;
        float stickX = vpad.leftStick.x;
        bool moverAbajo = (vpad.hold & VPAD_BUTTON_DOWN) || (stickY < -0.5f);
        bool moverArriba = (vpad.hold & VPAD_BUTTON_UP) || (stickY > 0.5f);
        bool moverDer   = (vpad.hold & VPAD_BUTTON_RIGHT) || (stickX > 0.5f);
        bool moverIzq   = (vpad.hold & VPAD_BUTTON_LEFT) || (stickX < -0.5f);
        bool botonA = (vpad.trigger & VPAD_BUTTON_A);
        bool botonB = (vpad.trigger & VPAD_BUTTON_B);

        if (estado == ESTADO_MENU_PRINCIPAL) {
            // TÍTULO: Bajado a 120 para no chocar
            DibujarTextoCentrado(renderer, fuenteGrande, "WiiUCamera", 120, colorBlanco);
            DibujarTexto(renderer, fuenteMini, APP_VERSION, 20, 680, colorBlanco);

            SDL_Texture* btn1 = esIngles ? texIniciar_EN : texIniciar_ES;
            SDL_Texture* btn2 = esIngles ? texUpdates_EN : texUpdates_ES;
            SDL_Texture* btn3 = esIngles ? texAgrade_EN : texAgrade_ES;
            SDL_Texture* btn4 = esIngles ? texIdioma_EN : texIdioma_ES;
            
            SDL_Texture* lista[] = { btn1, btn2, btn3, btn4 };
            
            // Cuadrícula 2x2
            int col1_x = 320, col2_x = 660;
            int row1_y = 300, row2_y = 450;
            int posX[] = { col1_x, col2_x, col1_x, col2_x };
            int posY[] = { row1_y, row1_y, row2_y, row2_y };

            for (int i = 0; i < 4; i++) DibujarBoton(renderer, lista[i], posX[i], posY[i], (i == seleccion));

            if (delayInput == 0) {
                if (moverDer)   { if (seleccion == 0) seleccion=1; else if(seleccion==2) seleccion=3; delayInput=VELOCIDAD_CURSOR; }
                if (moverIzq)   { if (seleccion == 1) seleccion=0; else if(seleccion==3) seleccion=2; delayInput=VELOCIDAD_CURSOR; }
                if (moverAbajo) { if (seleccion == 0) seleccion=2; else if(seleccion==1) seleccion=3; delayInput=VELOCIDAD_CURSOR; }
                if (moverArriba){ if (seleccion == 2) seleccion=0; else if(seleccion==3) seleccion=1; delayInput=VELOCIDAD_CURSOR; }

                if (botonA) {
                    if (seleccion == 0) { estado = ESTADO_SUBMENU_MODOS; seleccion = 0; }
                    else if (seleccion == 1) { estado = ESTADO_UPDATES; }
                    else if (seleccion == 2) { estado = ESTADO_AGRADECIMIENTOS; }
                    else if (seleccion == 3) { esIngles = !esIngles; }
                    delayInput = 20;
                }
                if (vpad.trigger & VPAD_BUTTON_HOME) appRunning = false;
            }

        } else if (estado == ESTADO_SUBMENU_MODOS) {
            // CORREGIDO: Títulos bajados a 120
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Camera Mode" : "Modo de Camara", 120, colorBlanco);

            const char* opEN[] = { "Take photo (normal)", "Record video", "Take photo (effects)", "Soon..." };
            const char* opES[] = { "Tomar foto (normal)", "Grabar video", "Tomar foto (efectos)", "Pronto..." };
            int numSub = 4;

            for (int i = 0; i < numSub; i++) {
                SDL_Color col = (i == seleccion) ? colorAmarillo : colorBlanco;
                // Texto bajado a partir de 300
                DibujarTextoCentrado(renderer, fuentePequena, esIngles ? opEN[i] : opES[i], 300 + (i * 70), col);
            }

            if (delayInput == 0) {
                if (moverAbajo) { seleccion++; if (seleccion >= numSub) seleccion = 0; delayInput = VELOCIDAD_CURSOR; }
                if (moverArriba) { seleccion--; if (seleccion < 0) seleccion = numSub - 1; delayInput = VELOCIDAD_CURSOR; }
                if (botonB) { estado = ESTADO_MENU_PRINCIPAL; seleccion = 0; delayInput = 30; }
                
                // Si presionan A en cualquier opción, vamos a la cámara
                if (botonA) { 
                    estado = ESTADO_CAMARA; 
                    delayInput = 30; 
                }
            }

        } else if (estado == ESTADO_UPDATES || estado == ESTADO_AGRADECIMIENTOS) {
            bool isUpdates = (estado == ESTADO_UPDATES);
            const char* titEN = isUpdates ? "Updates" : "Credits";
            const char* titES = isUpdates ? "Actualizaciones" : "Agradecimientos";
            const char* bodEN = isUpdates ? "More updates soon..." : "Credits coming soon";
            const char* bodES = isUpdates ? "Mas actualizaciones pronto..." : "Creditos proximamente";
            const char* retEN = "(Press B to return)";
            const char* retES = "(Presiona B para volver)";

            // CORREGIDO: Posiciones bajadas
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? titEN : titES, 120, colorBlanco);
            DibujarTextoCentrado(renderer, fuentePequena, esIngles ? bodEN : bodES, 350, colorBlanco);
            DibujarTextoCentrado(renderer, fuentePequena, esIngles ? retEN : retES, 600, colorBlanco);
            
            if (botonB) { 
                estado = ESTADO_MENU_PRINCIPAL; 
                seleccion = isUpdates ? 1 : 2; 
                delayInput = 30; 
            }

        } else if (estado == ESTADO_CAMARA) {
            // Ejecutar cámara y esperar resultado
            int res = EjecutarCamara(renderer);
            if (res == 1) estado = ESTADO_SUBMENU_MODOS; // Volver con B
            else if (res == -1) appRunning = false; // Salir con HOME
            delayInput = 30;
        }

        SDL_RenderPresent(renderer);
    }

    // LIMPIEZA FINAL
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