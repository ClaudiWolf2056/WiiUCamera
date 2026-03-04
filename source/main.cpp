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

const char* APP_VERSION = "v1.3.8"; 

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
    ESTADO_MAS_OPCIONES, ESTADO_INFO_WEBCAM, ESTADO_INFO_CHROMA, ESTADO_AJUSTES 
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
    

    SDL_Color colW = {255, 255, 255, 255}; SDL_Color colY = {255, 255, 0, 255};
    
    bool appRunning = true; int estado = ESTADO_MENU_PRINCIPAL; int seleccion = 2; bool esIngles = true;
    const int VEL_CURSOR = 12; int delayInput = 0; SDL_Rect btnRects[8]; 
    int touchX = -100, touchY = -100; bool dedo = false; float scrollY = 0; int tStartY = -1;
    int selAjustes = 0; float scrollChroma = 0.0f;

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
            if (estado != ESTADO_MENU_PRINCIPAL && estado != ESTADO_AJUSTES && estado != ESTADO_MAS_OPCIONES && estado != ESTADO_SUBMENU_MODOS && estado != ESTADO_INFO_WEBCAM && estado != ESTADO_INFO_CHROMA && touchY > 600 && touchX > 1100 && delayInput == 0) {
                 if (estado != ESTADO_CAMARA && estado != ESTADO_GALERIA && estado != ESTADO_EDITOR) {
                    estado = ESTADO_MENU_PRINCIPAL; delayInput = 30; ReproducirSonidoSelect(); tStartY = -1;
                 }
            }
            if (estado == ESTADO_MENU_PRINCIPAL && delayInput == 0) {
                for(int i=0; i<8; i++) {
                    if (VerificarToqueBoton(touchX, touchY, btnRects[i])) {
                         seleccion = i; ReproducirSonidoSelect();
                         if (i==0) estado = ESTADO_UPDATES; else if (i==1) { estado = ESTADO_AGRADECIMIENTOS; scrollY=0; }
                         else if (i==2) { estado = ESTADO_SUBMENU_MODOS; seleccion=0; } else if (i==3) estado = ESTADO_GALERIA;
                         else if (i==4) estado = ESTADO_EDITOR; else if (i==5) { estado = ESTADO_MAS_OPCIONES; seleccion = 0; }
                         else if (i==6) esIngles = !esIngles; else if (i==7) { estado = ESTADO_AJUSTES; selAjustes = 0; } 
                         delayInput = 30; tStartY = -1; 
                    }
                }
            }
            if (estado == ESTADO_MAS_OPCIONES && delayInput == 0) {
                int startY = 300; 
                for(int i=0; i<4; i++) {
                    SDL_Rect rT = { (1280-600)/2, startY + (i*60), 600, 50 };
                    if (VerificarToqueBoton(touchX, touchY, rT)) { 
                        seleccion = i; ReproducirSonidoSelect(); 
                        if(i==0) { Mix_PauseMusic(); EjecutarWebcam(renderer, fuenteMini, esIngles); Mix_ResumeMusic(); }
                        else if(i==1) { Mix_PauseMusic(); EjecutarChroma(renderer, fuenteMini, ROOT_PATH); Mix_ResumeMusic(); } 
                        if (!WHBProcIsRunning()) { appRunning = false; break; }
                        delayInput = 30; tStartY = -1; 
                    }
                    if (i==0) { SDL_Rect rInfo = { ((1280-600)/2) + 610, startY + (i*60), 120, 50 }; if (VerificarToqueBoton(touchX, touchY, rInfo)) { ReproducirSonidoSelect(); estado = ESTADO_INFO_WEBCAM; delayInput = 30; } }
                    if (i==1) { SDL_Rect rInfo = { ((1280-600)/2) + 610, startY + (i*60), 120, 50 }; if (VerificarToqueBoton(touchX, touchY, rInfo)) { ReproducirSonidoSelect(); estado = ESTADO_INFO_CHROMA; scrollChroma = 0; delayInput = 30; } }
                }
                if (touchY > 600) { estado = ESTADO_MENU_PRINCIPAL; seleccion = 5; delayInput=30; }
            }
            if ((estado == ESTADO_INFO_WEBCAM) && delayInput == 0 && touchY > 600) { estado = ESTADO_MAS_OPCIONES; delayInput = 30; ReproducirSonidoSelect(); }
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
            
            DibujarTextoCentrado(renderer, fuenteMini, esIngles ? "Touchscreen now works!" : "El tactil ya funciona!", 240, colY);
            DibujarTextoCentrado(renderer, fuenteMini, esIngles ? "Remember to return here before closing the app" : "Recuerda volver a este menu antes de cerrar la aplicacion", 270, colY);
            
            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, APP_VERSION, "(D-Pad) Navigate", "(A) Select"); 
            else DibujarBarraInferiorGlobal(renderer, fuenteMini, APP_VERSION, "(Cruceta) Navegar", "(A) Seleccionar");
            
            SDL_Texture* bStart = esIngles ? texStart_EN : texStart_ES; SDL_Texture* bGal = esIngles ? texGal_EN : texGal_ES; 
            SDL_Texture* bEdit = esIngles ? texEdit_EN : texEdit_ES; SDL_Texture* bMore = esIngles ? texMore_EN : texMore_ES; 
            SDL_Texture* bLang = esIngles ? texLang_EN : texLang_ES;
            
            DibujarBoton(renderer, iconUpd, 30, 130, 80, 80, (seleccion==0), &btnRects[0]); 
            DibujarBoton(renderer, iconCred, 130, 130, 80, 80, (seleccion==1), &btnRects[1]);
            
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
                if (right) { if(seleccion==0) seleccion=1; else if(seleccion==2) seleccion=3; else if(seleccion==3) seleccion=4; else if(seleccion==5) seleccion=6; else if(seleccion==6) seleccion=7; delayInput=VEL_CURSOR; moved=true; }
                if (left) { if(seleccion==1) seleccion=0; else if(seleccion==3) seleccion=2; else if(seleccion==4) seleccion=3; else if(seleccion==6) seleccion=5; else if(seleccion==7) seleccion=6; delayInput=VEL_CURSOR; moved=true; }
                if (down) { if(seleccion<=1) seleccion=2; else if(seleccion>=2 && seleccion<=4) seleccion=5; else if(seleccion>=5 && seleccion<=6) seleccion=7; delayInput=VEL_CURSOR; moved=true; }
                if (up) { if(seleccion==7) seleccion=6; else if(seleccion>=5 && seleccion<=6) seleccion=3; else if(seleccion==2||seleccion==3) seleccion=0; else if(seleccion==4) seleccion=1; delayInput=VEL_CURSOR; moved=true; }
                if (moved) ReproducirSonidoMover();
                if (btnA) { ReproducirSonidoSelect();
                    if (seleccion==0) estado=ESTADO_UPDATES; else if (seleccion==1) { estado=ESTADO_AGRADECIMIENTOS; scrollY=0; }
                    else if (seleccion==2) { estado=ESTADO_SUBMENU_MODOS; seleccion=0; } else if (seleccion==3) estado=ESTADO_GALERIA; 
                    else if (seleccion==4) estado=ESTADO_EDITOR; else if (seleccion==5) { estado=ESTADO_MAS_OPCIONES; seleccion=0; }
                    else if (seleccion==6) esIngles=!esIngles; else if (seleccion==7) { estado=ESTADO_AJUSTES; selAjustes=0; } 
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
            const char* opMoreEN[] = { "Start Webcam (Beta)", "Chroma Key (Green Screen)", "Button 3", "Soon..." }; const char* opMoreES[] = { "Ejecutar Webcam (Beta)", "Chroma Key (Pantalla Verde)", "Boton 3", "Pronto..." };
            int startY = 300;
            for (int i = 0; i < 4; i++) { SDL_Color c = (i==seleccion) ? colY : colW; DibujarTextoCentrado(renderer, fuentePequena, esIngles ? opMoreEN[i] : opMoreES[i], startY + (i*60), c); if (i <= 1) { int idInfo = 10 + i; SDL_Rect rInfo = { ((1280-600)/2) + 610, startY + (i*60), 120, 50 }; SDL_Color cInfo = (seleccion == idInfo) ? colY : (SDL_Color){0,150,255,255}; SDL_SetRenderDrawColor(renderer, cInfo.r, cInfo.g, cInfo.b, cInfo.a); SDL_RenderFillRect(renderer, &rInfo); SDL_Surface* s=TTF_RenderText_Blended(fuenteMini, "[ ! ] Info", {255,255,255,255}); if(s){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s); SDL_Rect r={rInfo.x + (120-s->w)/2, rInfo.y + (50-s->h)/2, s->w, s->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s); SDL_DestroyTexture(t); } } }
            if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "(A) Select - (Right) Info", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "(A) Seleccionar - (Der) Info", "(B) Atras", "");
            if (delayInput == 0) { if (down) { if (seleccion >= 10) seleccion -= 9; else { seleccion++; if (seleccion>=4) seleccion=0; } delayInput=VEL_CURSOR; moved=true; } if (up) { if (seleccion >= 10) seleccion -= 10; else { seleccion--; if (seleccion<0) seleccion=3; } delayInput=VEL_CURSOR; moved=true; } if (right && (seleccion == 0 || seleccion == 1)) { seleccion += 10; delayInput=VEL_CURSOR; moved=true; } if (left && (seleccion == 10 || seleccion == 11)) { seleccion -= 10; delayInput=VEL_CURSOR; moved=true; } if (moved) ReproducirSonidoMover(); if (btnB) { estado = ESTADO_MENU_PRINCIPAL; seleccion=5; delayInput = 30; } 
            if (btnA) { ReproducirSonidoSelect(); if (seleccion == 10) { estado = ESTADO_INFO_WEBCAM; } else if (seleccion == 11) { estado = ESTADO_INFO_CHROMA; scrollChroma = 0; } else if (seleccion == 0) { Mix_PauseMusic(); EjecutarWebcam(renderer, fuenteMini, esIngles); Mix_ResumeMusic(); } else if (seleccion == 1) { Mix_PauseMusic(); EjecutarChroma(renderer, fuenteMini, ROOT_PATH); Mix_ResumeMusic(); } 
                if (!WHBProcIsRunning()) { appRunning = false; break; }
                delayInput = 30; } }
        } else if (estado == ESTADO_INFO_WEBCAM) {
             SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255); SDL_RenderClear(renderer); DibujarTextoCentrado(renderer, fuenteGrande, "Webcam Info", 60, colY);
             const char* t1 = esIngles ? "Clean Feed Mode: No UI, Full Screen." : "Modo Limpio: Sin interfaz, Pantalla Completa."; const char* t2 = esIngles ? "Use with Capture Card." : "Usalo con Capturadora."; const char* t3 = esIngles ? "Inspiration Video:" : "Video de Inspiracion:";
             DibujarTextoCentrado(renderer, fuenteMini, t1, 130, colW); DibujarTextoCentrado(renderer, fuenteMini, t2, 160, colW); DibujarTextoCentrado(renderer, fuentePequena, t3, 220, {0,255,255,255}); if (texQR) { SDL_Rect rQR = { (1280-300)/2, 270, 300, 300 }; SDL_RenderCopy(renderer, texQR, NULL, &rQR); }
             if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Atras", "");
             if (btnB && delayInput == 0) { estado = ESTADO_MAS_OPCIONES; seleccion=0; delayInput = 30; }
        } else if (estado == ESTADO_INFO_CHROMA) {
             SDL_SetRenderDrawColor(renderer, 20, 30, 20, 255); SDL_RenderClear(renderer); DibujarTextoCentrado(renderer, fuenteGrande, "Chroma Key Guide", 50, colY); SDL_Rect rClip = {0, 100, 1280, 560}; SDL_RenderSetClipRect(renderer, &rClip); int startY = 120 - (int)scrollChroma; SDL_Color cTit = {0, 255, 0, 255}; SDL_Color cTxt = colW; int spacing = 450; 
             DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "1. Green Screen Mode (Default)" : "1. Modo Pantalla Verde", startY, cTit); if (esIngles) { DibujarTextoCentrado(renderer, fuenteMini, "Use a green cloth background.", startY + 40, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "Wii U replaces it with the default or uploaded image.", startY + 70, cTxt); } else { DibujarTextoCentrado(renderer, fuenteMini, "Usa un estandarte de color verde.", startY + 40, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "La Wii U lo reemplazara con la imagen (Default/Subida).", startY + 70, cTxt); } if (texGuide1) { SDL_Rect rI = {(1280-500)/2, startY + 100, 500, 280}; SDL_RenderCopy(renderer, texGuide1, NULL, &rI); }
             int y2 = startY + spacing; DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "2. Magic Mode (No Green Screen)" : "2. Modo Magico (Sin Tela)", y2, cTit); if (esIngles) { DibujarTextoCentrado(renderer, fuenteMini, "No Green Screen? No problem.", y2 + 40, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "Keep GamePad still (on a table).", y2 + 70, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "Step out of frame. Press (A) to scan. Then step back in.", y2 + 100, cTxt); } else { DibujarTextoCentrado(renderer, fuenteMini, "¿Sin fondo verde? No hay problema.", y2 + 40, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "Manten el GamePad quieto (en una mesa).", y2 + 70, cTxt); DibujarTextoCentrado(renderer, fuenteMini, "Sal del cuadro. Presiona (A) para escanear. Vuelve a entrar.", y2 + 100, cTxt); } if (texGuide2) { SDL_Rect rI = {(1280-500)/2, y2 + 130, 500, 280}; SDL_RenderCopy(renderer, texGuide2, NULL, &rI); }
             int y3 = y2 + spacing + 30; DibujarTextoCentrado(renderer, fuentePequena, esIngles ? "3. Custom Background" : "3. Fondo Personalizado", y3, cTit); DibujarTextoCentrado(renderer, fuenteMini, esIngles ? "Press (+) to upload via Wi-Fi (Phone/PC)." : "Presiona (+) para subir por Wi-Fi (Celular/PC).", y3 + 40, cTxt); DibujarTextoCentrado(renderer, fuenteMini, esIngles ? "Scan the QR code and select your image." : "Escanea el QR y selecciona tu imagen.", y3 + 70, cTxt); SDL_RenderSetClipRect(renderer, NULL); 
             int totalH = spacing * 3 + 100; int viewH = 560; if (totalH > viewH) { float pct = scrollChroma / (float)(totalH - viewH); int barH = viewH * viewH / totalH; int barY = 100 + (int)(pct * (viewH - barH)); SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_Rect bgBar = {1260, 100, 10, viewH}; SDL_RenderFillRect(renderer, &bgBar); SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); SDL_Rect fgBar = {1260, barY, 10, barH}; SDL_RenderFillRect(renderer, &fgBar); }
             if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "(D-Pad) Scroll", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "(Cruceta) Desplazar", "(B) Atras", "");
             if (down) scrollChroma += 15; if (up) scrollChroma -= 15; int maxScroll = totalH - viewH; if (scrollChroma < 0) scrollChroma = 0; if (scrollChroma > maxScroll) scrollChroma = maxScroll; if (btnB && delayInput == 0) { estado = ESTADO_MAS_OPCIONES; seleccion=1; delayInput = 30; }
        
        } else if (estado == ESTADO_SUBMENU_MODOS) { 
            
            // --- DIBUJADO DE CABECERA (Más abajo para no tapar la lente) ---
            SDL_Texture* tHead = esIngles ? texHeadCam_EN : texHeadCam_ES;
            if (tHead) {
                int w, h; SDL_QueryTexture(tHead, NULL, NULL, &w, &h);
                SDL_Rect rHead = { (1280 - w) / 2, 230, w, h }; 
                SDL_RenderCopy(renderer, tHead, NULL, &rHead);
            } else {
                DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Camera Mode" : "Modo de Camara", 230, colW);
            }

            // --- NUEVO SISTEMA DE BOTONES EN CUADRÍCULA 2x2 ---
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
                DibujarTextoCentrado(renderer, fuentePequena, "v1.3.8 - Video + Audio Update!", y, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NEW: Native Video Recording WITH AUDIO!", y + gap*1.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NEW: Gallery now works as a full Media Player. ", y + gap*2.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- UI NEW: Beautiful new camera menu buttons", y + gap*3.5, colTxt);
            } else { 
                DibujarTextoCentrado(renderer, fuentePequena, "v1.3.8 - La actualizacion de Audio + Video", y, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NUEVO: Grabación de video nativo CON AUDIO!", y + gap*1.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- NUEVO: La Galería ahora es un reproductor multimedia", y + gap*2.5, colTxt);
                DibujarTextoCentrado(renderer, fuenteMini, "- UI: Nuevos botones en el modo cámara.", y + gap*3.5, colTxt);
            }
             if (esIngles) DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Back", ""); else DibujarBarraInferiorGlobal(renderer, fuenteMini, "", "(B) Atras", "");
             if (btnB && delayInput == 0) { estado = ESTADO_MENU_PRINCIPAL; delayInput = 30; }
        } else if (estado == ESTADO_AGRADECIMIENTOS) {
            DibujarTextoCentrado(renderer, fuenteGrande, esIngles ? "Credits" : "Creditos", 160, colY); SDL_Rect clip = { 200, 250, 880, 390 }; SDL_RenderSetClipRect(renderer, &clip);
            const char* linesEN[] = { "Created by: ClaudiWolf2056", "", "Special Thanks to:", "whateveritwas", "For providing part of his code", "for the exit logic of this app", "You can also find him on Github", "", "Wii U Community (Latam)", "- p-anthoX", "- JEAN_PRETENDO", "- Santix Aldama", "- Downyjarl", "- Keines", "", "Facebook Community:", "Da****, Ro****", "Ce***, and more <3", "", "Technical Help:", "ForTheUsers (4TU)", "My SD Card", "", "Libraries:", "SDL2 for Wii U - devkitPro", "", "See you soon! - ClaudiWolf2056" };
            const char* linesES[] = { "Creado por: ClaudiWolf2056", "", "Agradecimientos especiales a:", "whateveritwas", "Por brindar una parte de su codigo", "para la logica de cierre de esta app", "Tambien puedes encontrarlo en Github", "", "Comunidad Wii U (Latam)", "- p-anthoX", "- JEAN_PRETENDO", "- Santix Aldama", "- Downyjarl", "- Keines", "", "Comunidad de Facebook:", "Da****, Ro****", "Ce***, y muchos mas <3", "", "Ayuda Tecnica:", "ForTheUsers (4TU)", "Mi tarjeta SD xd", "", "Librerias:", "SDL2 (Wii U) - devkitPro", "", "Gracias por su interes en mi app", ":)", "", "Nos vemos pronto! - ClaudiWolf2056" };
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
        }
        
        if (dedo) DibujarCursorTactil(renderer, touchX, touchY);
        
        if(appRunning) SDL_RenderPresent(renderer);
    }
    
    
    if(texHeadCam_EN) SDL_DestroyTexture(texHeadCam_EN); if(texHeadCam_ES) SDL_DestroyTexture(texHeadCam_ES);
    if(texBtnPhoto_EN) SDL_DestroyTexture(texBtnPhoto_EN); if(texBtnPhoto_ES) SDL_DestroyTexture(texBtnPhoto_ES);
    if(texBtnVideo_EN) SDL_DestroyTexture(texBtnVideo_EN); if(texBtnVideo_ES) SDL_DestroyTexture(texBtnVideo_ES);
    if(texBtnFx_EN) SDL_DestroyTexture(texBtnFx_EN); if(texBtnFx_ES) SDL_DestroyTexture(texBtnFx_ES);
    if(texBtnMic_EN) SDL_DestroyTexture(texBtnMic_EN); if(texBtnMic_ES) SDL_DestroyTexture(texBtnMic_ES);

    CerrarAudio(); TTF_CloseFont(fuenteGrande); TTF_CloseFont(fuentePequena); TTF_CloseFont(fuenteMini);
    SDL_DestroyTexture(texBg); SDL_DestroyTexture(texStart_ES); SDL_DestroyTexture(texStart_EN); SDL_DestroyTexture(texGal_ES); SDL_DestroyTexture(texGal_EN);
    SDL_DestroyTexture(texEdit_ES); SDL_DestroyTexture(texEdit_EN); SDL_DestroyTexture(texLang_ES); SDL_DestroyTexture(texLang_EN); SDL_DestroyTexture(texMore_ES); SDL_DestroyTexture(texMore_EN);
    SDL_DestroyTexture(iconUpd); SDL_DestroyTexture(iconCred); SDL_DestroyTexture(iconSet); SDL_DestroyTexture(texQR);
    if(texGuide1) SDL_DestroyTexture(texGuide1); if(texGuide2) SDL_DestroyTexture(texGuide2);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    VPADShutdown(); IMG_Quit(); TTF_Quit(); SDL_Quit(); WHBLogConsoleFree(); WHBProcShutdown();
    return 0;
}