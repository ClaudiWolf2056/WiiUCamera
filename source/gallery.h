#ifndef GALLERY_H
#define GALLERY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h> 
#include <vpad/input.h>
#include <whb/proc.h> 
#include <vector>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdio>
#include <map>
#include <malloc.h>
#include "webserver.h"
#include "qrcodegen.hpp" 
using namespace qrcodegen;

const float GAL_ADC_MIN_X = 100.0f; const float GAL_ADC_MAX_X = 3950.0f;
const float GAL_ADC_MIN_Y = 100.0f; const float GAL_ADC_MAX_Y = 3900.0f;
const float GAL_APP_W = 1280.0f; const float GAL_APP_H = 720.0f;

enum TipoArchivo { TIPO_FOTO, TIPO_VIDEO };
struct FotoEntry { std::string nombre; std::string rutaCompleta; TipoArchivo tipo; };

int MapearGaleria(float val, float min, float max, float outMax, bool inv) {
    float pct = (val - min) / (max - min); if (pct < 0) pct = 0; if (pct > 1) pct = 1; if (inv) pct = 1.0f - pct; return (int)(pct * outMax);
}

void DibujarBtnG(SDL_Renderer* ren, int x, int y, int w, int h, SDL_Color c, const char* t, TTF_Font* f) {
    SDL_Rect r={x,y,w,h}; 
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a); SDL_RenderFillRect(ren, &r);
    SDL_SetRenderDrawColor(ren, 255,255,255,255); SDL_RenderDrawRect(ren, &r);
    if(t && f){ SDL_Surface* s=TTF_RenderText_Blended(f,t,{255,255,255,255}); if(s){ SDL_Texture* tx=SDL_CreateTextureFromSurface(ren,s); SDL_Rect rt={x+(w-s->w)/2,y+(h-s->h)/2,s->w,s->h}; SDL_RenderCopy(ren,tx,NULL,&rt); SDL_FreeSurface(s); SDL_DestroyTexture(tx); } }
}

void EscanearRecursivo(std::string ruta, std::vector<FotoEntry>& lista, int &outF, int &outV) {
    DIR* dir = opendir(ruta.c_str()); if (!dir) return;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string n = ent->d_name; if (n == "." || n == "..") continue;
        std::string fullPath = ruta + "/" + n; struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) { EscanearRecursivo(fullPath, lista, outF, outV); } 
            else {
                if (n.length() > 4) {
                    std::string ext = n.substr(n.length()-4); for(auto& c:ext) c=tolower(c);
                    FotoEntry f; f.nombre=n; f.rutaCompleta=fullPath; bool valido=false;
                    if(ext==".jpg"||ext==".png"||ext==".bmp"){ f.tipo=TIPO_FOTO; outF++; valido=true; }
                    else if(ext==".avi"){ f.tipo=TIPO_VIDEO; outV++; valido=true; } 
                    if(valido) lista.push_back(f);
                }
            }
        }
    }
    closedir(dir);
}

std::vector<FotoEntry> EscanearMedia(const std::string& directorio, int &outF, int &outV) {
    std::vector<FotoEntry> lista; outF=0; outV=0;
    EscanearRecursivo(directorio, lista, outF, outV);
    std::sort(lista.begin(), lista.end(), [](const FotoEntry& a, const FotoEntry& b){return a.nombre > b.nombre;});
    return lista;
}

void ReproducirVideoAVI(SDL_Renderer* renderer, TTF_Font* font, const std::string& ruta, bool esIngles) {
    FILE* f = fopen(ruta.c_str(), "rb"); if (!f) return;
    
    uint32_t vW = 320, vH = 240; long moviOffset = 0;
    uint8_t head[2048]; size_t hLen = fread(head, 1, 2048, f);
    for(size_t i=0; i<hLen-16; i++) {
        if(head[i]=='s'&&head[i+1]=='t'&&head[i+2]=='r'&&head[i+3]=='f' && head[i+4]==40 && head[i+5]==0){ 
            uint32_t w = head[i+12] | (head[i+13]<<8) | (head[i+14]<<16) | (head[i+15]<<24);
            uint32_t h = head[i+16] | (head[i+17]<<8) | (head[i+18]<<16) | (head[i+19]<<24);
            if (w > 0 && w <= 1280 && h > 0 && h <= 720) { vW = w; vH = h; }
        }
        if(head[i]=='m'&&head[i+1]=='o'&&head[i+2]=='v'&&head[i+3]=='i') { moviOffset = i + 4; }
    }
    if (moviOffset == 0) moviOffset = 500;
    
    bool isOldVideo = false;
    fseek(f, moviOffset, SEEK_SET); char pId[5] = {0}; uint32_t pSzLE = 0;
    if (fread(pId, 1, 4, f) == 4 && fread(&pSzLE, 1, 4, f) == 4) {
        uint32_t pSz = ((pSzLE >> 24) & 0xFF) | ((pSzLE >> 8) & 0xFF00) | ((pSzLE << 8) & 0xFF0000) | ((pSzLE << 24) & 0xFF000000);
        if (pSz > 500000) { isOldVideo = true; vW = 640; vH = 480; } 
    }
    
    fseek(f, moviOffset, SEEK_SET);
    uint32_t maxAudioBytes = 32000 * 2 * 60; 
    uint8_t* wavBuf = (uint8_t*)malloc(maxAudioBytes + 44);
    uint32_t audioSize = 0;
    
    if (wavBuf) {
        char cId[5] = {0}; uint32_t cSzLE = 0;
        while (fread(cId, 1, 4, f) == 4 && fread(&cSzLE, 1, 4, f) == 4) {
            uint32_t cSz = ((cSzLE >> 24) & 0xFF) | ((cSzLE >> 8) & 0xFF00) | ((cSzLE << 8) & 0xFF0000) | ((cSzLE << 24) & 0xFF000000);
            uint32_t rSz = cSz + (cSz % 2);
            if (strncmp(cId, "idx1", 4) == 0) break;
            if (strncmp(cId, "01wb", 4) == 0 && audioSize + cSz <= maxAudioBytes) {
                fread(wavBuf + 44 + audioSize, 1, cSz, f);
                audioSize += cSz;
                if (cSz % 2 != 0) fseek(f, 1, SEEK_CUR);
            } else {
                fseek(f, rSz, SEEK_CUR);
            }
        }
    }
    
    Mix_Chunk* audioChunk = NULL;
    if (audioSize > 0 && wavBuf) {
        auto W32 = [&](int o, uint32_t v) { wavBuf[o]=v&0xFF; wavBuf[o+1]=(v>>8)&0xFF; wavBuf[o+2]=(v>>16)&0xFF; wavBuf[o+3]=(v>>24)&0xFF; };
        auto W16 = [&](int o, uint16_t v) { wavBuf[o]=v&0xFF; wavBuf[o+1]=(v>>8)&0xFF; };
        memcpy(wavBuf, "RIFF", 4); W32(4, 36 + audioSize); memcpy(wavBuf+8, "WAVEfmt ", 8);
        W32(16, 16); W16(20, 1); W16(22, 1); W32(24, 32000); W32(28, 64000); W16(32, 2); W16(34, 16);
        memcpy(wavBuf+36, "data", 4); W32(40, audioSize);
        SDL_RWops* rw = SDL_RWFromMem(wavBuf, 44 + audioSize);
        audioChunk = Mix_LoadWAV_RW(rw, 1); 
    }
    if (wavBuf) free(wavBuf);

    fseek(f, moviOffset, SEEK_SET); 
    SDL_Texture* videoTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, vW, vH); 
    uint32_t maxFrameSize = vW * vH * 4; 
    uint8_t* rawBuf = (uint8_t*)memalign(256, maxFrameSize + 8192); 
    uint32_t* argbBuf = (uint32_t*)memalign(256, vW * vH * 4);
    
    if (!rawBuf || !argbBuf) {
        if(rawBuf) free(rawBuf); if(argbBuf) free(argbBuf);
        if(audioChunk) Mix_FreeChunk(audioChunk);
        SDL_DestroyTexture(videoTex); fclose(f); return;
    }

    bool reproduciendo = true; bool pausado = false; int delayVideo = 10; 
    uint32_t tickStart = SDL_GetTicks();
    uint32_t targetDelay = isOldVideo ? 33 : 50; 
    
    if (Mix_PlayingMusic()) Mix_PauseMusic();
    if (audioChunk) Mix_PlayChannel(1, audioChunk, 0); 
    
    while (reproduciendo && WHBProcIsRunning()) {
        SDL_Event event; while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) reproduciendo = false; }
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
        if (delayVideo > 0) delayVideo--;
        if (delayVideo == 0) {
            if (vpad.trigger & VPAD_BUTTON_B) reproduciendo = false; 
            if (vpad.trigger & VPAD_BUTTON_A) { 
                pausado = !pausado; 
                if (audioChunk) { if (pausado) Mix_Pause(1); else Mix_Resume(1); }
            }
        }
        
        if (!pausado) { 
            char chunkId[5] = {0}; uint32_t chunkSizeLE = 0;
            if (fread(chunkId, 1, 4, f) != 4) { fseek(f, moviOffset, SEEK_SET); if(audioChunk) Mix_PlayChannel(1, audioChunk, 0); continue; }
            fread(&chunkSizeLE, 1, 4, f);
            uint32_t chunkSize = ((chunkSizeLE >> 24) & 0xFF) | ((chunkSizeLE >> 8) & 0xFF00) | ((chunkSizeLE << 8) & 0xFF0000) | ((chunkSizeLE << 24) & 0xFF000000);
            uint32_t readSize = chunkSize + (chunkSize % 2); 
            
            if (strncmp(chunkId, "idx1", 4) == 0) { fseek(f, moviOffset, SEEK_SET); if(audioChunk) Mix_PlayChannel(1, audioChunk, 0); continue; }
            if (readSize > maxFrameSize + 8192) { fseek(f, readSize, SEEK_CUR); continue; } 
            
            fread(rawBuf, 1, readSize, f);
            
            if (strncmp(chunkId, "00db", 4) == 0 || strncmp(chunkId, "00dc", 4) == 0) {
                if (isOldVideo) {
                    for(int i=0; i<vW*vH; i++) {
                        uint32_t p = ((uint32_t*)rawBuf)[i];
                        uint32_t b = (p >> 24) & 0xFF; uint32_t g = (p >> 16) & 0xFF; uint32_t r = (p >> 8) & 0xFF; 
                        argbBuf[i] = (0xFF << 24) | (b << 16) | (g << 8) | r; 
                    }
                } else {
                    for(int i=0; i<vW*vH; i++) {
                        uint8_t b = rawBuf[i*3]; uint8_t g = rawBuf[i*3+1]; uint8_t r = rawBuf[i*3+2];
                        argbBuf[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                    }
                }
                SDL_UpdateTexture(videoTex, NULL, argbBuf, vW * 4); 
                
                uint32_t now = SDL_GetTicks();
                if (now - tickStart < targetDelay) SDL_Delay(targetDelay - (now - tickStart)); 
                tickStart = SDL_GetTicks();
            } 
        }
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
        SDL_Rect rVid = { (1280 - 960)/2, (720 - 720)/2, 960, 720 }; 
        SDL_RenderCopyEx(renderer, videoTex, NULL, &rVid, 0, NULL, SDL_FLIP_VERTICAL);
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180); SDL_Rect rBar = {0, 660, 1280, 60}; SDL_RenderFillRect(renderer, &rBar);
        DibujarBtnG(renderer, 50, 665, 300, 50, {0,150,0,255}, pausado ? (esIngles?"(A) Play":"(A) Reprod") : (esIngles?"(A) Pause":"(A) Pausa"), font);
        DibujarBtnG(renderer, 1050, 665, 180, 50, {150,50,50,255}, "(B) Exit", font);
        SDL_RenderPresent(renderer); 
    }
    
    Mix_HaltChannel(1);
    if (audioChunk) Mix_FreeChunk(audioChunk);
    free(rawBuf); free(argbBuf); SDL_DestroyTexture(videoTex); fclose(f);
    if (Mix_PausedMusic()) Mix_ResumeMusic(); 
}

int EjecutarGaleria(SDL_Renderer* renderer, TTF_Font* font, bool esIngles, int tipoGaleria, Mix_Music* musicPtr = NULL, bool musicEnabled = true) {
    std::string ruta = (tipoGaleria == 1) ? "fs:/vol/external01/wiiu/screenshots" : "fs:/vol/external01/WiiUCamera Files";
    int cP=0, cV=0; std::vector<FotoEntry> fotos = EscanearMedia(ruta, cP, cV);
    const int COLS=4, CELL_W=280, CELL_H=210, GAP=20, MARGIN_X=50, MARGIN_TOP=80;
    int sel=0; float scroll=0; bool salir=false; bool verFoto=false; bool borrar=false; bool share=false;
    std::string msg=""; int tmr=0; float zoom=1.0f; float px=0, py=0; int delay=0;
    std::map<int, SDL_Texture*> cache;
    
    Uint32 musicGapTimer = 0;

    while(!salir && WHBProcIsRunning()) {
        VPADStatus vpad; VPADReadError e; VPADRead(VPAD_CHAN_0, &vpad, 1, &e);
        SDL_Event ev; while(SDL_PollEvent(&ev)){ if(ev.type==SDL_QUIT) return -1; }
        if(delay>0) delay--; if(tmr>0) tmr--; else msg="";

        if (musicEnabled && musicPtr) {
            if (!Mix_PlayingMusic()) {
                if (musicGapTimer == 0) musicGapTimer = SDL_GetTicks(); 
                if (SDL_GetTicks() - musicGapTimer > 200) { Mix_PlayMusic(musicPtr, 0); musicGapTimer = 0; }
            } else { musicGapTimer = 0; }
        }

        if (borrar) {
             if (delay == 0) {
                 if (vpad.trigger & VPAD_BUTTON_B) { borrar = false; delay = 20; }
                 if (vpad.trigger & VPAD_BUTTON_A) {
                     remove(fotos[sel].rutaCompleta.c_str()); 
                     if(cache.count(sel)){ SDL_DestroyTexture(cache[sel]); cache.erase(sel); } 
                     if(fotos[sel].tipo==TIPO_FOTO) cP--; else cV--; 
                     fotos.erase(fotos.begin()+sel); if(sel>=fotos.size()&&sel>0) sel--; 
                     for(auto const& [k,v]:cache) SDL_DestroyTexture(v); cache.clear(); 
                     borrar=false; delay=20; 
                 }
             }
        }
        else if(share) { 
            if(!serverRunning) { share=false; msg=serverStatusMsg; tmr=120; }
            else if(sel < fotos.size()) AtenderClientes(fotos[sel].rutaCompleta);
            if(delay==0 && ((vpad.trigger & VPAD_BUTTON_B) || vpad.tpNormal.touched)) { share=false; DetenerServidor(); delay=30; }
            SDL_SetRenderDrawColor(renderer, 20,20,40,255); SDL_RenderClear(renderer);
            DibujarBtnG(renderer, 340, 30, 600, 50, {0,100,200,255}, esIngles?"Mobile Transfer":"Transferencia Movil", font);
            char ip[100]; sprintf(ip, "http://%s:8080", myIPAddress.c_str());
            SDL_Surface* sIP=TTF_RenderText_Blended(font, ip, {255,255,0,255}); if(sIP){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sIP); SDL_Rect r={(1280-sIP->w)/2, 530, sIP->w, sIP->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sIP); SDL_DestroyTexture(t); }
            QrCode qr = QrCode::encodeText(ip, QrCode::Ecc::LOW); int sz=qr.getSize(); int sc=300/sz; int fs=sz*sc; int sx=(1280-fs)/2; int sy=150;
            SDL_Rect bg={sx-10,sy-10,fs+20,fs+20}; SDL_SetRenderDrawColor(renderer,255,255,255,255); SDL_RenderFillRect(renderer,&bg); SDL_SetRenderDrawColor(renderer,0,0,0,255);
            for(int y=0;y<sz;y++) for(int x=0;x<sz;x++) if(qr.getModule(x,y)){ SDL_Rect m={sx+x*sc,sy+y*sc,sc,sc}; SDL_RenderFillRect(renderer,&m); }
            DibujarBtnG(renderer, 540, 650, 200, 50, {150,0,0,255}, "(B) Stop", font);
            SDL_RenderPresent(renderer); continue;
        }
        else if(verFoto) { 
            if(delay==0) {
                if((vpad.trigger & VPAD_BUTTON_B)) { verFoto=false; delay=20; zoom=1.0f; px=0; py=0; }
                if(vpad.rightStick.y>0.2) zoom+=0.05f; if(vpad.rightStick.y<-0.2) zoom-=0.05f; if(zoom<0.5f) zoom=0.5f; if(zoom>5.0f) zoom=5.0f;
                float ms=10.0f*zoom; 
                if((vpad.hold&VPAD_BUTTON_RIGHT)||vpad.leftStick.x>0.5) px-=ms; if((vpad.hold&VPAD_BUTTON_LEFT)||vpad.leftStick.x<-0.5) px+=ms;
                if((vpad.hold&VPAD_BUTTON_UP)||vpad.leftStick.y>0.5) py+=ms; if((vpad.hold&VPAD_BUTTON_DOWN)||vpad.leftStick.y<-0.5) py-=ms;
                // Botón Y para compartir fotos o videos desde el visor individual
                if((vpad.trigger & VPAD_BUTTON_Y)) { SDL_SetRenderDrawColor(renderer,0,0,0,200); SDL_RenderClear(renderer); SDL_RenderPresent(renderer); if(IniciarServidor()) share=true; else { msg=serverStatusMsg; tmr=120; } delay=30; }
            }
            SDL_SetRenderDrawColor(renderer,0,0,0,255); SDL_RenderClear(renderer);
            SDL_Texture* t=NULL; if(cache.find(sel)!=cache.end()) t=cache[sel]; else { SDL_Surface* s=IMG_Load(fotos[sel].rutaCompleta.c_str()); if(s){ t=SDL_CreateTextureFromSurface(renderer,s); SDL_FreeSurface(s); cache[sel]=t; } }
            if(t) { int w,h; SDL_QueryTexture(t,NULL,NULL,&w,&h); SDL_Rect rq={(int)((1280-w*zoom)/2+px), (int)((720-h*zoom)/2+py), (int)(w*zoom), (int)(h*zoom)}; SDL_RenderCopy(renderer,t,NULL,&rq); }
            SDL_SetRenderDrawColor(renderer,0,0,0,180); SDL_Rect bar={0,660,1280,60}; SDL_RenderFillRect(renderer,&bar);
            DibujarBtnG(renderer, 450, 665, 300, 50, {0,150,0,255}, esIngles?"(Joysticks L/R) Move/Zoom":"(Joysticks L/R) Mover/Zoom", font);
            DibujarBtnG(renderer, 250, 665, 180, 50, {0,100,200,255}, esIngles?"(Y) Share":"(Y) Compartir", font);
            DibujarBtnG(renderer, 1100, 665, 150, 50, {150,50,50,255}, "(B) Close", font);
            SDL_RenderPresent(renderer); continue;
        }
        else { 
            int tot=fotos.size();
            if (delay == 0 && !borrar) {
                if ((vpad.trigger & VPAD_BUTTON_B)) salir=true; 
                if (tot > 0) {
                    if(((vpad.hold&VPAD_BUTTON_RIGHT)||vpad.leftStick.x>0.5) && sel<tot-1){ sel++; delay=8; } 
                    if(((vpad.hold&VPAD_BUTTON_LEFT)||vpad.leftStick.x<-0.5) && sel>0){ sel--; delay=8; }
                    if(((vpad.hold&VPAD_BUTTON_DOWN)||vpad.leftStick.y<-0.5) && sel+COLS<tot){ sel+=COLS; delay=8; } 
                    if(((vpad.hold&VPAD_BUTTON_UP)||vpad.leftStick.y>0.5) && sel-COLS>=0){ sel-=COLS; delay=8; }
                    if((vpad.trigger & VPAD_BUTTON_A)) { if(fotos[sel].tipo == TIPO_VIDEO) { ReproducirVideoAVI(renderer, font, fotos[sel].rutaCompleta, esIngles); delay=20; } else { verFoto=true; delay=20; } }
                    
                    // LÓGICA CORREGIDA: Habilitado para Compartir Videos
                    if((vpad.trigger & VPAD_BUTTON_Y)) { SDL_SetRenderDrawColor(renderer,0,0,0,200); SDL_RenderClear(renderer); SDL_RenderPresent(renderer); if(IniciarServidor()) share=true; else { msg=serverStatusMsg; tmr=120; } delay=30; }
                    
                    if((vpad.trigger & VPAD_BUTTON_X)) { borrar=true; delay=20; }
                    if(vpad.tpNormal.touched) {
                        int tx=MapearGaleria(vpad.tpNormal.x, GAL_ADC_MIN_X, GAL_ADC_MAX_X, GAL_APP_W, false);
                        int ty=MapearGaleria(vpad.tpNormal.y, GAL_ADC_MIN_Y, GAL_ADC_MAX_Y, GAL_APP_H, true);
                        if(ty>620) {
                            if(tx>50 && tx<230){ if(fotos[sel].tipo == TIPO_VIDEO) ReproducirVideoAVI(renderer, font, fotos[sel].rutaCompleta, esIngles); else verFoto=true; delay=20; }
                            
                            // LÓGICA CORREGIDA TÁCTIL: Botón Share activado para todos
                            if(tx>260 && tx<440){ if(IniciarServidor()) share=true; delay=30; }
                            
                            if(tx>470 && tx<650){ borrar=true; delay=20; }
                            if(tx>1050 && tx<1230){ salir=true; delay=20; }
                        } else { float gy=ty+scroll-MARGIN_TOP; if(gy>=0 && tx>=MARGIN_X){ int c=(tx-MARGIN_X)/(CELL_W+GAP); int rw=(int)(gy/(CELL_H+GAP)); if(c>=0 && c<COLS){ int i=rw*COLS+c; if(i>=0 && i<tot) sel=i; } } }
                    }
                } else {
                    if(vpad.tpNormal.touched) { int ty=MapearGaleria(vpad.tpNormal.y, GAL_ADC_MIN_Y, GAL_ADC_MAX_Y, GAL_APP_H, true); int tx=MapearGaleria(vpad.tpNormal.x, GAL_ADC_MIN_X, GAL_ADC_MAX_X, GAL_APP_W, false); if(ty > 620 && tx > 1050) { salir=true; delay=20; } }
                }
            }
        }

        int tot=fotos.size();
        if(tot>0) { int rs=sel/COLS; int yt=MARGIN_TOP+rs*(CELL_H+GAP); int yb=yt+CELL_H; if(yt<scroll+MARGIN_TOP) scroll=yt-MARGIN_TOP; if(yb>scroll+600) scroll=yb-600; if(scroll<0) scroll=0; }
        int fvr=scroll/(CELL_H+GAP); int lvr=(scroll+720)/(CELL_H+GAP)+1; int minI=fvr*COLS; int maxI=(lvr+1)*COLS;
        for(auto it=cache.begin(); it!=cache.end(); ) { if(it->first < minI-COLS || it->first > maxI+COLS) { SDL_DestroyTexture(it->second); it=cache.erase(it); } else ++it; }
        for(int i=minI; i<maxI; i++) { if(i>=0 && i<tot) { if(fotos[i].tipo==TIPO_FOTO && cache.find(i)==cache.end()) { SDL_Surface* s=IMG_Load(fotos[i].rutaCompleta.c_str()); if(s){ cache[i]=SDL_CreateTextureFromSurface(renderer,s); SDL_FreeSurface(s); } } } }

        SDL_SetRenderDrawColor(renderer,20,20,25,255); SDL_RenderClear(renderer);
        for(int i=minI; i<maxI; i++) {
            if(i>=tot) break; int c=i%COLS; int r=i/COLS; int x=MARGIN_X+c*(CELL_W+GAP); int y=MARGIN_TOP+r*(CELL_H+GAP)-(int)scroll; if(y+CELL_H<0 || y>720) continue;
            SDL_Rect cr={x,y,CELL_W,CELL_H}; SDL_SetRenderDrawColor(renderer,40,40,40,255); SDL_RenderFillRect(renderer,&cr);
            if(fotos[i].tipo==TIPO_FOTO) { if(cache.count(i)) { SDL_Rect pr={x+5,y+5,CELL_W-10,CELL_H-10}; SDL_RenderCopy(renderer,cache[i],NULL,&pr); } }
            else { SDL_SetRenderDrawColor(renderer,80,0,120,255); SDL_Rect pr={x+10,y+10,CELL_W-20,CELL_H-20}; SDL_RenderFillRect(renderer,&pr); SDL_SetRenderDrawColor(renderer,255,255,255,255); SDL_Point p[4] = {{x+CELL_W/2-10, y+CELL_H/2-15}, {x+CELL_W/2-10, y+CELL_H/2+15}, {x+CELL_W/2+15, y+CELL_H/2}, {x+CELL_W/2-10, y+CELL_H/2-15}}; SDL_RenderDrawLines(renderer, p, 4); }
            if(i==sel) { SDL_SetRenderDrawColor(renderer,255,230,0,255); SDL_Rect b1={x-4,y-4,CELL_W+8,CELL_H+8}; SDL_RenderDrawRect(renderer,&b1); }
        }
        
        SDL_SetRenderDrawColor(renderer,30,30,35,255); SDL_Rect rh={0,0,1280,70}; SDL_RenderFillRect(renderer,&rh);
        char buf[128]; if (tipoGaleria == 1) snprintf(buf, sizeof(buf), esIngles ? "Screenshots (%d Images)" : "Screenshots (%d Imagenes)", cP); else snprintf(buf, sizeof(buf), esIngles ? "Gallery (%d Photos, %d Videos)" : "Galeria (%d Fotos, %d Videos)", cP, cV);
        SDL_Surface* st=TTF_RenderText_Blended(font,buf,{255,255,255,255}); if(st){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,st); SDL_Rect rt={50,20,st->w,st->h}; SDL_RenderCopy(renderer,t,NULL,&rt); SDL_FreeSurface(st); SDL_DestroyTexture(t); }
        SDL_Rect rb={0,620,1280,100}; SDL_SetRenderDrawColor(renderer,0,0,0,220); SDL_RenderFillRect(renderer,&rb);
        
        if(tot == 0) { const char* msgEmpty = esIngles ? "No images found." : "No se encontraron imagenes."; SDL_Surface* sem=TTF_RenderText_Blended(font,msgEmpty,{150,150,150,255}); if(sem){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,sem); SDL_Rect r={(1280-sem->w)/2,360,sem->w,sem->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sem); SDL_DestroyTexture(t); } }

        // INTERFAZ DE BOTONES CORREGIDA
        if(fotos.size() > 0 && fotos[sel].tipo == TIPO_VIDEO) { 
            DibujarBtnG(renderer, 50, 640, 180, 50, {0,150,0,255}, esIngles?"(A) Play":"(A) Ver", font); 
            DibujarBtnG(renderer, 260, 640, 180, 50, {0,100,200,255}, esIngles?"(Y) Share":"(Y) Compartir", font); 
            DibujarBtnG(renderer, 470, 640, 180, 50, {150,0,0,255}, esIngles?"(X) Delete":"(X) Borrar", font); 
        } 
        else if (fotos.size() > 0) { 
            DibujarBtnG(renderer, 50, 640, 180, 50, {0,150,0,255}, esIngles?"(A) View":"(A) Ver", font); 
            DibujarBtnG(renderer, 260, 640, 180, 50, {0,100,200,255}, esIngles?"(Y) Share":"(Y) Compartir", font); 
            DibujarBtnG(renderer, 470, 640, 180, 50, {150,0,0,255}, esIngles?"(X) Delete":"(X) Borrar", font); 
        } 
        else { DibujarBtnG(renderer, 50, 640, 180, 50, {50,50,50,255}, "-", font); DibujarBtnG(renderer, 260, 640, 180, 50, {50,50,50,255}, "-", font); DibujarBtnG(renderer, 470, 640, 180, 50, {50,50,50,255}, "-", font); }
        
        DibujarBtnG(renderer, 1050, 640, 180, 50, {100,100,100,255}, esIngles?"(B) Back":"(B) Volver", font);
        if(tmr>0 && msg.length()>0) { SDL_SetRenderDrawColor(renderer,200,0,0,255); SDL_Rect rm={400,300,480,60}; SDL_RenderFillRect(renderer,&rm); SDL_Surface* sm=TTF_RenderText_Blended(font,msg.c_str(),{255,255,255,255}); if(sm){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer, sm); SDL_Rect r={420,310,sm->w,sm->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(sm); SDL_DestroyTexture(t); } }

        if(borrar) {
             SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); SDL_Rect rBg = {0,0,1280,720}; SDL_RenderFillRect(renderer, &rBg); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
             SDL_Rect rBox = {340, 260, 600, 200}; SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255); SDL_RenderFillRect(renderer, &rBox); SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &rBox);
             const char* txt1 = esIngles ? "Delete this file?" : "Borrar este archivo?"; const char* txt2 = esIngles ? "(A) YES    (B) NO" : "(A) SI    (B) NO";
             SDL_Surface* s1 = TTF_RenderText_Blended(font, txt1, {255,255,255,255}); if(s1){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s1); SDL_Rect r={(1280-s1->w)/2, 300, s1->w, s1->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s1); SDL_DestroyTexture(t); }
             SDL_Surface* s2 = TTF_RenderText_Blended(font, txt2, {255,255,0,255}); if(s2){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s2); SDL_Rect r={(1280-s2->w)/2, 380, s2->w, s2->h}; SDL_RenderCopy(renderer,t,NULL,&r); SDL_FreeSurface(s2); SDL_DestroyTexture(t); }
        }
        SDL_RenderPresent(renderer);
    }
    for(auto const& [k,v]:cache) SDL_DestroyTexture(v); return 0;
}
#endif