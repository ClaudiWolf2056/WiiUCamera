#ifndef IA_MENU_H
#define IA_MENU_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <vpad/input.h>
#include <whb/proc.h>
#include <curl/curl.h>
#include <string>
#include <vector>


extern void DibujarTextoCentrado(SDL_Renderer* renderer, TTF_Font* font, const char* texto, int y, SDL_Color color);
extern void DibujarBarraInferiorGlobal(SDL_Renderer* renderer, TTF_Font* font, const char* txtIzq, const char* txtCen, const char* txtDer);


static size_t WriteStrCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}


std::string ProcesarIADescribir(const std::string& rutaArchivo, bool esIngles) {
    std::string respuesta = "";
    FILE* imgFile = fopen(rutaArchivo.c_str(), "rb");
    if(imgFile) {
        fseek(imgFile, 0, SEEK_END); long fsize = ftell(imgFile); fseek(imgFile, 0, SEEK_SET);
        uint8_t* buffer = (uint8_t*)malloc(fsize); fread(buffer, 1, fsize, imgFile); fclose(imgFile);

        CURL *curl = curl_easy_init();
        if(curl) {
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
            headers = curl_slist_append(headers, "Expect:");

            std::string url_final = esIngles ? "https://wiiucamera-ai-proxy.onrender.com/describir_en" : "https://wiiucamera-ai-proxy.onrender.com/describir_es";

            curl_easy_setopt(curl, CURLOPT_URL, url_final.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, fsize);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteStrCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respuesta);
            curl_easy_perform(curl);
            
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
        free(buffer);
    }
    return respuesta;
}


void EjecutarMenuIA(SDL_Renderer* renderer, TTF_Font* fontG, TTF_Font* fontP, TTF_Font* fontM, bool esIngles) {
    bool corriendo = true;
    int fase = 0; // 0:Carpeta, 1:Selector (Galeria), 2:Procesando, 3:Resultados
    int carpeta = 0; 
    int delayInput = 15;
    
    
    int indexSeleccion = 0; 
    const int COLS = 4;
    const int ROWS = 3;
    const int ITEMS_PER_PAGE = COLS * ROWS;
    std::vector<FotoEntry> listaFotos; 
    std::string rutaSeleccionada = "";
    
    
    std::vector<SDL_Texture*> texturasPagina;
    int paginaCargada = -1;

    std::string textoResultado = "";
    SDL_Texture* texResultado = NULL;
    float textScrollY = 0;

    SDL_Color colW = {255, 255, 255, 255};
    SDL_Color colY = {255, 255, 0, 255};

    while (corriendo && WHBProcIsRunning()) {
        SDL_Event event; while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) corriendo = false; }
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);
        if (delayInput > 0) delayInput--;

        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        if (fase == 0) {
            DibujarTextoCentrado(renderer, fontG, esIngles ? "Select Folder" : "Seleccionar Carpeta", 150, colY);
            SDL_Color c0 = (carpeta == 0) ? colY : colW; SDL_Color c1 = (carpeta == 1) ? colY : colW;
            DibujarTextoCentrado(renderer, fontP, esIngles ? "Camera Photos" : "Fotos de la camara", 350, c0);
            DibujarTextoCentrado(renderer, fontP, "Screenshots", 430, c1);
            DibujarBarraInferiorGlobal(renderer, fontM, esIngles?"(A) Select":"(A) Seleccionar", esIngles?"(B) Back":"(B) Atras", "");

            if (delayInput == 0) {
                if ((vpad.trigger & VPAD_BUTTON_DOWN) || vpad.leftStick.y < -0.5f) { carpeta = 1; delayInput = 10; }
                if ((vpad.trigger & VPAD_BUTTON_UP) || vpad.leftStick.y > 0.5f) { carpeta = 0; delayInput = 10; }
                if (vpad.trigger & VPAD_BUTTON_B) corriendo = false;
                if (vpad.trigger & VPAD_BUTTON_A) {
                    std::string ruta = (carpeta == 0) ? "fs:/vol/external01/WiiUCamera Files" : "fs:/vol/external01/wiiu/screenshots";
                    int f=0, v=0; listaFotos = EscanearMedia(ruta, f, v);
                    std::vector<FotoEntry> soloFotos;
                    for(auto& entry : listaFotos) if(entry.tipo == TIPO_FOTO) soloFotos.push_back(entry);
                    listaFotos = soloFotos;
                    
                    indexSeleccion = 0; 
                    paginaCargada = -1; // Resetear cache
                    fase = 1; delayInput = 15;
                }
            }
        }
        else if (fase == 1) { // SELECTOR DE IMAGEN (GALERIA 3x4)
            DibujarTextoCentrado(renderer, fontG, esIngles ? "Select an Image" : "Selecciona una Imagen", 30, colY);
            int tot = listaFotos.size();
            
            if (tot == 0) {
                DibujarTextoCentrado(renderer, fontP, esIngles ? "No images found." : "No hay imagenes aqui.", 350, colW);
            } else {
                if (indexSeleccion >= tot) indexSeleccion = tot - 1;

                int paginaActual = indexSeleccion / ITEMS_PER_PAGE;

                
                if (paginaActual != paginaCargada) {
                    for (auto tex : texturasPagina) { if (tex) SDL_DestroyTexture(tex); }
                    texturasPagina.clear();
                    
                    int inicio = paginaActual * ITEMS_PER_PAGE;
                    int fin = inicio + ITEMS_PER_PAGE;
                    if (fin > tot) fin = tot;
                    
                    for (int i = inicio; i < fin; i++) {
                        SDL_Surface* s = IMG_Load(listaFotos[i].rutaCompleta.c_str());
                        if (s) {
                            texturasPagina.push_back(SDL_CreateTextureFromSurface(renderer, s));
                            SDL_FreeSurface(s);
                        } else {
                            texturasPagina.push_back(NULL);
                        }
                    }
                    paginaCargada = paginaActual;
                }

                // Dibujar la Cuadricula
                int startX = 140; int startY = 100;
                int tW = 220; int tH = 165;
                int gapX = 40; int gapY = 30;

                int inicioPagina = paginaCargada * ITEMS_PER_PAGE;
                for (size_t i = 0; i < texturasPagina.size(); i++) {
                    int currentIdx = inicioPagina + i;
                    int col = i % COLS;
                    int row = i / COLS;
                    
                    SDL_Rect rImg = { startX + col * (tW + gapX), startY + row * (tH + gapY), tW, tH };
                    
                    if (texturasPagina[i]) {
                        SDL_RenderCopy(renderer, texturasPagina[i], NULL, &rImg);
                    } else {
                        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                        SDL_RenderFillRect(renderer, &rImg);
                    }
                    
                    // Marco de seleccion
                    if (currentIdx == indexSeleccion) {
                        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                        for (int t = 0; t < 5; t++) { // Grosor 5px
                            SDL_Rect rOut = { rImg.x - t, rImg.y - t, rImg.w + t*2, rImg.h + t*2 };
                            SDL_RenderDrawRect(renderer, &rOut);
                        }
                    }
                }
                SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255); // Restaurar fondo

                char numBuf[64]; 
                sprintf(numBuf, "Pag %d / %d   -   Img %d / %d", paginaCargada + 1, (tot + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE, indexSeleccion + 1, tot);
                DibujarTextoCentrado(renderer, fontM, numBuf, 620, {0,255,255,255});
            }

            DibujarBarraInferiorGlobal(renderer, fontM, esIngles?"(L-Stick/D-Pad) Move":"(Stick/Cruz) Mover", esIngles?"(B) Back":"(B) Atras", esIngles?"(A) Process":"(A) Procesar");

            if (delayInput == 0) {
                if (vpad.trigger & VPAD_BUTTON_B) { 
                    for (auto tex : texturasPagina) { if (tex) SDL_DestroyTexture(tex); }
                    texturasPagina.clear(); paginaCargada = -1;
                    fase = 0; delayInput = 15; 
                }
                if (tot > 0) {
                    if ((vpad.hold & VPAD_BUTTON_RIGHT) || vpad.leftStick.x > 0.5f) { 
                        if (indexSeleccion < tot - 1) indexSeleccion++; 
                        delayInput = 8; 
                    }
                    if ((vpad.hold & VPAD_BUTTON_LEFT) || vpad.leftStick.x < -0.5f) { 
                        if (indexSeleccion > 0) indexSeleccion--; 
                        delayInput = 8; 
                    }
                    if ((vpad.hold & VPAD_BUTTON_DOWN) || vpad.leftStick.y < -0.5f) { 
                        if (indexSeleccion + COLS < tot) indexSeleccion += COLS; 
                        else indexSeleccion = tot - 1; 
                        delayInput = 10; 
                    }
                    if ((vpad.hold & VPAD_BUTTON_UP) || vpad.leftStick.y > 0.5f) { 
                        if (indexSeleccion - COLS >= 0) indexSeleccion -= COLS; 
                        else indexSeleccion = 0; 
                        delayInput = 10; 
                    }

                    if (vpad.trigger & VPAD_BUTTON_A) {
                        rutaSeleccionada = listaFotos[indexSeleccion].rutaCompleta;
                        textScrollY = 0; 
                        
                        // Limpiar cache para ahorrar RAM durante el procesamiento
                        for (auto tex : texturasPagina) { if (tex) SDL_DestroyTexture(tex); }
                        texturasPagina.clear(); paginaCargada = -1;
                        
                        fase = 2; delayInput = 2;
                    }
                }
            }
        }
        else if (fase == 2) { // PROCESANDO
            DibujarTextoCentrado(renderer, fontG, esIngles ? "PROCESSING AI..." : "PROCESANDO IA...", 300, {0,255,255,255});
            DibujarTextoCentrado(renderer, fontM, esIngles ? "Please wait, analyzing image :)" : "Por favor espera, analizando imagen :)", 380, colW);
            SDL_RenderPresent(renderer);

            textoResultado = ProcesarIADescribir(rutaSeleccionada, esIngles); 
            SDL_Surface* s = IMG_Load(rutaSeleccionada.c_str());
            if (s) { texResultado = SDL_CreateTextureFromSurface(renderer, s); SDL_FreeSurface(s); }
            
            fase = 3; delayInput = 20;
        }
        else if (fase == 3) { // RESULTADOS
            float scrollSpeed = 5.0f;
            if (vpad.hold & VPAD_BUTTON_DOWN) textScrollY += scrollSpeed;
            if (vpad.hold & VPAD_BUTTON_UP) textScrollY -= scrollSpeed;
            if (vpad.leftStick.y < -0.2f) textScrollY += scrollSpeed * (-vpad.leftStick.y * 1.5f);
            if (vpad.leftStick.y > 0.2f) textScrollY -= scrollSpeed * (vpad.leftStick.y * 1.5f);

            if (texResultado) {
                SDL_Rect rImg = { (1280 - 480)/2, 40, 480, 360 };
                SDL_RenderCopy(renderer, texResultado, NULL, &rImg);
            }
            
            if (textoResultado.empty()) textoResultado = "Error! Try again";
            
            SDL_Surface* sTxt = TTF_RenderText_Blended_Wrapped(fontM, textoResultado.c_str(), colY, 1100);
            if (sTxt) {
                SDL_Texture* tTxt = SDL_CreateTextureFromSurface(renderer, sTxt);
                
                int maxScroll = sTxt->h - 220;
                if (maxScroll < 0) maxScroll = 0;
                if (textScrollY > maxScroll) textScrollY = maxScroll;
                if (textScrollY < 0) textScrollY = 0;

                SDL_Rect clipRect = { 50, 420, 1180, 220 };
                SDL_RenderSetClipRect(renderer, &clipRect);
                
                SDL_Rect rTxt = { (1280 - sTxt->w)/2, 420 - (int)textScrollY, sTxt->w, sTxt->h };
                SDL_RenderCopy(renderer, tTxt, NULL, &rTxt);
                
                SDL_RenderSetClipRect(renderer, NULL);

                SDL_FreeSurface(sTxt); SDL_DestroyTexture(tTxt);
            }
            
            DibujarBarraInferiorGlobal(renderer, fontM, esIngles?"(Stick/D-Pad) Scroll":"(Stick/Cruz) Leer", esIngles?"(B) Exit":"(B) Salir", "");

            if (delayInput == 0 && (vpad.trigger & VPAD_BUTTON_B)) {
                if (texResultado) { SDL_DestroyTexture(texResultado); texResultado = NULL; }
                fase = 1; delayInput = 15;
            }
        }
        
        if (fase != 2) SDL_RenderPresent(renderer);
    }
    
    
    for (auto tex : texturasPagina) { if (tex) SDL_DestroyTexture(tex); }
    texturasPagina.clear();
    if (texResultado) SDL_DestroyTexture(texResultado);
}

#endif