#ifndef MIC_TEST_H
#define MIC_TEST_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <mic/mic.h>
#include <vpad/input.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <algorithm>
#include <dirent.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/cache.h>

// =============================================================================
// MOTOR DE AUDIO (3 MINIONS) - LOGICA INTACTA
// =============================================================================

#define SAMPLE_RATE 32000    
#define HW_BUFFER_SIZE 0x10000 
#define MAX_REC_SECONDS 60     
#define REC_BUFFER_SIZE (SAMPLE_RATE * 2 * MAX_REC_SECONDS) 

static MICHandle g_hMic = 0;
static void* g_hwBuffer = NULL;     
static MICWorkMemory g_workMem;
static bool g_micReady = false;

static uint8_t* g_recBuffer = NULL; 
static volatile uint32_t g_recWritePos = 0; 
static volatile bool g_isRecording = false;

static OSThread g_audioThread;
static uint8_t g_threadStack[0x4000]; 
static volatile bool g_threadRunning = false;
static volatile uint32_t g_lastHwPos = 0;

static char g_systemMessage[128] = ""; 
static volatile float g_visualVolume = 0.0f; 

uint16_t Swap16(uint16_t val) { return (val << 8) | (val >> 8); }
uint32_t Swap32(uint32_t val) {
    return ((val << 24) & 0xFF000000) | ((val <<  8) & 0x00FF0000) |
           ((val >>  8) & 0x0000FF00) | ((val >> 24) & 0x000000FF);
}

struct WAVHeader {
    char riff[4] = {'R','I','F','F'}; uint32_t overallSize; char wave[4] = {'W','A','V','E'};
    char fmt[4] = {'f','m','t',' '}; uint32_t fmtLength; uint16_t audioFormat; uint16_t numChannels;
    uint32_t sampleRate; uint32_t byteRate; uint16_t blockAlign; uint16_t bitsPerSample;
    char data[4] = {'d','a','t','a'}; uint32_t dataSize;
};

// MINION 3: GUARDAR
bool GuardarWAV_Minion3() {
    if (!g_recBuffer || g_recWritePos == 0) {
        sprintf(g_systemMessage, "Error: Audio vacio.");
        return false;
    }
    char filePath[256];
    const char* folderPath = "/vol/external01/WiiUCamera Files";
    struct stat st = {0};
    if (stat(folderPath, &st) == -1) mkdir(folderPath, 0777);

    int id = 1;
    while(true) {
        sprintf(filePath, "%s/Audio_%02d.wav", folderPath, id);
        FILE* check = fopen(filePath, "rb");
        if (!check) break; 
        fclose(check);
        id++;
    }
    FILE* f = fopen(filePath, "wb");
    if (!f) { sprintf(g_systemMessage, "Error SD."); return false; }

    int16_t* samples = (int16_t*)g_recBuffer;
    size_t sampleCount = g_recWritePos / 2;
    for (size_t i = 0; i < sampleCount; i++) samples[i] = Swap16(samples[i]);

    WAVHeader header;
    header.fmtLength = Swap32(16); header.audioFormat = Swap16(1); header.numChannels = Swap16(1);
    header.sampleRate = Swap32(SAMPLE_RATE); header.byteRate = Swap32(SAMPLE_RATE * 2);
    header.blockAlign = Swap16(2); header.bitsPerSample = Swap16(16);
    header.dataSize = Swap32(g_recWritePos); header.overallSize = Swap32(g_recWritePos + 36);

    fwrite(&header, sizeof(WAVHeader), 1, f);
    fwrite(g_recBuffer, 1, g_recWritePos, f);
    fflush(f); fsync(fileno(f)); fclose(f);

    sprintf(g_systemMessage, "Guardado exitoso: Audio_%02d.wav", id);
    return true;
}

// MINION 2: COLECCIONISTA
int Minion2_Thread(int argc, const char **argv) {
    while (g_threadRunning) {
        if (g_micReady) {
            MICStatus status;
            MICGetStatus(g_hMic, &status);
            
            int16_t* s = (int16_t*)g_hwBuffer;
            int idx = (status.bufferPos/2) - 1; if(idx<0) idx=0;
            float v = abs(s[idx]) / 5000.0f;
            g_visualVolume += (v - g_visualVolume) * 0.25f;

            if (g_isRecording) {
                DCInvalidateRange(g_hwBuffer, HW_BUFFER_SIZE);
                uint32_t currentHwPos = status.bufferPos;
                int bytesAvailable = 0;
                
                if (currentHwPos >= g_lastHwPos) bytesAvailable = currentHwPos - g_lastHwPos;
                else bytesAvailable = (HW_BUFFER_SIZE - g_lastHwPos) + currentHwPos;

                if (bytesAvailable > 0) {
                    uint8_t* src = (uint8_t*)g_hwBuffer;
                    if (g_recWritePos + bytesAvailable < REC_BUFFER_SIZE) {
                        if (currentHwPos >= g_lastHwPos) {
                            memcpy(&g_recBuffer[g_recWritePos], &src[g_lastHwPos], bytesAvailable);
                        } else {
                            int part1 = HW_BUFFER_SIZE - g_lastHwPos;
                            int part2 = currentHwPos;
                            memcpy(&g_recBuffer[g_recWritePos], &src[g_lastHwPos], part1);
                            memcpy(&g_recBuffer[g_recWritePos + part1], &src[0], part2);
                        }
                        g_recWritePos += bytesAvailable;
                        g_lastHwPos = currentHwPos;
                    } else {
                        g_isRecording = false; 
                    }
                }
            } else {
                g_lastHwPos = status.bufferPos;
            }
        }
        OSSleepTicks(OSMillisecondsToTicks(3));
    }
    return 0;
}

bool IniciarMicrofono() {
    if (g_micReady) return true;
    g_hwBuffer = memalign(0x40, HW_BUFFER_SIZE); 
    if (!g_hwBuffer) return false;
    memset(g_hwBuffer, 0, HW_BUFFER_SIZE);
    g_workMem.sampleMaxCount = HW_BUFFER_SIZE / 2; g_workMem.sampleBuffer = g_hwBuffer;
    MICError err;
    g_hMic = MICInit(MIC_INSTANCE_0, 0, &g_workMem, &err);
    if (g_hMic < 0 || err != MIC_ERROR_OK) { free(g_hwBuffer); return false; }
    if (MICOpen(g_hMic) != MIC_ERROR_OK) { MICUninit(g_hMic); free(g_hwBuffer); return false; }
    g_micReady = true;
    g_threadRunning = true;
    OSCreateThread(&g_audioThread, Minion2_Thread, 0, NULL, g_threadStack + sizeof(g_threadStack), sizeof(g_threadStack), 10, OS_THREAD_ATTRIB_AFFINITY_CPU1);
    OSResumeThread(&g_audioThread);
    return true;
}

void CerrarMicrofono() {
    g_threadRunning = false; OSJoinThread(&g_audioThread, NULL); 
    if (g_micReady) { MICClose(g_hMic); MICUninit(g_hMic); free(g_hwBuffer); g_micReady = false; }
}

// =============================================================================
// GALERIA Y REPRODUCTOR
// =============================================================================

std::vector<std::string> g_wavFiles;
int g_selectedFileIndex = 0;
Mix_Music* g_currentMusic = NULL;
bool g_isMusicPlaying = false;

void EscanearArchivosWAV() {
    g_wavFiles.clear();
    DIR* dir;
    struct dirent* ent;
    // Escanear la carpeta de grabaciones
    if ((dir = opendir("/vol/external01/WiiUCamera Files")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name.length() > 4 && name.substr(name.length() - 4) == ".wav") {
                g_wavFiles.push_back(name);
            }
        }
        closedir(dir);
    }
    std::sort(g_wavFiles.begin(), g_wavFiles.end());
    if (g_selectedFileIndex >= (int)g_wavFiles.size()) g_selectedFileIndex = 0;
}

void ReproducirWAV(const char* filename) {
    if (g_currentMusic) { Mix_FreeMusic(g_currentMusic); g_currentMusic = NULL; }
    
    char fullPath[256];
    sprintf(fullPath, "/vol/external01/WiiUCamera Files/%s", filename);
    
    g_currentMusic = Mix_LoadMUS(fullPath);
    if (g_currentMusic) {
        Mix_PlayMusic(g_currentMusic, 0); 
        g_isMusicPlaying = true;
        sprintf(g_systemMessage, "Reproduciendo: %s", filename);
    } else {
        sprintf(g_systemMessage, "Error al cargar archivo.");
    }
}

void DetenerReproduccion() {
    if (g_isMusicPlaying) {
        Mix_HaltMusic();
        g_isMusicPlaying = false;
        if (g_currentMusic) { Mix_FreeMusic(g_currentMusic); g_currentMusic = NULL; }
        sprintf(g_systemMessage, "Reproduccion detenida.");
    }
}

// =============================================================================
// UI MEJORADA
// =============================================================================

void DibujarUI(SDL_Renderer* ren, TTF_Font* font, TTF_Font* fontSmall) {
    // Fondo oscuro elegante
    SDL_SetRenderDrawColor(ren, 18, 18, 24, 255); SDL_RenderClear(ren);

    // --- TITULO Y DESCRIPCION ---
    SDL_Surface* sT = TTF_RenderText_Blended(font, "Grabador de Audio (Test)", {255, 200, 0, 255});
    SDL_Texture* tT = SDL_CreateTextureFromSurface(ren, sT);
    SDL_Rect rT = {(1280-sT->w)/2, 30, sT->w, sT->h};
    SDL_RenderCopy(ren, tT, NULL, &rT);
    SDL_FreeSurface(sT); SDL_DestroyTexture(tT);

    SDL_Surface* sD = TTF_RenderText_Blended(fontSmall, "Este es un espacio de pruebas para el microfono de Wii U, puedes grabar audios aqui.", {200, 200, 200, 255});
    SDL_Texture* tD = SDL_CreateTextureFromSurface(ren, sD);
    SDL_Rect rD = {(1280-sD->w)/2, 80, sD->w, sD->h};
    SDL_RenderCopy(ren, tD, NULL, &rD);
    SDL_FreeSurface(sD); SDL_DestroyTexture(tD);

    SDL_Surface* sR = TTF_RenderText_Blended(fontSmall, "Carpeta de destino: WiiUCamera Files", {150, 150, 150, 255});
    SDL_Texture* tR = SDL_CreateTextureFromSurface(ren, sR);
    SDL_Rect rR = {(1280-sR->w)/2, 110, sR->w, sR->h};
    SDL_RenderCopy(ren, tR, NULL, &rR);
    SDL_FreeSurface(sR); SDL_DestroyTexture(tR);

    // --- BARRA DE VOLUMEN (Coherente) ---
    // Marco
    SDL_Rect barBg = {340, 180, 600, 60};
    SDL_SetRenderDrawColor(ren, 30, 30, 40, 255); SDL_RenderFillRect(ren, &barBg);
    SDL_SetRenderDrawColor(ren, 100, 100, 100, 255); SDL_RenderDrawRect(ren, &barBg);

    // Relleno Dinamico
    int w = (int)(596 * g_visualVolume); 
    if(w > 596) w = 596;
    SDL_Rect fill = {342, 182, w, 56};
    
    // Verde (bajo) -> Amarillo (medio) -> Rojo (alto/grabando)
    Uint8 r = (g_visualVolume > 0.5f) ? 255 : (Uint8)(g_visualVolume * 510);
    Uint8 g = (g_visualVolume > 0.5f) ? (Uint8)((1.0f - g_visualVolume) * 510) : 255;
    if(g_isRecording) { r=255; g=0; } 

    SDL_SetRenderDrawColor(ren, r, g, 0, 255);
    SDL_RenderFillRect(ren, &fill);

    // --- INSTRUCCIONES DE CONTROL ---
    const char* instr = g_isRecording 
        ? "<< GRABANDO EN PROGRESO >> Presiona (B) para Terminar" 
        : "Presiona (A) dos veces para GRABAR  |  (B) Salir";
    
    SDL_Color cInstr = g_isRecording ? (SDL_Color){255, 100, 100, 255} : (SDL_Color){255, 255, 255, 255};
    SDL_Surface* sI = TTF_RenderText_Blended(font, instr, cInstr);
    SDL_Texture* tI = SDL_CreateTextureFromSurface(ren, sI);
    SDL_Rect rI = {(1280-sI->w)/2, 260, sI->w, sI->h};
    SDL_RenderCopy(ren, tI, NULL, &rI);
    SDL_FreeSurface(sI); SDL_DestroyTexture(tI);

    // --- MENSAJES DEL SISTEMA ---
    if (strlen(g_systemMessage) > 0) {
        SDL_Surface* sM = TTF_RenderText_Blended(fontSmall, g_systemMessage, {255, 200, 80, 255});
        SDL_Texture* tM = SDL_CreateTextureFromSurface(ren, sM);
        SDL_Rect rM = {(1280-sM->w)/2, 300, sM->w, sM->h};
        SDL_RenderCopy(ren, tM, NULL, &rM);
        SDL_FreeSurface(sM); SDL_DestroyTexture(tM);
    }

    // --- MINI GALERIA ---
    int galY = 360;
    SDL_Rect galBox = {200, galY, 880, 250};
    SDL_SetRenderDrawColor(ren, 25, 25, 30, 255); SDL_RenderFillRect(ren, &galBox);
    SDL_SetRenderDrawColor(ren, 60, 60, 70, 255); SDL_RenderDrawRect(ren, &galBox);

    // Titulo Galeria
    SDL_Surface* sG = TTF_RenderText_Blended(fontSmall, "--- Galeria de Grabaciones ---", {100, 200, 255, 255});
    SDL_Texture* tG = SDL_CreateTextureFromSurface(ren, sG);
    SDL_Rect rG = {(1280-sG->w)/2, galY + 10, sG->w, sG->h};
    SDL_RenderCopy(ren, tG, NULL, &rG);
    SDL_FreeSurface(sG); SDL_DestroyTexture(tG);

    if (g_wavFiles.empty()) {
        SDL_Surface* sF = TTF_RenderText_Blended(font, "(No hay archivos .wav)", {80, 80, 80, 255});
        SDL_Texture* tF = SDL_CreateTextureFromSurface(ren, sF);
        SDL_Rect rF = {(1280-sF->w)/2, galY + 100, sF->w, sF->h};
        SDL_RenderCopy(ren, tF, NULL, &rF);
        SDL_FreeSurface(sF); SDL_DestroyTexture(tF);
    } else {
        int currentY = galY + 80;
        for (int i = -1; i <= 1; i++) {
            int idx = g_selectedFileIndex + i;
            if (idx >= 0 && idx < (int)g_wavFiles.size()) {
                SDL_Color cF = (i == 0) ? (SDL_Color){255, 255, 0, 255} : (SDL_Color){100, 100, 100, 255};
                char display[300];
                if (i == 0) sprintf(display, ">  %s  <", g_wavFiles[idx].c_str());
                else sprintf(display, "%s", g_wavFiles[idx].c_str());

                SDL_Surface* sF = TTF_RenderText_Blended(fontSmall, display, cF);
                SDL_Texture* tF = SDL_CreateTextureFromSurface(ren, sF);
                SDL_Rect rF = {(1280-sF->w)/2, currentY + (i * 40), sF->w, sF->h};
                SDL_RenderCopy(ren, tF, NULL, &rF);
                SDL_FreeSurface(sF); SDL_DestroyTexture(tF);
            }
        }
        
        // Controles de Reproductor
        const char* playerMsg = g_isMusicPlaying 
            ? "[ X: STOP ]   [ < -5s Retroceder ]   [ > +5s Adelantar ]" 
            : "[ X: PLAY ]   [ Flechas: Seleccionar ]";
            
        SDL_Surface* sP = TTF_RenderText_Blended(fontSmall, playerMsg, {100, 255, 255, 255});
        SDL_Texture* tP = SDL_CreateTextureFromSurface(ren, sP);
        SDL_Rect rP = {(1280-sP->w)/2, galY + 200, sP->w, sP->h};
        SDL_RenderCopy(ren, tP, NULL, &rP);
        SDL_FreeSurface(sP); SDL_DestroyTexture(tP);
    }
}

int EjecutarPruebaMic(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    if (Mix_PlayingMusic()) Mix_PauseMusic();
    
    TTF_Font* fontSmall = TTF_OpenFont("content/font.ttf", 22); 
    if (!fontSmall) fontSmall = font; 

    g_recBuffer = (uint8_t*)malloc(REC_BUFFER_SIZE);
    if (!g_recBuffer) return 0;

    bool running = true;
    if (!IniciarMicrofono()) sprintf(g_systemMessage, "Error Hardware Mic");
    
    EscanearArchivosWAV(); 
    OSTime lastTimeA = 0;
    
    while (running) {
        VPADStatus vpad; VPADReadError err;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &err); 
        SDL_Event ev; while(SDL_PollEvent(&ev));

        // 1. SALIR O PARAR (B)
        if (vpad.trigger & VPAD_BUTTON_B) {
            if (g_isRecording) {
                g_isRecording = false; 
                sprintf(g_systemMessage, "Finalizando grabacion...");
                DibujarUI(renderer, font, fontSmall); SDL_RenderPresent(renderer);
                GuardarWAV_Minion3(); 
                g_recWritePos = 0;    
                EscanearArchivosWAV(); 
            } else {
                DetenerReproduccion();
                running = false;
            }
        }

        // 2. GRABAR (DOBLE CLICK A)
        if (vpad.trigger & VPAD_BUTTON_A) {
            if (!g_isRecording) {
                OSTime now = OSGetTime();
                if (OSTicksToMilliseconds(now - lastTimeA) < 400) { 
                    DetenerReproduccion(); 
                    g_recWritePos = 0;
                    MICStatus st; MICGetStatus(g_hMic, &st);
                    g_lastHwPos = st.bufferPos; 
                    g_isRecording = true;
                    sprintf(g_systemMessage, "GRABANDO...");
                } else {
                    sprintf(g_systemMessage, "Pulsa (A) rapido otra vez para confirmar");
                }
                lastTimeA = now;
            }
        }

        // 3. GALERIA
        if (!g_isRecording && !g_wavFiles.empty()) {
            if (vpad.trigger & VPAD_BUTTON_UP) {
                g_selectedFileIndex--;
                if (g_selectedFileIndex < 0) g_selectedFileIndex = g_wavFiles.size() - 1;
            }
            if (vpad.trigger & VPAD_BUTTON_DOWN) {
                g_selectedFileIndex++;
                if (g_selectedFileIndex >= (int)g_wavFiles.size()) g_selectedFileIndex = 0;
            }
            if (vpad.trigger & VPAD_BUTTON_X) {
                if (g_isMusicPlaying) DetenerReproduccion();
                else ReproducirWAV(g_wavFiles[g_selectedFileIndex].c_str());
            }
            if (g_isMusicPlaying) {
                if (vpad.trigger & VPAD_BUTTON_RIGHT) {
                    Mix_SetMusicPosition(Mix_GetMusicPosition(g_currentMusic) + 5.0);
                }
                if (vpad.trigger & VPAD_BUTTON_LEFT) {
                    double pos = Mix_GetMusicPosition(g_currentMusic) - 5.0;
                    Mix_SetMusicPosition(pos < 0 ? 0 : pos);
                }
                if (Mix_PlayingMusic() == 0) {
                    g_isMusicPlaying = false;
                    sprintf(g_systemMessage, "Fin del audio.");
                }
            }
        }

        DibujarUI(renderer, font, fontSmall);
        SDL_RenderPresent(renderer);
    }

    CerrarMicrofono();
    if (fontSmall != font) TTF_CloseFont(fontSmall);
    if(g_recBuffer) free(g_recBuffer);

    // --- CORRECCION DEL BUG DE MUSICA ---
    // Como SDL_mixer mata la musica anterior al cargar el WAV, 
    // necesitamos recargarla manualmente al salir.
    // CAMBIA "content/music.mp3" POR TU ARCHIVO REAL DEL MENU
    Mix_Music* menuMusic = Mix_LoadMUS("content/music.mp3"); 
    if (menuMusic) {
        Mix_PlayMusic(menuMusic, -1); // -1 = Loop infinito
    }

    return 1;
}

#endif