#ifndef UPLOAD_H
#define UPLOAD_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <errno.h> 
#include <stdlib.h> // Para atoi

#define UPLOAD_PORT 8080
#define RECV_BUF_SIZE 4096 

// --- HTML V6: CONSOLA DE DEPURACIÓN EN PANTALLA ---
const char* HTML_UPLOAD_PAGE = 
"<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
"<style>body{background:#111;color:#fff;font-family:monospace;text-align:center;padding:10px;}"
".btn{background:#00d2ff;color:#000;padding:15px;border-radius:5px;display:block;width:100%;font-size:16px;font-weight:bold;margin:10px 0;border:none;}"
"#log{background:#222;padding:10px;border:1px solid #444;height:100px;overflow-y:scroll;text-align:left;font-size:12px;color:#0f0;}"
"input{display:none;}</style></head>"
"<body><h3>Wii U Transfer</h3>"
"<label class='btn'>[ SELECT PHOTO ]<input type='file' id='f' accept='image/*'></label>"
"<div id='log'>System Ready...</div>"
"<script>"
"function log(msg) { var l=document.getElementById('log'); l.innerHTML += '<br>> '+msg; l.scrollTop=l.scrollHeight; }"
"document.getElementById('f').onchange = function(e) {"
"  if(e.target.files.length===0) return;"
"  var file = e.target.files[0];"
"  log('File: ' + file.name + ' (' + (file.size/1024).toFixed(1) + ' KB)');"
"  log('Converting to 640x480...');"
"  var reader = new FileReader();"
"  reader.onload = function(evt) {"
"    var img = new Image();"
"    img.onload = function() {"
"       var cvs = document.createElement('canvas');"
"       cvs.width=640; cvs.height=480;"
"       var ctx = cvs.getContext('2d');"
"       ctx.drawImage(img,0,0,640,480);"
"       cvs.toBlob(function(blob){ send(blob); }, 'image/jpeg', 0.90);"
"    }; img.src = evt.target.result;"
"  }; reader.readAsDataURL(file);"
"};"
"function send(blob) {"
"  log('Sending ' + blob.size + ' bytes...');"
"  var xhr = new XMLHttpRequest();"
"  xhr.open('POST', '/upload', true);"
"  xhr.onload = function() { log('Response: ' + xhr.status + ' ' + xhr.responseText); };"
"  xhr.onerror = function() { log('ERROR: Network fail'); };"
"  xhr.send(blob);"
"}"
"</script></body></html>";

int up_serverSock = -1;

bool Up_IniciarServidor() {
    if(up_serverSock >= 0) return true;
    up_serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (up_serverSock < 0) return false;

    int val = 1; setsockopt(up_serverSock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    // IMPORTANTE: El Server Socket es Non-Blocking (para poder cancelar con B), 
    // pero el Client Socket será Blocking (para transferir seguro).
    int flags = fcntl(up_serverSock, F_GETFL, 0); 
    fcntl(up_serverSock, F_SETFL, flags | O_NONBLOCK); 

    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(UPLOAD_PORT);

    if (bind(up_serverSock, (struct sockaddr*)&addr, sizeof(addr)) < 0) return false;
    if (listen(up_serverSock, 1) < 0) return false;
    return true;
}

void Up_CerrarServidor() {
    if (up_serverSock >= 0) close(up_serverSock);
    up_serverSock = -1;
}

// Helper para enviar respuestas HTTP simples
void SendHTTP(int client, const char* status, const char* type, const char* body) {
    char header[512];
    sprintf(header, "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n", status, type, strlen(body));
    send(client, header, strlen(header), 0);
    send(client, body, strlen(body), 0);
}

// Devuelve: 1 si se guardó archivo, 0 si no pasó nada, -1 error
int Up_Procesar(const char* rutaDestino) {
    if (up_serverSock < 0) return -1;

    // 1. Aceptar Cliente (Non-Blocking)
    int client = accept(up_serverSock, NULL, NULL);
    if (client < 0) return 0; // Nadie conecta, seguimos esperando

    // 2. Cliente Conectado -> PASAR A MODO BLOQUEANTE (Crucial para estabilidad)
    int f = fcntl(client, F_GETFL, 0);
    fcntl(client, F_SETFL, f & ~O_NONBLOCK);

    // 3. Leer Cabecera (Header)
    char buffer[RECV_BUF_SIZE];
    int r = recv(client, buffer, RECV_BUF_SIZE - 1, 0);
    if (r <= 0) { close(client); return 0; }
    buffer[r] = '\0'; // Null terminate para string ops

    // 4. Analizar Método
    if (strncmp(buffer, "GET / ", 6) == 0) {
        SendHTTP(client, "200 OK", "text/html", HTML_UPLOAD_PAGE);
        usleep(100000); // Dar tiempo al buffer
        close(client);
        return 0;
    }
    
    // ESTE ES EL FIX IMPORTANTE: RESPONDER A 'OPTIONS'
    if (strncmp(buffer, "OPTIONS", 7) == 0) {
        char cors[] = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nConnection: close\r\n\r\n";
        send(client, cors, strlen(cors), 0);
        close(client);
        return 0;
    }

    if (strncmp(buffer, "POST /upload", 12) == 0) {
        // Encontrar Content-Length
        int contentLen = 0;
        char* lenPtr = strstr(buffer, "Content-Length: ");
        if (lenPtr) contentLen = atoi(lenPtr + 16);

        // Encontrar inicio del cuerpo (\r\n\r\n)
        char* bodyStart = strstr(buffer, "\r\n\r\n");
        if (bodyStart && contentLen > 0) {
            bodyStart += 4;
            int headerSize = bodyStart - buffer;
            int dataInFirstChunk = r - headerSize;

            remove(rutaDestino);
            FILE* fOut = fopen(rutaDestino, "wb");
            if (fOut) {
                // Escribir lo que llegó en el primer paquete
                if (dataInFirstChunk > 0) fwrite(bodyStart, 1, dataInFirstChunk, fOut);
                
                int totalLeido = dataInFirstChunk;
                
                // Bucle de lectura BLOQUEANTE hasta completar Content-Length
                while (totalLeido < contentLen) {
                    int n = recv(client, buffer, RECV_BUF_SIZE, 0);
                    if (n <= 0) break; // Error o desconexion
                    fwrite(buffer, 1, n, fOut);
                    totalLeido += n;
                }
                
                fflush(fOut);
                fsync(fileno(fOut)); // Asegurar SD
                fclose(fOut);
                
                SendHTTP(client, "200 OK", "text/plain", "Upload Complete");
                close(client);
                return 1; // EXITO TOTAL
            }
        }
    }

    close(client);
    return 0;
}

#endif