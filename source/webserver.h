#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/select.h>
#include <vector>
#include <string>
#include <whb/log.h>
#include <errno.h>

int serverSocket = -1;
bool serverRunning = false;
std::string myIPAddress = "0.0.0.0";
std::string serverStatusMsg = "";

bool uploadCompletado = false;
std::string archivoSubido = "";

std::string GetLocalIP() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) return "Error Socket";
    struct sockaddr_in serv; memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET; serv.sin_addr.s_addr = inet_addr("8.8.8.8"); serv.sin_port = htons(53);
    connect(sock, (const struct sockaddr*)&serv, sizeof(serv));
    struct sockaddr_in name; socklen_t namelen = sizeof(name);
    getsockname(sock, (struct sockaddr*)&name, &namelen);
    char buffer[100]; const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, 100);
    close(sock);
    return p ? std::string(p) : "127.0.0.1";
}

bool IniciarServidor() {
    if (serverRunning) return true;
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) { serverStatusMsg = "Socket Fail"; return false; }
    int opt = 1; setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    int flags = fcntl(serverSocket, F_GETFL, 0); 
    fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);
    struct sockaddr_in serverAddr; serverAddr.sin_family = AF_INET; serverAddr.sin_port = htons(8080); serverAddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) { close(serverSocket); serverStatusMsg = "Port Busy"; return false; }
    if (listen(serverSocket, 5) < 0) { close(serverSocket); serverStatusMsg = "Listen Fail"; return false; }
    myIPAddress = GetLocalIP();
    serverRunning = true;
    serverStatusMsg = "Ready";
    return true;
}

void DetenerServidor() { if (serverSocket >= 0) { close(serverSocket); serverSocket = -1; } serverRunning = false; }

void EnviarHTML(int clientSocket, const std::string& html) {
    char header[512];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", (int)html.length());
    send(clientSocket, header, strlen(header), 0);
    send(clientSocket, html.c_str(), html.length(), 0);
}

void EnviarArchivo(int clientSocket, const std::string& path, bool forceDownload) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f) {
        fseek(f, 0, SEEK_END); long fsize = ftell(f); fseek(f, 0, SEEK_SET);
        std::string ext = ""; size_t dot = path.find_last_of(".");
        if(dot != std::string::npos) { ext = path.substr(dot); for(auto& c : ext) c = tolower(c); }
        std::string filename = "WiiU_Export" + ext;
        size_t slash = path.find_last_of("/\\");
        if(slash != std::string::npos) filename = path.substr(slash + 1);
        std::string mime = "application/octet-stream";
        if (ext == ".bmp") mime = "image/bmp";
        else if (ext == ".jpg" || ext == ".jpeg") mime = "image/jpeg";
        else if (ext == ".png") mime = "image/png";
        else if (ext == ".avi") mime = "video/x-msvideo";
        else if (ext == ".mp4") mime = "video/mp4";
        else if (ext == ".obj") mime = "text/plain";
        else if (ext == ".stl") mime = "model/stl";
        std::string dispo = forceDownload ? "attachment" : "inline";
        char header[1024];
        sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nContent-Disposition: %s; filename=\"%s\"\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n", mime.c_str(), fsize, dispo.c_str(), filename.c_str());
        send(clientSocket, header, strlen(header), 0);
        std::vector<char> buf(65536); size_t leido;
        while ((leido = fread(buf.data(), 1, buf.size(), f)) > 0) {
            size_t sent = 0;
            while(sent < leido) { ssize_t res = send(clientSocket, buf.data()+sent, leido-sent, 0); if(res < 0) break; sent += res; }
        }
        fclose(f);
    } else {
        const char* e = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n"; send(clientSocket, e, strlen(e), 0);
    }
}

void AtenderClientes(const std::string& rutaArchivo) {
    if (!serverRunning) return;
    struct sockaddr_in clientAddr; socklen_t len = sizeof(clientAddr);
    int client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
    
    if (client >= 0) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(client, &rfds);
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 50000;
        
        if (select(client + 1, &rfds, NULL, NULL, &tv) > 0) {
            int flags = fcntl(client, F_GETFL, 0); fcntl(client, F_SETFL, flags & ~O_NONBLOCK);

            char buf[2048]; memset(buf, 0, 2048);
            int bytesRead = recv(client, buf, 2047, 0);
            if (bytesRead > 0) {
                std::string request(buf);
                
                if (request.find("GET /receiver") != std::string::npos) {
                    std::string html = "<html><head><title>Wii U Upload</title><meta name='viewport' content='width=device-width, initial-scale=1'><style>";
                    html += "body{background:#222;color:#eee;font-family:sans-serif;text-align:center;padding:20px;}";
                    html += ".btn{background:#e87ffa;color:white;padding:15px 30px;border-radius:8px;font-size:18px;border:none;margin-top:20px;font-weight:bold;cursor:pointer;}";
                    html += ".btn:active{background:#c64bd9;}";
                    html += "#prog{margin-top:20px; font-size:24px; color:#5f5; font-weight:bold;}";
                    html += "</style></head><body><h1>Enviar a Wii U</h1>";
                    html += "<p>Selecciona un archivo .obj o .stl para enviarlo a la consola:</p>";
                    html += "<input type='file' id='f' accept='.obj,.stl' style='margin:20px; font-size:16px;'><br>";
                    html += "<button class='btn' onclick='subir()'>Enviar Modelo 3D</button>";
                    html += "<div id='prog'></div>";
                    html += "<script>";
                    html += "function subir(){ ";
                    html += "let fi = document.getElementById('f').files[0]; if(!fi) return;";
                    html += "let xhr = new XMLHttpRequest();";
                    html += "xhr.open('POST', '/upload?name=' + encodeURIComponent(fi.name), true);";
                    html += "xhr.upload.onprogress = function(e){ if(e.lengthComputable) document.getElementById('prog').innerText = 'Subiendo: ' + Math.round((e.loaded/e.total)*100) + '%'; };";
                    html += "xhr.onload = function(){ document.getElementById('prog').innerText = 'Modelo recibido. Confirma en la Wii U.'; };";
                    html += "xhr.send(fi);";
                    html += "}</script></body></html>";
                    EnviarHTML(client, html);
                }
                else if (request.find("POST /upload") != std::string::npos) {
                    std::string name = "modelo_recibido.obj";
                    size_t namePos = request.find("name=");
                    if (namePos != std::string::npos) {
                        size_t endPos = request.find(" ", namePos);
                        if (endPos != std::string::npos) {
                            name = request.substr(namePos + 5, endPos - (namePos + 5));
                            size_t p = 0; while((p = name.find("%20", p)) != std::string::npos) { name.replace(p, 3, " "); p += 1; }
                        }
                    }
                    long clen = 0; size_t clPos = request.find("Content-Length: ");
                    if (clPos != std::string::npos) clen = atol(request.c_str() + clPos + 16);
                    char* bodyPtr = strstr(buf, "\r\n\r\n");
                    if (bodyPtr) {
                        bodyPtr += 4;
                        long headerSize = bodyPtr - buf;
                        long initialBodySize = bytesRead - headerSize;
                        std::string fullPath = "fs:/vol/external01/WiiUCamera Files/" + name;
                        FILE* f = fopen(fullPath.c_str(), "wb");
                        if (f) {
                            if (initialBodySize > 0) fwrite(bodyPtr, 1, initialBodySize, f);
                            long bytesRemaining = clen - initialBodySize;
                            std::vector<char> fileBuf(65536);
                            while (bytesRemaining > 0) {
                                int toRead = (bytesRemaining > (long)fileBuf.size()) ? fileBuf.size() : bytesRemaining;
                                int r = recv(client, fileBuf.data(), toRead, 0);
                                if (r <= 0) break;
                                fwrite(fileBuf.data(), 1, r, f);
                                bytesRemaining -= r;
                            }
                            fclose(f);
                            uploadCompletado = true;
                            archivoSubido = fullPath;
                        }
                    }
                    const char* resp = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK"; send(client, resp, strlen(resp), 0);
                }
                else if (request.find("GET /download") != std::string::npos) {
                    bool forzar = (request.find("dl=1") != std::string::npos);
                    EnviarArchivo(client, rutaArchivo, forzar);
                } 
                else if (request.find("GET /favicon.ico") != std::string::npos) {
                    const char* e = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n"; send(client, e, strlen(e), 0);
                } 
                else {
                    std::string ext = ""; size_t dot = rutaArchivo.find_last_of(".");
                    if(dot != std::string::npos) { ext = rutaArchivo.substr(dot); for(auto& c : ext) c = tolower(c); }
                    std::string html = "<html><head><title>Wii U Share</title><meta name='viewport' content='width=device-width, initial-scale=1'><style>";
                    html += "body{background:#222;color:#eee;font-family:sans-serif;text-align:center;padding:20px;}";
                    html += ".btn{background:#0078d7;color:white;padding:15px 30px;text-decoration:none;border-radius:8px;font-size:18px;display:inline-block;margin-top:20px;}";
                    html += "img, video{max-width:90%;border:2px solid #555;border-radius:10px;margin-top:20px;}";
                    html += ".nota{color:#ffcc00; font-size:14px; margin-top:15px; border:1px dashed #ffcc00; padding:10px; border-radius:5px; text-align:left; display:inline-block;}";
                    html += "</style>";
                    if(ext == ".obj" || ext == ".stl") html += "<script type='module' src='https://ajax.googleapis.com/ajax/libs/model-viewer/3.3.0/model-viewer.min.js'></script>";
                    html += "</head><body><h1>WiiUCamera Web :)</h1><p>Archivo compartido desde consola:</p>";
                    if (ext == ".avi" || ext == ".mp4") {
                        std::string vtype = (ext == ".avi") ? "video/x-msvideo" : "video/mp4";
                        html += "<video controls playsinline><source src='/download' type='" + vtype + "'>Video no soportado.</video>";
                    } else if (ext == ".obj" || ext == ".stl") {
                        html += "<model-viewer src='/download' auto-rotate camera-controls style='width:100%; height:350px; background-color:#333; margin:auto; border-radius:10px;'></model-viewer>";
                    } else {
                        html += "<img src='/download' alt='Preview'><br>";
                    }
                    html += "<br><div class='nota'>Si no carga la previsualizacion, forzar la descarga de forma segura.</div>";
                    html += "<br><a href='/download?dl=1' class='btn'>Descargar Archivo</a>";
                    html += "</body></html>";
                    EnviarHTML(client, html);
                }
            }
        }
        close(client);
    }
}

#endif