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
#include <fstream> 

#include "qrcodegen.hpp"
using namespace qrcodegen;

#include "camara.h" 
#include "recorder.h" 
#include "camera_effects.h" 
#include "gallery_master.h" 
#include "gallery.h"
#include "editor.h"
#include "mic_test.h" 
#include "webcam.h" 
#include "chroma.h" 
#include "editor3d.h"
#include "ia_menu.h"
#include "puzzle.h"

const char* APP_VERSION = "v2.0.0"; 

std::string ROOT_PATH = "";
void DetectarRutaRaiz() {
    struct stat buffer;
    if (stat("/vol/content/background.png", &buffer) == 0) { ROOT_PATH = "/vol/content"; } 
    else {
        char cwd[1024]; if (getcwd(cwd, sizeof(cwd)) != NULL) ROOT_PATH = std::string(cwd) + "/content";
        else ROOT_PATH = "fs:/vol/external01/wiiu/apps/WiiUCamera/content";
    }
}
std::string GetPath(const std::string& filename) { return ROOT_PATH + "/" + filename; }

SDL_Texture* CargarTextura(SDL_Renderer* renderer, const std::string& nombreArchivo) {
    std::string ruta = GetPath(nombreArchivo); SDL_Surface* sup = IMG_Load(ruta.c_str()); if (!sup) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, sup); SDL_FreeSurface(sup); return tex;
}

SDL_Texture* GenerarTexturaQR(SDL_Renderer* renderer, const char* texto) {
    QrCode qr = QrCode::encodeText(texto, QrCode::Ecc::MEDIUM);
    int size = qr.getSize(); int scale = 5; int border = 2; int texSize = (size + border * 2) * scale;
    SDL_Surface* surface = SDL_CreateRGBSurface(0, texSize, texSize, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 255, 255, 255)); 
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (qr.getModule(x, y)) {
                SDL_Rect r = { (x + border) * scale, (y + border) * scale, scale, scale };
                SDL_FillRect(surface, &r, SDL_MapRGB(surface->format, 0, 0, 0));
            }
        }
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface); SDL_FreeSurface(surface); return texture;
}

enum EstadoApp { 
    ESTADO_MENU_PRINCIPAL, ESTADO_SUBMENU_MODOS, ESTADO_UPDATES, ESTADO_AGRADECIMIENTOS, 
    ESTADO_CAMARA, ESTADO_GALERIA, ESTADO_EDITOR, 
    ESTADO_MAS_OPCIONES, ESTADO_INFO_WEBCAM, ESTADO_INFO_CHROMA, ESTADO_INFO_3D, ESTADO_AJUSTES,
    ESTADO_IA_MENU, ESTADO_INFO_IA, ESTADO_INFO_VIBE, ESTADO_INFO_PUZZLE 
};

const float MAIN_ADC_MIN_X = 100.0f; const float MAIN_ADC_MAX_X = 3950.0f;
const float MAIN_ADC_MIN_Y = 100.0f; const float MAIN_ADC_MAX_Y = 3900.0f;
const float MAIN_APP_W = 1280.0f; const float MAIN_APP_H = 720.0f;

int MapearMain(float val, float min, float max, float outMax, bool inv) {
    float pct = (val - min) / (max - min); if (pct < 0) pct = 0; if (pct > 1) pct = 1; if (inv) pct = 1.0f - pct; return (int)(pct * outMax);
}

Mix_Music* g_MusicaFondo = NULL; 
Mix_Music* g_MusicaGaleria = NULL; 
Mix_Chunk* g_SfxMove = NULL; 
Mix_Chunk* g_SfxSelect = NULL;

int g_VolMusica = 60; 
int g_VolSFX = 80;
bool g_EnableGalleryMusic = true;

void ActualizarVolumen() { Mix_VolumeMusic(g_VolMusica); Mix_Volume(-1, g_VolSFX); }

void IniciarAudio() {
    if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return;
    g_MusicaFondo = Mix_LoadMUS(GetPath("music.mp3").c_str()); 
    g_MusicaGaleria = Mix_LoadMUS(GetPath("gallery_bgm.mp3").c_str()); 
    g_SfxMove = Mix_LoadWAV(GetPath("move.wav").c_str());
    g_SfxSelect = Mix_LoadWAV(GetPath("select.wav").c_str());
    if (g_MusicaFondo) Mix_PlayMusic(g_MusicaFondo, -1);
    ActualizarVolumen();
}

void CerrarAudio() {
    Mix_HaltMusic(); Mix_HaltChannel(-1); 
    if (g_SfxMove) Mix_FreeChunk(g_SfxMove); 
    if (g_SfxSelect) Mix_FreeChunk(g_SfxSelect); 
    if (g_MusicaFondo) Mix_FreeMusic(g_MusicaFondo); 
    if (g_MusicaGaleria) Mix_FreeMusic(g_MusicaGaleria); 
    Mix_CloseAudio(); Mix_Quit();
}

void ReproducirSonidoMover() { if (g_SfxMove && g_VolSFX > 0) Mix_PlayChannel(-1, g_SfxMove, 0); }
void ReproducirSonidoSelect() { if (g_SfxSelect && g_VolSFX > 0) Mix_PlayChannel(-1, g_SfxSelect, 0); }

void DibujarTextoCentrado(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int y, SDL_Color color) {
    if (!font) return; SDL_Surface* s = TTF_RenderText_Blended(font, texto, color);
    if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); SDL_Rect r = { (1280 - s->w) / 2, y, s->w, s->h }; SDL_RenderCopy(renderer, t, NULL, &r); SDL_FreeSurface(s); SDL_DestroyTexture(t); }
}

void DibujarBoton(SDL_Renderer* renderer, SDL_Texture* textura, int x, int y, int w, int h, bool seleccionado, SDL_Rect* outRect) {
    if (outRect) { outRect->x = x; outRect->y = y; outRect->w = w; outRect->h = h; } if (!textura) return;
    SDL_Rect r = { x, y, w, h };
    if (seleccionado) { SDL_SetRenderDrawColor(renderer, 255, 230, 0, 255); SDL_Rect rBorde = { x - 4, y - 4, w + 8, h + 8 }; SDL_RenderFillRect(renderer, &rBorde); }
    SDL_RenderCopy(renderer, textura, NULL, &r);
}

void DibujarBarraVolumen(SDL_Renderer* renderer, int x, int y, int w, int h, int valor, int maxVal, bool seleccionado) {
    SDL_Rect bg = {x, y, w, h}; SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_RenderFillRect(renderer, &bg);
    float porcentaje = (float)valor / (float)maxVal; int wFill = (int)(w * porcentaje); SDL_Rect fill = {x, y, wFill, h};
    if (seleccionado) SDL_SetRenderDrawColor(renderer, 255, 230, 0, 255); else SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255); 
    SDL_RenderFillRect(renderer, &fill); SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &bg);
}

void DibujarToggle(SDL_Renderer* renderer, int x, int y, int w, int h, bool estado, bool seleccionado, TTF_Font* font) {
    SDL_Rect rBg = {x, y, w, h};
    if (estado) SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); else SDL_SetRenderDrawColor(renderer, 150, 50, 50, 255);
    SDL_RenderFillRect(renderer, &rBg);
    if(seleccionado) { SDL_SetRenderDrawColor(renderer, 255, 230, 0, 255); SDL_Rect rB = {x-4, y-4, w+8, h+8}; SDL_RenderDrawRect(renderer, &rB); } 
    else { SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &rBg); }
    const char* txt = estado ? "ON" : "OFF"; SDL_Surface* s = TTF_RenderText_Blended(font, txt, {255,255,255,255});
    if(s){ SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); SDL_Rect rT = {x + (w-s->w)/2, y + (h-s->h)/2, s->w, s->h}; SDL_RenderCopy(renderer, t, NULL, &rT); SDL_FreeSurface(s); SDL_DestroyTexture(t); }
}

void DibujarBarraInferiorGlobal(SDL_Renderer* renderer, TTF_Font* font, const char* izq, const char* der, const char* cen) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); 
    SDL_Rect bar = {0, 660, 1280, 60}; SDL_RenderFillRect(renderer, &bar); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_Color col = {255, 255, 255, 255}; 
    if (izq && *izq) { SDL_Surface* s=TTF_RenderText_Blended(font,izq,col); if(s){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s); SDL_Rect r={30,675,s->w,s->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s); SDL_DestroyTexture(t); } }
    if (der && *der) { SDL_Surface* s=TTF_RenderText_Blended(font,der,col); if(s){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s); SDL_Rect r={1250-s->w,675,s->w,s->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s); SDL_DestroyTexture(t); } }
    if (cen && *cen) { SDL_Surface* s=TTF_RenderText_Blended(font,cen,col); if(s){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s); SDL_Rect r={(1280-s->w)/2,675,s->w,s->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s); SDL_DestroyTexture(t); } }
}

void DibujarCursorTactil(SDL_Renderer* renderer, int x, int y) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 255, 255, 255, 150); 
    SDL_Rect r = { x - 10, y - 10, 20, 20 }; SDL_RenderFillRect(renderer, &r); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); 
}

bool VerificarToqueBoton(int tx, int ty, SDL_Rect btn) {
    int m = 15; return (tx >= (btn.x - m) && tx <= (btn.x + btn.w + m) && ty >= (btn.y - m) && ty <= (btn.y + btn.h + m));
}

int main(int argc, char **argv) {
    WHBProcInit(); WHBLogConsoleInit(); 
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO); TTF_Init(); 
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG); 
    
    VPADInit(); 
    DetectarRutaRaiz(); mkdir("fs:/vol/external01/WiiUCamera Files", 0777); IniciarAudio();

    SDL_Window *window = SDL_CreateWindow("WiiUCamera", 0, 0, 1280, 720, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    TTF_Font* fuenteGrande = TTF_OpenFont(GetPath("font.ttf").c_str(), 64);
    TTF_Font* fuentePequena = TTF_OpenFont(GetPath("font.ttf").c_str(), 40);
    TTF_Font* fuenteMini = TTF_OpenFont(GetPath("font.ttf").c_str(), 24); 

    SDL_Texture* texBg = CargarTextura(renderer, "background.png");
    SDL_Texture* texStart_ES = CargarTextura(renderer, "button_iniciar.png"); SDL_Texture* texStart_EN = CargarTextura(renderer, "button_iniciar_en.png");
    SDL_Texture* texGal_ES = CargarTextura(renderer, "button_gallery.png");   SDL_Texture* texGal_EN = CargarTextura(renderer, "button_gallery_en.png");
    SDL_Texture* texEdit_ES = CargarTextura(renderer, "button_editor.png");   SDL_Texture* texEdit_EN = CargarTextura(renderer, "button_editor_en.png");
    SDL_Texture* texLang_ES = CargarTextura(renderer, "button_idioma_es.png"); SDL_Texture* texLang_EN = CargarTextura(renderer, "button_language_en.png");
    SDL_Texture* texMore_ES = CargarTextura(renderer, "button_more_es.png");   SDL_Texture* texMore_EN = CargarTextura(renderer, "button_more_en.png");
    SDL_Texture* iconUpd = CargarTextura(renderer, "icon_updates.png");
    SDL_Texture* iconCred = CargarTextura(renderer, "icon_credits.png");
    SDL_Texture* iconSet = CargarTextura(renderer, "icon_settings.png");
    SDL_Texture* iconVibe = CargarTextura(renderer, "icon_vibe.png");
    
    SDL_Texture* texGuide1 = CargarTextura(renderer, "guide_chroma.png");
    SDL_Texture* texGuide2 = CargarTextura(renderer, "guide_magic.png");
    SDL_Texture* texQR = GenerarTexturaQR(renderer, "https://www.youtube.com/watch?v=E8xQsfuRHuA");

    SDL_Texture* texHeadCam_EN = CargarTextura(renderer, "header_camera_en.png");
    SDL_Texture* texHeadCam_ES = CargarTextura(renderer, "header_camera_es.png");
    SDL_Texture* texBtnPhoto_EN = CargarTextura(renderer, "button_photo_en.png");
    SDL_Texture* texBtnPhoto_ES = CargarTextura(renderer, "button_photo_es.png");
    SDL_Texture* texBtnVideo_EN = CargarTextura(renderer, "button_video_en.png");
    SDL_Texture* texBtnVideo_ES = CargarTextura(renderer, "button_video_es.png");
    SDL_Texture* texBtnFx_EN = CargarTextura(renderer, "button_effects_en.png");
    SDL_Texture* texBtnFx_ES = CargarTextura(renderer, "button_effects_es.png");
    SDL_Texture* texBtnMic_EN = CargarTextura(renderer, "button_mic_en.png");
    SDL_Texture* texBtnMic_ES = CargarTextura(renderer, "button_mic_es.png");
    
    // Novedad: Texturas del menú Más Opciones
    SDL_Texture* tMoreWebcamEN = CargarTextura(renderer, "btn_more_webcam_en.png");
    SDL_Texture* tMoreWebcamES = CargarTextura(renderer, "btn_more_webcam_es.png");
    SDL_Texture* tMoreChromaEN = CargarTextura(renderer, "btn_more_chroma_en.png");
    SDL_Texture* tMoreChromaES = CargarTextura(renderer, "btn_more_chroma_es.png");
    SDL_Texture* tMore3DEN = CargarTextura(renderer, "btn_more_3d_en.png");
    SDL_Texture* tMore3DES = CargarTextura(renderer, "btn_more_3d_es.png");
    SDL_Texture* tMoreAiEN = CargarTextura(renderer, "btn_more_ai_en.png");
    SDL_Texture* tMoreAiES = CargarTextura(renderer, "btn_more_ai_es.png");
    SDL_Texture* tMorePuzzleEN = CargarTextura(renderer, "btn_more_puzzle_en.png");
    SDL_Texture* tMorePuzzleES = CargarTextura(renderer, "btn_more_puzzle_es.png");

    SDL_Color colW = {255, 255, 255, 255}; SDL_Color colY = {255, 255, 0, 255};
    
    bool appRunning = true; int estado = ESTADO_MENU_PRINCIPAL; int seleccion = 2; bool esIngles = true;
    const int VEL_CURSOR = 12; int delayInput = 0; SDL_Rect btnRects[9]; 
    int touchX = -100, touchY = -100; bool dedo = false; float scrollY = 0; int tStartY = -1;
    int selAjustes = 0; float scrollChroma = 0.0f; float scrollVibe = 0.0f;

    while (appRunning && WHBProcIsRunning()) {
        SDL_Event event; while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) appRunning = false; }
        
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err); 
        
        if (vpad.tpNormal.touched) {
            dedo = true;
            touchX = MapearMain(vpad.tpNormal.x, MAIN_ADC_MIN_X, MAIN_ADC_MAX_X, MAIN_APP_W, false);
            touchY = MapearMain(vpad.tpNormal.y, MAIN_ADC_MIN_Y, MAIN_ADC_MAX_Y, MAIN_APP_H, true); 
            if (tStartY == -1) tStartY = touchY;
            if (estado == ESTADO_INFO_CHROMA && tStartY != -1) { int d = touchY - tStartY; if (abs(d) > 5) { scrollChroma -= d; if (scrollChroma < 0) scrollChroma = 0; tStartY = touchY; } }
            if (estado == ESTADO_AGRADECIMIENTOS && tStartY != -1) { int d = touchY - tStartY; if (abs(d) > 5) { scrollY -= d; if (scrollY < 0) scrollY = 0; tStartY = touchY; } }
            if (estado == ESTADO_INFO_VIBE && tStartY != -1) { int d = touchY - tStartY; if (abs(d) > 5) { scrollVibe -= d * 2; if (scrollVibe < 0) scrollVibe = 0; tStartY = touchY; } }
            if (estado != ESTADO_MENU_PRINCIPAL && estado != ESTADO_AJUSTES && estado != ESTADO_MAS_OPCIONES && estado != ESTADO_SUBMENU_MODOS && estado != ESTADO_INFO_WEBCAM && estado != ESTADO_INFO_CHROMA && estado != ESTADO_INFO_3D && estado != ESTADO_INFO_VIBE && touchY > 600 && touchX > 1100 && delayInput == 0) {
                 if (estado != ESTADO_CAMARA && estado != ESTADO_GALERIA && estado != ESTADO_EDITOR) {
                    estado = ESTADO_MENU_PRINCIPAL; delayInput = 30; ReproducirSonidoSelect(); tStartY = -1;
                 }
            }
            if (estado == ESTADO_MENU_PRINCIPAL && delayInput == 0) {
                for(int i=0; i<9; i++) {
                    if (VerificarToqueBoton(touchX, touchY, btnRects[i])) {
                         seleccion = i; ReproducirSonidoSelect();
                         if (i==0) estado = ESTADO_UPDATES; else if (i==1) { estado = ESTADO_AGRADECIMIENTOS; scrollY=0; } else if (i==8) { estado = ESTADO_INFO_VIBE; }
                         else if (i==2) { estado = ESTADO_SUBMENU_MODOS; seleccion=0; } else if (i==3) estado = ESTADO_GALERIA;
                         else if (i==4) estado = ESTADO_EDITOR; else if (i==5) { estado = ESTADO_MAS_OPCIONES; seleccion = 0; }
                         else if (i==6) esIngles = !esIngles; else if (i==7) { estado = ESTADO_AJUSTES; selAjustes = 0; } 
                         delayInput = 30; tStartY = -1; 
                    }
                }
            }
            if (estado == ESTADO_MAS_OPCIONES && delayInput == 0) {
                int btnW = 250; int btnH = 75;
                int totalGroupW = btnW + 20 + 120;
                int startX = (1280 - totalGroupW) / 2;
                int startY = 240; 
                
                for(int i=0; i<5; i++) {
                    SDL_Rect rT = { startX, startY + (i*85), btnW, btnH };
                    if (VerificarToqueBoton(touchX, touchY, rT)) { 
                        seleccion = i; ReproducirSonidoSelect(); 
                        if(i==0) { Mix_PauseMusic(); EjecutarWebcam(renderer, fuenteMini, esIngles); Mix_ResumeMusic(); }
                        else if(i==1) { Mix_PauseMusic(); EjecutarChroma(renderer, fuenteMini, ROOT_PATH); Mix_ResumeMusic(); } 
                        else if(i==2) { Mix_PauseMusic(); EjecutarGenerador3D(renderer, fuenteMini, esIngles); Mix_ResumeMusic(); } 
                        else if(i==3) { estado = ESTADO_IA_MENU; delayInput = 30; }
                        else if(i==4) { Mix_PauseMusic(); EjecutarPuzzle(renderer, fuenteMini, esIngles); if(g_VolMusica > 0) Mix_ResumeMusic(); delayInput = 30;}
                        
                        if (!WHBProcIsRunning()) { appRunning = false; break; }
                        delayInput = 30; tStartY = -1; 
                    }
                    
                    SDL_Rect rInfo = { rT.x + btnW + 20, startY + (i*85) + 12, 120, 50 }; 
                    if (VerificarToqueBoton(touchX, touchY, rInfo)) { 
                        ReproducirSonidoSelect(); 
                        if(i==0) estado = ESTADO_INFO_WEBCAM;
                        if(i==1) { estado = ESTADO_INFO_CHROMA; scrollChroma = 0; }
                        if(i==2) estado = ESTADO_INFO_3D;
                        if(i==3) estado = ESTADO_INFO_IA;
                        if(i==4) estado = ESTADO_INFO_PUZZLE;
                        delayInput = 30; 
                    }
                }
                if (touchY > 600) { estado = ESTADO_MENU_PRINCIPAL; seleccion = 5; delayInput=30; }
            }
            if ((estado == ESTADO_INFO_WEBCAM || estado == ESTADO_INFO_3D || estado == ESTADO_INFO_IA || estado == ESTADO_INFO_PUZZLE) && delayInput == 0 && touchY > 600) { estado = ESTADO_MAS_OPCIONES; delayInput = 30; ReproducirSonidoSelect(); }
            if (estado == ESTADO_AJUSTES && delayInput == 0) {
                if (touchY > 230 && touchY < 290) { selAjustes = 0; float pct = (float)(touchX - 340) / 600.0f; if (pct < 0) pct = 0; if (pct > 1) pct = 1; g_VolMusica = (int)(pct * 128); ActualizarVolumen(); }
                if (touchY > 360 && touchY < 420) { selAjustes = 1; float pct = (float)(touchX - 340) / 600.0f; if (pct < 0) pct = 0; if (pct > 1) pct = 1; g_VolSFX = (int)(pct * 128); ActualizarVolumen(); }
                if (touchY > 490 && touchY < 550) { selAjustes = 2; if(touchX > 340 && touchX < 940) { g_EnableGalleryMusic = !g_EnableGalleryMusic; ReproducirSonidoSelect(); delayInput = 20; } }
                if (touchY > 600) { ReproducirSonidoSelect(); estado = ESTADO_MENU_PRINCIPAL; seleccion = 7; delayInput = 30; }
            }
            if (estado == ESTADO_SUBMENU_MODOS && delayInput == 0) {
                 int btnW = 250; int btnH = 100; int gapX = 60; int gapY = 40;
                 int startX = (1280 - (btnW * 2 + gapX)) / 2; int startY = 340;
                 SDL_Rect rCam[4] = {
                     {startX, startY, btnW, btnH},
                     {startX + btnW + gapX, startY, btnW, btnH},
                     {startX, startY + btnH + gapY, btnW, btnH},
                     {startX + btnW + gapX, startY + btnH + gapY, btnW, btnH}
                 };
                 for(int i=0; i<4; i++) { 
                     if (VerificarToqueBoton(touchX, touchY, rCam[i])) { 
                         seleccion = i; ReproducirSonidoSelect(); estado = ESTADO_CAMARA; delayInput = 30; tStartY = -1; 
                     } 
                 }
                 if (touchY > 600) { estado = ESTADO_MENU_PRINCIPAL; seleccion = 2; delayInput=30; }
            }
        } else { dedo = false; tStartY = -1; }

        SDL_RenderClear(renderer);
        if (texBg) SDL_RenderCopy(renderer, texBg, NULL, NULL); else { SDL_SetRenderDrawColor(renderer, 0,0,50,255); SDL_RenderClear(renderer); }
        if (delayInput > 0) delayInput--;

        float sy = vpad.leftStick.y; float sx = vpad.leftStick.x;
        bool down = (vpad.hold & VPAD_BUTTON_DOWN) || (sy < -0.5f); bool up = (vpad.hold & VPAD_BUTTON_UP) || (sy > 0.5f);
        bool right = (vpad.hold & VPAD_BUTTON_RIGHT) || (sx > 0.5f); bool left = (vpad.hold & VPAD_BUTTON_LEFT) || (sx < -0.5f);
        bool btnA = (vpad.trigger & VPAD_BUTTON_A); bool btnB = (vpad.trigger & VPAD_BUTTON_B);
        bool moved = false;

        if (estado == ESTADO_MENU_PRINCIPAL) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Welcome!" : "Bienvenido!", 160, colW);
            DibujarTextoCentrado(renderer, fuenteMini, esIngles ? "Touchscreen now works!" : "El tactil ya funciona!", 230, colY);
            DibujarTextoCentrado(renderer, fuenteMini, esIngles ? "Remember to return here before closing the app" : "Recuerda volver a este menu antes de cerrar la aplicacion", 270, colY);
            
            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, APP_VERSION, "(D-Pad) Navigate", "(A) Select"); 
            else DibujarBarraInferiorGlobal(renderer, fuenteMini, APP_VERSION, "(Cruceta) Navegar", "(A) Seleccionar");
            
            SDL_Texture* bStart = esIngles ? texStart_EN : texStart_ES; SDL_Texture* bGal = esIngles ? texGal_EN : texGal_ES; 
            SDL_Texture* bEdit = esIngles ? texEdit_EN : texEdit_ES; SDL_Texture* bMore = esIngles ? texMore_EN : texMore_ES; 
            SDL_Texture* bLang = esIngles ? texLang_EN : texLang_ES;
            
            DibujarBoton(renderer, iconUpd, 30, 130, 80, 80, (seleccion==0), &btnRects[0]); 
            DibujarBoton(renderer, iconCred, 130, 130, 80, 80, (seleccion==1), &btnRects[1]);
            DibujarBoton(renderer, iconVibe, 230, 130, 80, 80, (seleccion==8), &btnRects[8]);
            
            int btnW = 250; int btnH = 100; int gap = 30;
            int r1Y = 340; int galX = (1280 - btnW) / 2; int startX = galX - gap - btnW; int editX = galX + gap + btnW; 
            
            DibujarBoton(renderer, bStart, startX, r1Y, btnW, btnH, (seleccion==2), &btnRects[2]); 
            DibujarBoton(renderer, bGal, galX, r1Y, btnW, btnH, (seleccion==3), &btnRects[3]); 
            DibujarBoton(renderer, bEdit, editX, r1Y, btnW, btnH, (seleccion==4), &btnRects[4]); 
            
            int r2Y = 470; int gap2 = 40; int moreX = 640 - (gap2/2) - btnW; int langX = 640 + (gap2/2);        
            
            DibujarBoton(renderer, bMore, moreX, r2Y, btnW, btnH, (seleccion==5), &btnRects[5]); 
            DibujarBoton(renderer, bLang, langX, r2Y, btnW, btnH, (seleccion==6), &btnRects[6]); 
            
            DibujarBoton(renderer, iconSet, 1160, 580, 80, 80, (seleccion==7), &btnRects[7]);
            
            const char* txtBug = esIngles ? "Found a bug? Report it at: claudiwolf2056@gmail.com" : "Encontraste algun bug? Reportalo en: claudiwolf2056@gmail.com";
            DibujarTextoCentrado(renderer, fuenteMini, txtBug, 600, colW);

            const char* txtSD = esIngles ? "Photos saved in: SD Card > WiiUCamera Files! :)" : "Fotos guardadas en: SD > WiiUCamera Files! :)"; 
            DibujarTextoCentrado(renderer, fuenteMini, txtSD, 630, colW);
            
            if (delayInput == 0) {
                if (right) { if(seleccion==0) seleccion=1; else if(seleccion==1) seleccion=8; else if(seleccion==2) seleccion=3; else if(seleccion==3) seleccion=4; else if(seleccion==5) seleccion=6; else if(seleccion==6) seleccion=7; delayInput=VEL_CURSOR; moved=true; }
                if (left) { if(seleccion==8) seleccion=1; else if(seleccion==1) seleccion=0; else if(seleccion==3) seleccion=2; else if(seleccion==4) seleccion=3; else if(seleccion==6) seleccion=5; else if(seleccion==7) seleccion=6; delayInput=VEL_CURSOR; moved=true; }
                if (down) { if(seleccion==8) seleccion=4; else if(seleccion<=1) seleccion=2; else if(seleccion>=2 && seleccion<=4) seleccion=5; else if(seleccion>=5 && seleccion<=6) seleccion=7; delayInput=VEL_CURSOR; moved=true; }
                if (up) { if(seleccion==7) seleccion=6; else if(seleccion>=5 && seleccion<=6) seleccion=3; else if(seleccion==2||seleccion==3) seleccion=0; else if(seleccion==4) seleccion=8; delayInput=VEL_CURSOR; moved=true; }
                if (moved) ReproducirSonidoMover();
                if (btnA) { ReproducirSonidoSelect();
                    if (seleccion==0) estado=ESTADO_UPDATES; else if (seleccion==1) { estado=ESTADO_AGRADECIMIENTOS; scrollY=0; }
                    else if (seleccion==2) { estado=ESTADO_SUBMENU_MODOS; seleccion=0; } else if (seleccion==3) estado=ESTADO_GALERIA; 
                    else if (seleccion==4) estado=ESTADO_EDITOR; else if (seleccion==5) { estado=ESTADO_MAS_OPCIONES; seleccion=0; }
                    else if (seleccion==6) esIngles=!esIngles; else if (seleccion==7) { estado=ESTADO_AJUSTES; selAjustes=0; }
                    else if (seleccion==8) { estado=ESTADO_INFO_VIBE; } 
                    delayInput=20;
                }
            }
        
        } else if (estado == ESTADO_AJUSTES) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Settings" : "Ajustes", 160, colY);
            int yMus = 250; DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "Music Volume" : "Volumen Musica", yMus - 50, colW); DibujarBarraVolumen(renderer, 340, yMus, 600, 40, g_VolMusica, 128, (selAjustes==0));
            int ySfx = 380; DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "SFX Volume" : "Volumen Efectos", ySfx - 50, colW); DibujarBarraVolumen(renderer, 340, ySfx, 600, 40, g_VolSFX, 128, (selAjustes==1));
            int yGal = 510; DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "Gallery Music" : "Musica Galeria", yGal - 50, colW); DibujarToggle(renderer, 340, yGal, 600, 40, g_EnableGalleryMusic, (selAjustes==2), fuenteMini);
            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "Select", "Adjust", "(B) Save"); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "Seleccionar", "Ajustar", "(B) Guardar");
            if (delayInput == 0) {
                if (down) { selAjustes++; if(selAjustes > 2) selAjustes=0; delayInput=VEL_CURSOR; ReproducirSonidoMover(); }
                if (up) { selAjustes--; if(selAjustes < 0) selAjustes=2; delayInput=VEL_CURSOR; ReproducirSonidoMover(); }
                bool cambioVol = false;
                if (right) { if (selAjustes==0) { g_VolMusica += 8; if(g_VolMusica > 128) g_VolMusica=128; cambioVol=true; } else if (selAjustes==1) { g_VolSFX += 8; if(g_VolSFX > 128) g_VolSFX=128; cambioVol=true; } else if (selAjustes==2) { g_EnableGalleryMusic = !g_EnableGalleryMusic; delayInput=20; ReproducirSonidoSelect(); } }
                if (left) { if (selAjustes==0) { g_VolMusica -= 8; if(g_VolMusica < 0) g_VolMusica=0; cambioVol=true; } else if (selAjustes==1) { g_VolSFX -= 8; if(g_VolSFX < 0) g_VolSFX=0; cambioVol=true; } else if (selAjustes==2) { g_EnableGalleryMusic = !g_EnableGalleryMusic; delayInput=20; ReproducirSonidoSelect(); } }
                if (btnA && selAjustes==2) { g_EnableGalleryMusic = !g_EnableGalleryMusic; delayInput=20; ReproducirSonidoSelect(); }
                if (cambioVol) { ActualizarVolumen(); delayInput=5; }
                if (btnB) { ReproducirSonidoSelect(); estado = ESTADO_MENU_PRINCIPAL; seleccion = 7; delayInput = 30; }
            }
        } else if (estado == ESTADO_MAS_OPCIONES) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "More Options" : "Mas Opciones", 160, colY);
            
            SDL_Texture* texList[5];
            if (esIngles) {
                texList[0] = tMoreWebcamEN; texList[1] = tMoreChromaEN; texList[2] = tMore3DEN; texList[3] = tMoreAiEN; texList[4] = tMorePuzzleEN;
            } else {
                texList[0] = tMoreWebcamES; texList[1] = tMoreChromaES; texList[2] = tMore3DES; texList[3] = tMoreAiES; texList[4] = tMorePuzzleES;
            }
            
            int btnW = 250; int btnH = 75;
            int totalGroupW = btnW + 20 + 120;
            int startX = (1280 - totalGroupW) / 2;
            int startY = 240; 
            
            for (int i = 0; i < 5; i++) { 
                SDL_Rect rBtn = { startX, startY + (i*85), btnW, btnH };
                DibujarBoton(renderer, texList[i], rBtn.x, rBtn.y, rBtn.w, rBtn.h, (seleccion == i), NULL);
                
                int idInfo = 10 + i; 
                SDL_Rect rInfo = { rBtn.x + btnW + 20, startY + (i*85) + 12, 120, 50 }; 
                SDL_Color cInfo = (seleccion == idInfo) ? colY : (SDL_Color){0,150,255,255}; 
                
                SDL_SetRenderDrawColor(renderer, cInfo.r, cInfo.g, cInfo.b, cInfo.a); 
                SDL_RenderFillRect(renderer, &rInfo); 
                
                SDL_Surface* s=TTF_RenderText_Blended(fuenteMini, "[ ! ] Info", {255,255,255,255}); 
                if(s){ 
                    SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s); 
                    SDL_Rect r={rInfo.x + (120-s->w)/2, rInfo.y + (50-s->h)/2, s->w, s->h}; 
                    SDL_RenderCopy(renderer,t,NULL,&r); 
                    SDL_FreeSurface(s); SDL_DestroyTexture(t); 
                } 
            }
            
            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "(A) Select - (Right) Info", "(B) Back", ""); 
            else DibujarBarraInferiorGlobal(renderer, fuenteMini, "(A) Seleccionar - (Der) Info", "(B) Atras", "");
            
            if (delayInput == 0) { 
                if (down) { if (seleccion >= 10) { seleccion++; if(seleccion>=15) seleccion=10; } else { seleccion++; if (seleccion>=5) seleccion=0; } delayInput=VEL_CURSOR; moved=true; } 
                if (up) { if (seleccion >= 10) { seleccion--; if(seleccion<10) seleccion=14; } else { seleccion--; if (seleccion<0) seleccion=4; } delayInput=VEL_CURSOR; moved=true; } 
                if (right && (seleccion >= 0 && seleccion <= 4)) { seleccion += 10; delayInput=VEL_CURSOR; moved=true; } 
                if (left && (seleccion >= 10 && seleccion <= 14)) { seleccion -= 10; delayInput=VEL_CURSOR; moved=true; } 
                if (moved) ReproducirSonidoMover(); 
                
                if (btnB) { estado = ESTADO_MENU_PRINCIPAL; seleccion=5; delayInput = 30; } 
                if (btnA) { 
                    ReproducirSonidoSelect(); 
                    if (seleccion == 10) { estado = ESTADO_INFO_WEBCAM; } 
                    else if (seleccion == 11) { estado = ESTADO_INFO_CHROMA; scrollChroma = 0; } 
                    else if (seleccion == 12) { estado = ESTADO_INFO_3D; } 
                    else if (seleccion == 13) { estado = ESTADO_INFO_IA; } 
                    else if (seleccion == 14) { estado = ESTADO_INFO_PUZZLE; } 
                    
                    else if (seleccion == 0) { Mix_PauseMusic(); EjecutarWebcam(renderer, fuenteMini, esIngles); Mix_ResumeMusic(); } 
                    else if (seleccion == 1) { Mix_PauseMusic(); EjecutarChroma(renderer, fuenteMini, ROOT_PATH); Mix_ResumeMusic(); } 
                    else if (seleccion == 2) { Mix_PauseMusic(); EjecutarGenerador3D(renderer, fuenteMini, esIngles); Mix_ResumeMusic(); } 
                    else if (seleccion == 3) { estado = ESTADO_IA_MENU; delayInput = 30; }
                    else if (seleccion == 4) { Mix_PauseMusic(); EjecutarPuzzle(renderer, fuenteMini, esIngles); if(g_VolMusica > 0) Mix_ResumeMusic(); delayInput = 30;} 
                    
                    if (!WHBProcIsRunning()) { appRunning = false; break; }
                    delayInput = 30; 
                } 
            }
        } else if (estado == ESTADO_INFO_WEBCAM) {
             SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255); SDL_RenderClear(renderer); DibujarTextoCentrado(renderer, fuenteGrande, "Webcam Info", 60, colY);
             const char* t1 = esIngles ? "Clean Feed Mode: No UI, Full Screen." : "Modo Limpio: Sin interfaz, Pantalla Completa."; const char* t2 = esIngles ? "Use with Capture Card." : "Usalo con Capturadora."; const char* t3 = esIngles ? "Inspiration Video:" : "Video de Inspiracion:";
             DibujarTextoCentrado(renderer, fuenteMini, t1, 130, colW); DibujarTextoCentrado(renderer, fuenteMini, t2, 160, colW); DibujarTextoCentrado(renderer, fuentePequena, t3, 220, {0,255,255,255}); if (texQR) { SDL_Rect rQR = { (1280-300)/2, 270, 300, 300 }; SDL_RenderCopy(renderer, texQR, NULL, &rQR); }
             if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Atras", "");
             if (btnB && delayInput == 0) { estado = ESTADO_MAS_OPCIONES; seleccion=10; delayInput = 30; }
        } else if (estado == ESTADO_INFO_CHROMA) {
             SDL_SetRenderDrawColor(renderer, 20, 30, 20, 255); SDL_RenderClear(renderer); DibujarTextoCentrado(renderer, fuenteGrande, "Chroma Key Guide", 50, colY); SDL_Rect rClip = {0, 100, 1280, 560}; SDL_RenderSetClipRect(renderer, &rClip); int startY = 120 - (int)scrollChroma; SDL_Color cTit = {0, 255, 0, 255}; SDL_Color cTxt = colW; int spacing = 450; 
             DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "1. Green Screen Mode (Default)" : "1. Modo Pantalla Verde", startY, cTit); if (esIngles) { DibujarTextoCentrado(renderer, fuenteMini, "Use a green cloth background.", startY + 40, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "Wii U replaces it with the default or uploaded image.", startY + 70, cTxt); } else { DibujarTextoCentrado(renderer, fuenteMini, "Usa un estandarte de color verde.", startY + 40, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "La Wii U lo reemplazara con la imagen (Default/Subida).", startY + 70, cTxt); } if (texGuide1) { SDL_Rect rI = {(1280-500)/2, startY + 100, 500, 280}; SDL_RenderCopy(renderer, texGuide1, NULL, &rI); }
             int y2 = startY + spacing; DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "2. Magic Mode (No Green Screen)" : "2. Modo Magico (Sin Tela)", y2, cTit); if (esIngles) { DibujarTextoCentrado(renderer, fuenteMini, "No Green Screen? No problem.", y2 + 40, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "Keep GamePad still (on a table).", y2 + 70, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "Step out of frame. Press (A) to scan. Then step back in.", y2 + 100, cTxt); } else { DibujarTextoCentrado(renderer, fuenteMini, "¿Sin fondo verde? No hay problema.", y2 + 40, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "Manten el GamePad quieto (en una mesa).", y2 + 70, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "Sal del cuadro. Presiona (A) para escanear. Vuelve a entrar.", y2 + 100, cTxt); } if (texGuide2) { SDL_Rect rI = {(1280-500)/2, y2 + 130, 500, 280}; SDL_RenderCopy(renderer, texGuide2, NULL, &rI); }
             int y3 = y2 + spacing + 30; DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "3. Custom Background" : "3. Fondo Personalizado", y3, cTit); DibujarTextoCentrado(renderer, fuenteMini, esIngles ? "Press (+) to upload via Wi-Fi (Phone/PC)." : "Presiona (+) para subir por Wi-Fi (Celular/PC).", y3 + 40, cTxt); DibujarTextoCentrado(renderer, fuenteMini, esIngles ? "Scan the QR code and select your image." : "Escanea el QR y selecciona tu imagen.", y3 + 70, cTxt); SDL_RenderSetClipRect(renderer, NULL); 
             int totalH = spacing * 3 + 100; int viewH = 560; if (totalH > viewH) { float pct = scrollChroma / (float)(totalH - viewH); int barH = viewH * viewH / totalH; int barY = 100 + (int)(pct * (viewH - barH)); SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_Rect bgBar = {1260, 100, 10, viewH}; SDL_RenderFillRect(renderer, &bgBar); SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); SDL_Rect fgBar = {1260, barY, 10, barH}; SDL_RenderFillRect(renderer, &fgBar); }
             if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "(D-Pad) Scroll", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "(Cruceta) Desplazar", "(B) Atras", "");
             if (down) scrollChroma += 15; if (up) scrollChroma -= 15; int maxScroll = totalH - viewH; if (scrollChroma < 0) scrollChroma = 0; if (scrollChroma > maxScroll) scrollChroma = maxScroll; if (btnB && delayInput == 0) { estado = ESTADO_MAS_OPCIONES; seleccion=11; delayInput = 30; }
        
       } else if (estado == ESTADO_INFO_VIBE) {
            SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
            SDL_RenderClear(renderer);

            SDL_Color colTitulo = {255, 255, 0, 255};
            SDL_Color colSubtitulo = {0, 255, 0, 255};
            SDL_Color colTexto = {255, 255, 255, 255};

            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Vibe Coding in WiiUCamera" : "Vibe Coding en WiiUCamera", 60, colTitulo);
            DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "Read it until the end!" : "Leelo hasta el final!", 135, colSubtitulo);

            const char* vibeTextES[] = {
                "Hola y un saludo a cada uno de los usuarios de WiiUCamera.",
                "Durante los testeos de esta actualizacion, decidi tomar todas", 
                "sus sugerencias que me dejaron",
                "por correo electronico y otros medios para poder incluir una parte de ustedes en esta app.",
                "Sin embargo, me percate de que, ademas de sus sugerencias, hubo algunas criticas",
                "con un tema algo peculiar.",
                "",
                "Las criticas son constructivas para el desarrollo de esta app y siempre son bien recibidas.",
                "Pero creo que no me deje explicar con un tema y se genero una pequena disputa",
                "por el supuesto uso excesivo del 100% IA para la creacion de esta app.",
                "",
                "ACLARACIONES:",
                "1. Desde la primera version de mi proyecto, deje en claro que use a Gemini IA para la creacion de este.",
                "Era mi primera vez programando para un entorno tan curioso que es la Wii U.",
                "Pero parece que hubo una malinterpretacion en esto:",
                "- Debido a que hay poca informacion o informacion no tan simplificada para uso general en mi idioma,",
                "busque fuentes confiables, consulte por informacion que pudiera instalar las librerias necesarias",
                "en el compilador para poder generar la app; pero como estaba perdido, necesite a la IA como",
                "fuente de recursos de informacion y capacitarme en un lapso corto.",
                "",
                "2. Gemini me ayudo a ideas esteticas (y tambien, un usuario de Discord llamado y3ss1n para la estetica actual).",
                "Porque, realmente la estetica antes del primer lanzamiento era horrible.",
                "Me ayudo a inspirarme a mejorar aspectos para la impresion de ustedes, animandome a hacer bocetos a mano",
                "y estructurando bloque por bloque.",
                "Que si bien no es perfecto, me gusto el estilo pasado y actual, ya que es simple y ordenado.",
                "",
                "3. Si bien Gemini colaboro con el codigo fuente de este proyecto, yo fui quien se encargo de corregir multiples errores,",
                "elaborar sintaxis enteras, innovar buscando de otros proyectos para tener una idea", 
                "y consultando con cada uno que entienda del tema.",
                "Tuve que sacar informacion que ni sabia que existia sobre el uso de la camara, y fue intento y error",
                "durante madrugadas enteras durante enero de este anio!",
                "Hacer (la mayoría) de los cuadros e imagenes en un editor digital a mano, componer la sinfonia del menu",
                "y muchas otras cosas.",
                "",
                "4. Solo fue un apoyo, me ayudo corrigiendome y dandome pautas rapidas sobre como programar una app en WiiU.",
                "Tambien aspectos morales, ya que tuve caidas (una vez tuve tantos errores despues de compilar mas de 53 veces en una noche).",
                "",
                "CONCLUSIONES:",
                "Entiendo algunas molestias de alguno por parte de usar IA, entiendo que, sin alma humana",
                "solo es una app vacia sintetica.",
                "Pero les aseguro, que gran parte de mi dedicacion, tiempo y emociones estan en este proyecto dedicado",
                "para cada uno de ustedes y darle una vida extra a la camara de Wii U.",
                "",
                "Ante ello, desde esta actualizacion, se minimizara el uso de IA y me comprometo ser pleno hasta",
                "dejar de usarlo, y usar el 100% mis capacidades y conocimientos de programacion y estructuracion para esta app.",
                "",
                "Recuerden que siempre escuchare sus sugerencias para mejorar la app.",
                "Y tambien que la IA siempre sera tu mejor aliado, siempre y cuando no la abuses.",
                "",
                "WiiUCamera recibira actualizaciones mas organicas despues de este mensaje :)",
                "",
                "Atentamente: ClaudiWolf2056",
                "",
                "PD: Puedes visitar mi pagina web: www.claudiwolf2056.com",
                "Estare publicando mas cosas en mi articulo de esta app pronto!",
                " Y ya se que esta pestania esta cortando algunas mayusculas, lo corregire pronto ",
                " Subire este articulo en mi pagina tambien ;)"
            };

            const char* vibeTextEN[] = {
                "Hello and greetings to every WiiUCamera user.",
                "During the testing of this update, I decided to take all the suggestions you sent me",
                "by email and other means so I could include a part of you in this app.",
                "However, I noticed that besides your suggestions, there were also some criticisms about",
                "a rather peculiar topic.",
                "",
                "Criticism is constructive for the development of this app and is always welcome.",
                "But I think I failed to explain myself clearly and a small dispute appeared",
                "about the supposed excessive use of 100% AI to create this app.",
                "",
                "CLARIFICATIONS:",
                "1. Since the first version of my project, I made it clear that I used Gemini AI to create it.",
                "It was my first time programming for such a curious environment as the Wii U.",
                "But there seems to have been a misunderstanding about this:",
                "- There is little information, or information not simplified enough for general use in my language,",
                "so I searched for reliable sources and information needed to install the libraries",
                "required by the compiler to build the app. Since I was lost, I needed AI as",
                "a source of information and to train myself in a short period of time.",
                "",
                "2. Gemini helped me with aesthetic ideas (and also a Discord user named y3ss1n for the current aesthetics).",
                "Because, honestly, the aesthetics before the first release were horrible.",
                "It inspired me to improve aspects for your experience, encouraging me to make hand-drawn sketches",
                "and structure everything block by block.",
                "Although it is not perfect, I liked the old and current style because it is simple and orderly.",
                "",
                "3. Although Gemini collaborated with the source code, I was the one who corrected multiple errors,",
                "wrote entire syntax sections, looked at other projects for ideas and consulted people who understand the subject.",
                "I had to learn information I did not even know existed about camera usage, through trial and error",
                "during many late nights throughout January this year!",
                "I made most of the boxes and images by hand in a digital editor, composed the menu symphony",
                "and many other things.",
                "",
                "4. It was only support. It helped correct me and gave me quick guidance on how to program an app on Wii U.",
                "It also helped morally when I had setbacks (once I had so many errors after compiling more than 53 times in one night).",
                "",
                "CONCLUSIONS:",
                "I understand some people's concerns about the use of AI. I understand that without a human soul,",
                "it can feel like a synthetic and empty app.",
                "But I assure you that a great part of my dedication, time and emotions are in this project, dedicated",
                "to each of you and to giving the Wii U camera an extra life.",
                "",
                "Therefore, starting with this update, AI use will be minimized and I commit myself to eventually",
                "stop using it and use 100% of my own programming and structuring abilities and knowledge for this app.",
                "",
                "Remember that I will always listen to your suggestions to improve the app.",
                "And AI will always be your best ally, as long as you do not abuse it.",
                "",
                "WiiUCamera will receive more organic updates after this message :)",
                "",
                "Sincerely: ClaudiWolf2056",
                "",
                "P.S. You can visit my website: www.claudiwolf2056.com",
                "I will be publishing more things about this app in my article soon!",
                "And I know this tab is cutting off some capital letters, I'll fix it soon.",
                "I'll upload this article to my page too ;)"
            };

            const char** vibeLines = esIngles ? vibeTextEN : vibeTextES;
            const int vibeCount = esIngles ? (int)(sizeof(vibeTextEN) / sizeof(vibeTextEN[0])) : (int)(sizeof(vibeTextES) / sizeof(vibeTextES[0]));

            const int startY = 215;
            const int lineSpacing = 28;
            const int viewH = 450;
            const int bottomPadding = 220;
            int contentHeight = vibeCount * lineSpacing;
            int vibeMaxScroll = contentHeight + bottomPadding - viewH;
            if (vibeMaxScroll < 0) vibeMaxScroll = 0;
            if (scrollVibe > vibeMaxScroll) scrollVibe = vibeMaxScroll;

            // Restringir el área de dibujado para que el texto de arriba desaparezca
            SDL_Rect rClip = {0, 190, 1280, viewH};
            SDL_RenderSetClipRect(renderer, &rClip);

            for (int i = 0; i < vibeCount; i++) {
                SDL_Color c = colTexto;
                if (i == 10 || i == 19 || i == 26 || i == 35) c = colSubtitulo;
                int y = startY - (int)scrollVibe + (i * lineSpacing);
                DibujarTextoCentrado(renderer, fuenteMini, vibeLines[i], y, c);
            }

            // Quitar el recorte antes de dibujar el resto de elementos (Barra espaciadora y UI base)
            SDL_RenderSetClipRect(renderer, NULL);

            if (vibeMaxScroll > 0) {
                float pct = scrollVibe / (float)vibeMaxScroll;
                int barH = (viewH * viewH) / (contentHeight + bottomPadding);
                if (barH < 35) barH = 35;
                int barY = 200 + (int)(pct * (viewH - barH));
                SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
                SDL_Rect bgBar = {1260, 200, 10, viewH};
                SDL_RenderFillRect(renderer, &bgBar);
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                SDL_Rect fgBar = {1260, barY, 10, barH};
                SDL_RenderFillRect(renderer, &fgBar);
            }

            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "(D-Pad) Scroll", "(B) Back", "");
            else DibujarBarraInferiorGlobal(renderer, fuenteMini, "(Cruceta) Desplazar", "(B) Atras", "");

            if (delayInput == 0) {
                if (down) {
                    scrollVibe += 70;
                    if (scrollVibe > vibeMaxScroll) scrollVibe = vibeMaxScroll;
                    delayInput = 2;
                }
                if (up) {
                    scrollVibe -= 70;
                    if (scrollVibe < 0) scrollVibe = 0;
                    delayInput = 2;
                }
                if (btnB) {
                    estado = ESTADO_MENU_PRINCIPAL;
                    seleccion = 8;
                    scrollVibe = 0;
                    delayInput = 30;
                }
            }
        } else if (estado == ESTADO_INFO_3D) {
             SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255); SDL_RenderClear(renderer); 
             DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Image to 3D Guide" : "Guia: Imagen a 3D", 60, colY);
             const char* t1 = esIngles ? "Use the stylus to draw over your photo." : "Usa el lapiz para dibujar sobre tu foto y moldea a tu gusto"; 
             const char* t2 = esIngles ? "A Whisk3D for Wii U experiment, it will be progressively improved." : "Un experimento de Whisk3D for Wii U, se mejorara progresivamente"; 
             const char* t3 = esIngles ? "Use the 3D model export system from a device to Wii U with caution." : "Usa el sistema de exportacion de modelos 3D de un dispositivo a Wii U con cuidado";
             DibujarTextoCentrado(renderer, fuenteMini, t1, 150, colW); 
             DibujarTextoCentrado(renderer, fuenteMini, t2, 180, colW); 
             DibujarTextoCentrado(renderer, fuenteMini, t3, 240, {0,255,255,255});
             if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Atras", "");
             if (btnB && delayInput == 0) { estado = ESTADO_MAS_OPCIONES; seleccion=12; delayInput = 30; }
             
        } else if (estado == ESTADO_INFO_IA) {
             SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255); SDL_RenderClear(renderer); 
             DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "AI Tools Info" : "Guia: Herramientas IA", 60, colY);
             
             const char* t1 = esIngles ? "Use: Select a photo to get a detailed AI analysis." : "Uso: Selecciona una imagen para obtener un analisis detallado."; 
             const char* t2 = esIngles ? "Requires internet connection. First request may take 45s to boot." : "Requiere conexion a internet. El primer uso puede tardar 45s en cargar."; 
             const char* t3 = esIngles ? "Note: Images are processed securely and discarded immediately." : "Nota: Las imagenes se procesan de forma segura y no se almacenan.";
             const char* t4 = esIngles ? "Experimental beta phase, expect errors" : "Fase beta experimental, espere errores";
             
             DibujarTextoCentrado(renderer, fuentePequena, t1, 150, colW); 
             DibujarTextoCentrado(renderer, fuenteMini, t2, 210, colW); 
             DibujarTextoCentrado(renderer, fuenteMini, t3, 260, {0,255,255,255});
             DibujarTextoCentrado(renderer, fuenteMini, t4, 300, {0,255,255,255});
             
             if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Atras", "");
             if (btnB && delayInput == 0) { estado = ESTADO_MAS_OPCIONES; seleccion=13; delayInput = 30; }

        } else if (estado == ESTADO_INFO_PUZZLE) {
             SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255); SDL_RenderClear(renderer); 
             DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Puzzle Info" : "Guia: Rompecabezas", 60, colY);
             
             const char* t1 = esIngles ? "Select any photo to turn it into a jigsaw puzzle!" : "Selecciona cualquier foto para convertirla en un rompecabezas."; 
             const char* t2 = esIngles ? "Touch one piece, then touch another to swap them." : "Toca una pieza y luego otra para intercambiarlas de lugar."; 
             const char* t3 = esIngles ? "Try to beat your own time!" : "¡Intenta superar tu propio tiempo record!";
             
             DibujarTextoCentrado(renderer, fuentePequena, t1, 150, colW); 
             DibujarTextoCentrado(renderer, fuenteMini, t2, 210, colW); 
             DibujarTextoCentrado(renderer, fuenteMini, t3, 260, {0,255,255,255});
             
             if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Atras", "");
             if (btnB && delayInput == 0) { estado = ESTADO_MAS_OPCIONES; seleccion=14; delayInput = 30; }
             
        } else if (estado == ESTADO_SUBMENU_MODOS) {

            SDL_Texture* tHead = esIngles ? texHeadCam_EN : texHeadCam_ES;
            if (tHead) {
                int w, h; SDL_QueryTexture(tHead, NULL, NULL, &w, &h);
                SDL_Rect rHead = { (1280 - w) / 2, 230, w, h }; 
                SDL_RenderCopy(renderer, tHead, NULL, &rHead);
            } else {
                DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Camera Mode" : "Modo de Camara", 230, colW);
            }

            int btnW = 250; int btnH = 100; int gapX = 60; int gapY = 40;
            int startX = (1280 - (btnW * 2 + gapX)) / 2; int startY = 340;
            
            SDL_Rect rCam[4] = {
                {startX, startY, btnW, btnH},
                {startX + btnW + gapX, startY, btnW, btnH},
                {startX, startY + btnH + gapY, btnW, btnH},
                {startX + btnW + gapX, startY + btnH + gapY, btnW, btnH}
            };

            SDL_Texture* tB0 = esIngles ? texBtnPhoto_EN : texBtnPhoto_ES;
            SDL_Texture* tB1 = esIngles ? texBtnVideo_EN : texBtnVideo_ES;
            SDL_Texture* tB2 = esIngles ? texBtnFx_EN : texBtnFx_ES;
            SDL_Texture* tB3 = esIngles ? texBtnMic_EN : texBtnMic_ES;

            DibujarBoton(renderer, tB0, rCam[0].x, rCam[0].y, btnW, btnH, (seleccion==0), NULL);
            DibujarBoton(renderer, tB1, rCam[1].x, rCam[1].y, btnW, btnH, (seleccion==1), NULL);
            DibujarBoton(renderer, tB2, rCam[2].x, rCam[2].y, btnW, btnH, (seleccion==2), NULL);
            DibujarBoton(renderer, tB3, rCam[3].x, rCam[3].y, btnW, btnH, (seleccion==3), NULL);
            

            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "(A) Confirm", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "(A) Confirmar", "(B) Atras", "");
            
            if (delayInput == 0) { 
                
                if (right) { if(seleccion==0) seleccion=1; else if(seleccion==2) seleccion=3; delayInput=VEL_CURSOR; moved=true; }
                if (left)  { if(seleccion==1) seleccion=0; else if(seleccion==3) seleccion=2; delayInput=VEL_CURSOR; moved=true; }
                if (down)  { if(seleccion==0) seleccion=2; else if(seleccion==1) seleccion=3; delayInput=VEL_CURSOR; moved=true; }
                if (up)    { if(seleccion==2) seleccion=0; else if(seleccion==3) seleccion=1; delayInput=VEL_CURSOR; moved=true; }
                
                if (moved) ReproducirSonidoMover(); 
                if (btnB) { estado = ESTADO_MENU_PRINCIPAL; seleccion = 2; delayInput = 30; } 
                if (btnA) { ReproducirSonidoSelect(); estado = ESTADO_CAMARA; delayInput = 30; } 
            }
            
        } else if (estado == ESTADO_UPDATES) {
             DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Changelog" : "Novedades", 160, colY); int y = 250; int gap = 45; SDL_Color colTxt = colW;
             if(esIngles){ 
                DibujarTextoCentrado(renderer, fuentePequena, "v2.0.0 - Community Update!", y, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NEW: Listen to suggestions from the community", y + gap*1.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NEW: VC Clarification, a New Era and light in camera", y + gap*2.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- Internal improvements", y + gap*3.5, colTxt);
            } else { 
                DibujarTextoCentrado(renderer, fuentePequena, "v2.0.0 - Actualizacion de la comunidad", y, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NUEVO: Escucha de sugerencias de la comunidad", y + gap*1.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NUEVO: Aclaracion de VC, una nueva era y luz", y + gap*2.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- Mejoras internas", y + gap*3.5, colTxt);
            }
             if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Atras", "");
             if (btnB && delayInput == 0) { estado = ESTADO_MENU_PRINCIPAL; delayInput = 30; }
        } else if (estado == ESTADO_AGRADECIMIENTOS) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Credits" : "Creditos", 160, colY); SDL_Rect clip = { 50, 250, 1180, 390 }; SDL_RenderSetClipRect(renderer, &clip);
            const char* linesEN[] = { "Created by: ClaudiWolf2056", "", "Special Thanks to:", "whateveritwas", "For providing part of his code", "for the exit logic of this app", "You can also find him on Github", "", "Wii U Community (Latam)", "- p-anthoX", "- JEAN_PRETENDO", "Bryanks como tester 2.0.0", "- Santix Aldama", "- Downyjarl", "- Keines", "", "Facebook Community:", "Da****, Ro****", "Ce***, and more <3", "", "Technical Help:", "ForTheUsers (4TU)", "My SD Card", "", "Libraries:", "SDL2 for Wii U - devkitPro", "", "See you soon! - ClaudiWolf2056" };
            const char* linesES[] = { "Creado por: ClaudiWolf2056", "", "Agradecimientos especiales a:", "whateveritwas", "Por brindar una parte de su codigo", "para la logica de cierre de esta app", "Tambien puedes encontrarlo en Github", "", "Comunidad Wii U (Latam)", "- p-anthoX", "- JEAN_PRETENDO", "Bryanks - tester 2.0.0", "- Santix Aldama", "- Downyjarl", "- Keines", "", "Comunidad de Facebook:", "Da****, Ro****", "Ce***, y muchos mas <3", "", "Ayuda Tecnica:", "ForTheUsers (4TU)", "Mi tarjeta SD xd", "", "Librerias:", "SDL2 (Wii U) - devkitPro", "", "Gracias por su interes en mi app", ":)", "", "Nos vemos pronto! - ClaudiWolf2056" };
            int ct = 27; const char** ls = esIngles ? linesEN : linesES;
            for(int i=0; i<ct; i++) { SDL_Surface* s=TTF_RenderText_Blended(fuentePequena, ls[i], colW); if(s){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s); SDL_Rect r={(1280-s->w)/2, (int)(250-scrollY+(i*50)), s->w, s->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s); SDL_DestroyTexture(t); } }
            SDL_RenderSetClipRect(renderer, NULL); if (down) scrollY += 5; if (up && scrollY > 0) scrollY -= 5;
            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "(D-Pad) Scroll", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "(Cruceta) Desplazar", "(B) Atras", "");
            if (btnB && delayInput == 0) { estado = ESTADO_MENU_PRINCIPAL; delayInput = 30; }
            
        } else if (estado == ESTADO_GALERIA) {
            Mix_HaltMusic(); 
            if (g_EnableGalleryMusic && g_MusicaGaleria && g_VolMusica > 0) {
                Mix_PlayMusic(g_MusicaGaleria, 0); 
            }

            int tipo = EjecutarGalleryMaster(renderer, fuenteGrande, fuentePequena, fuenteMini, esIngles);
            
            if (!WHBProcIsRunning()) { appRunning = false; break; }

            if (tipo != -1) {
                if (tipo == -2) appRunning = false;
                else {
                    int res = EjecutarGaleria(renderer, fuenteMini, esIngles, tipo, g_MusicaGaleria, g_EnableGalleryMusic);
                    if (res == -1) appRunning = false;
                    
                    if (!WHBProcIsRunning()) { appRunning = false; break; }
                }
            }
            
            Mix_HaltMusic();
            if (g_MusicaFondo && g_VolMusica > 0) Mix_PlayMusic(g_MusicaFondo, -1);
            estado = ESTADO_MENU_PRINCIPAL; 
            seleccion = 3; 
            delayInput = 30;
            
        } else if (estado == ESTADO_EDITOR) {
            Mix_PauseMusic(); 
            EjecutarEditor(renderer, fuenteMini, esIngles); 
            
            if (!WHBProcIsRunning()) { appRunning = false; break; }
            
            if(g_VolMusica > 0) Mix_ResumeMusic(); 
            estado = ESTADO_MENU_PRINCIPAL; seleccion = 4; delayInput = 30;
        
        } else if (estado == ESTADO_CAMARA) {
            Mix_PauseMusic(); dedo = false; int res = 0;
            if (seleccion == 0) res = EjecutarCamara(renderer, fuenteMini, esIngles); 
            else if (seleccion == 1) res = EjecutarGrabadora(renderer, fuenteMini, esIngles);
            else if (seleccion == 2) res = EjecutarCamaraEfectos(renderer, fuenteMini, esIngles); 
            else if (seleccion == 3) res = EjecutarPruebaMic(renderer, fuenteMini, esIngles); 
            else res = 0; 
            
            if (!WHBProcIsRunning()) { appRunning = false; break; }

            if (res != -1) { 
                if (g_VolMusica > 0) { if (Mix_PausedMusic()) Mix_ResumeMusic(); else Mix_PlayMusic(g_MusicaFondo, -1); }
                SDL_RenderClear(renderer); SDL_RenderPresent(renderer); 
                
                estado = ESTADO_SUBMENU_MODOS; 
                
            } else { appRunning = false; }
            delayInput = 30;
            
        } else if (estado == ESTADO_IA_MENU) {
            Mix_PauseMusic(); 
            EjecutarMenuIA(renderer, fuenteGrande, fuentePequena, fuenteMini, esIngles);
            
            if (!WHBProcIsRunning()) { appRunning = false; break; }
            
            if(g_VolMusica > 0) Mix_ResumeMusic(); 
            estado = ESTADO_MAS_OPCIONES; seleccion = 3; delayInput = 30;
        }
        
        if (dedo) DibujarCursorTactil(renderer, touchX, touchY);
        
        if(appRunning) SDL_RenderPresent(renderer);
    }
    
    // Liberación de recursos
    if(texHeadCam_EN) SDL_DestroyTexture(texHeadCam_EN); if(texHeadCam_ES) SDL_DestroyTexture(texHeadCam_ES);
    if(texBtnPhoto_EN) SDL_DestroyTexture(texBtnPhoto_EN); if(texBtnPhoto_ES) SDL_DestroyTexture(texBtnPhoto_ES);
    if(texBtnVideo_EN) SDL_DestroyTexture(texBtnVideo_EN); if(texBtnVideo_ES) SDL_DestroyTexture(texBtnVideo_ES);
    if(texBtnFx_EN) SDL_DestroyTexture(texBtnFx_EN); if(texBtnFx_ES) SDL_DestroyTexture(texBtnFx_ES);
    if(texBtnMic_EN) SDL_DestroyTexture(texBtnMic_EN); if(texBtnMic_ES) SDL_DestroyTexture(texBtnMic_ES);

    if(tMoreWebcamEN) SDL_DestroyTexture(tMoreWebcamEN); if(tMoreWebcamES) SDL_DestroyTexture(tMoreWebcamES);
    if(tMoreChromaEN) SDL_DestroyTexture(tMoreChromaEN); if(tMoreChromaES) SDL_DestroyTexture(tMoreChromaES);
    if(tMore3DEN) SDL_DestroyTexture(tMore3DEN); if(tMore3DES) SDL_DestroyTexture(tMore3DES);
    if(tMoreAiEN) SDL_DestroyTexture(tMoreAiEN); if(tMoreAiES) SDL_DestroyTexture(tMoreAiES);
    if(tMorePuzzleEN) SDL_DestroyTexture(tMorePuzzleEN); if(tMorePuzzleES) SDL_DestroyTexture(tMorePuzzleES);

    CerrarAudio(); TTF_CloseFont(fuenteGrande); TTF_CloseFont(fuentePequena); TTF_CloseFont(fuenteMini);
    SDL_DestroyTexture(texBg); SDL_DestroyTexture(texStart_ES); SDL_DestroyTexture(texStart_EN); SDL_DestroyTexture(texGal_ES); SDL_DestroyTexture(texGal_EN);
    SDL_DestroyTexture(texEdit_ES); SDL_DestroyTexture(texEdit_EN); SDL_DestroyTexture(texLang_ES); SDL_DestroyTexture(texLang_EN); SDL_DestroyTexture(texMore_ES); SDL_DestroyTexture(texMore_EN);
    SDL_DestroyTexture(iconUpd); SDL_DestroyTexture(iconCred); SDL_DestroyTexture(iconSet); SDL_DestroyTexture(iconVibe); SDL_DestroyTexture(texQR);
    if(texGuide1) SDL_DestroyTexture(texGuide1); if(texGuide2) SDL_DestroyTexture(texGuide2);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    VPADShutdown(); IMG_Quit(); TTF_Quit(); SDL_Quit(); WHBLogConsoleFree(); WHBProcShutdown();
    return 0;
}