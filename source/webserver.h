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
#include <vector>
#include <string>
#include <whb/log.h>
#include <errno.h>

int serverSocket = -1;
bool serverRunning = false;
std::string myIPAddress = "0.0.0.0";
std::string serverStatusMsg = "";

// Helper IP
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
    
    // Permitir reusar puerto inmediatamente
    int opt = 1; setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Socket SERVIDOR No Bloqueante
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
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", html.length());
    send(clientSocket, header, strlen(header), 0);
    send(clientSocket, html.c_str(), html.length(), 0);
}

void EnviarArchivo(int clientSocket, const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f) {
        fseek(f, 0, SEEK_END); long fsize = ftell(f); fseek(f, 0, SEEK_SET);
        
        const char* mime = "application/octet-stream";
        if (path.find(".bmp") != std::string::npos) mime = "image/bmp";
        else if (path.find(".jpg") != std::string::npos) mime = "image/jpeg";
        else if (path.find(".png") != std::string::npos) mime = "image/png";
        else if (path.find(".avi") != std::string::npos) mime = "video/x-msvideo";

        char header[512];
        sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", mime, fsize);
        send(clientSocket, header, strlen(header), 0);
        
        char* buf = (char*)malloc(8192);
        size_t leido;
        while ((leido = fread(buf, 1, 8192, f)) > 0) {
            size_t sent = 0;
            while(sent < leido) {
                ssize_t res = send(clientSocket, buf+sent, leido-sent, 0);
                if(res < 0) break;
                sent += res;
            }
        }
        free(buf); fclose(f);
    } else {
        const char* e = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nFile Missing"; 
        send(clientSocket, e, strlen(e), 0);
    }
}

// LOGICA SIMPLE (OPTIMIZADA PARA CELULAR)
void AtenderClientes(const std::string& rutaArchivo) {
    if (!serverRunning) return;

    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    
    // Aceptamos UNA conexión (No bloqueante para la UI)
    int client = accept(serverSocket, (struct sockaddr*)&clientAddr, &len);
    
    if (client >= 0) {
        // Volver socket CLIENTE a BLOQUEANTE (Crucial para enviar foto sin errores)
        int flags = fcntl(client, F_GETFL, 0);
        fcntl(client, F_SETFL, flags & ~O_NONBLOCK);
        
        char buf[1024]; 
        memset(buf, 0, 1024);
        recv(client, buf, 1024, 0);

        // Analizar petición
        if (strstr(buf, "GET /download")) {
            EnviarArchivo(client, rutaArchivo);
        } 
        else if (strstr(buf, "GET /favicon.ico")) {
            const char* e = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n"; 
            send(client, e, strlen(e), 0);
        } 
        else {
            // Pagina Principal (Bonita para celular)
            std::string html = "<html><head><title>Wii U Share</title><meta name='viewport' content='width=device-width, initial-scale=1'><style>";
            html += "body{background:#222;color:#eee;font-family:sans-serif;text-align:center;padding:20px;}";
            html += ".btn{background:#0078d7;color:white;padding:15px 30px;text-decoration:none;border-radius:8px;font-size:18px;display:inline-block;margin-top:20px;}";
            html += "img{max-width:90%;border:2px solid #555;border-radius:10px;margin-top:20px;}";
            html += "</style></head><body>";
            html += "<h1>Wii U Camera</h1>";
            html += "<p>File shared to Mobile / Archivo compartido</p>"; // Texto actualizado
            
            if (rutaArchivo.find(".avi") != std::string::npos) {
                html += "<p>[Video File Preview Not Available]</p>";
            } else {
                html += "<img src='/download' alt='Preview'><br>";
            }
            
            html += "<a href='/download' class='btn' download>Download / Descargar</a>";
            html += "</body></html>";
            
            EnviarHTML(client, html);
        }
        
        // Pausa diminuta para asegurar envío antes de cerrar
        usleep(10000);
        close(client);
    }
}

#endif