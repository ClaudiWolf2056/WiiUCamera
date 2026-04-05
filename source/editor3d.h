#ifndef EDITOR3D_H
#define EDITOR3D_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vpad/input.h>
#include <camera/camera.h> 
#include <vector>
#include <string>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <algorithm> 
#include <fstream>
#include <time.h> 
#include <dirent.h> 
#include "webserver.h"
#include "qrcodegen.hpp" 
using namespace qrcodegen;

struct Vector3 {
    float x, y, z;
    Vector3(float _x=0, float _y=0, float _z=0) : x(_x), y(_y), z(_z) {}
    Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
    Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
    Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
    Vector3& operator+=(const Vector3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    float LengthSq() const { return x*x + y*y + z*z; }
    Vector3 Normalized() const { float len = sqrt(LengthSq()); return (len == 0.0f) ? Vector3(0,0,0) : Vector3(x/len, y/len, z/len); }
    float Dot(const Vector3& v) const { return x*v.x + y*v.y + z*v.z; }
    Vector3 Cross(const Vector3& v) const { return Vector3(y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x); }
};

struct Matrix4 {
    float m[16];
    void Identity() { for(int i=0; i<16; i++) m[i] = 0.0f; m[0] = m[5] = m[10] = m[15] = 1.0f; }
    Vector3 operator*(const Vector3& v) const {
        return Vector3(m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12], m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13], m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]);
    }
};

struct Quaternion {
    float w, x, y, z;
    Quaternion(float W=1, float X=0, float Y=0, float Z=0) : w(W), x(X), y(Y), z(Z) {}
    static Quaternion FromAxisAngle(const Vector3& axis, float angleDeg) {
        float rad = angleDeg * M_PI / 180.0f; float s = sin(rad * 0.5f);
        return Quaternion(cos(rad * 0.5f), axis.x * s, axis.y * s, axis.z * s);
    }
    Quaternion operator*(const Quaternion& b) const {
        return Quaternion(w*b.w - x*b.x - y*b.y - z*b.z, w*b.x + x*b.w + y*b.z - z*b.y, w*b.y - x*b.z + y*b.w + z*b.x, w*b.z + x*b.y - y*b.x + z*b.w);
    }
    void Normalize() { float mag = sqrt(w*w + x*x + y*y + z*z); if (mag > 0.0f) { w /= mag; x /= mag; y /= mag; z /= mag; } }
    Matrix4 ToMatrix() const {
        Matrix4 out; out.Identity();
        float xx=x*x, yy=y*y, zz=z*z, xy=x*y, xz=x*z, yz=y*z, wx=w*x, wy=w*y, wz=w*z;
        out.m[0]=1-2*(yy+zz); out.m[1]=2*(xy+wz); out.m[2]=2*(xz-wy); out.m[4]=2*(xy-wz); out.m[5]=1-2*(xx+zz); out.m[6]=2*(yz+wx); out.m[8]=2*(xz+wy); out.m[9]=2*(yz-wx); out.m[10]=1-2*(xx+yy);
        return out;
    }
};

enum class ObjectType { baseObject, mesh };
struct FaceCorner { uint16_t vIndex; SDL_Color color; };
struct Face3D { FaceCorner c1, c2, c3; };
struct Edge3D { uint16_t v1, v2; };

class Object {
public:
    Object* Parent = nullptr; std::vector<Object*> Childrens; std::string name = "Object";
    bool visible = true; Vector3 pos; Quaternion rot; Vector3 scale;
    Object(Object* parent = nullptr, const std::string& nombre = "Obj", Vector3 Pos = Vector3(0,0,0)) : Parent(parent), pos(Pos), scale(Vector3(1,1,1)), name(nombre) { if(Parent) Parent->Childrens.push_back(this); }
    virtual ~Object() { for(Object* child : Childrens) delete child; }
    virtual ObjectType getType() { return ObjectType::baseObject; }
    void GetMatrix(Matrix4& out) const {
        Matrix4 T; T.Identity(); T.m[12] = pos.x; T.m[13] = pos.y; T.m[14] = pos.z; Matrix4 R = rot.ToMatrix(); Matrix4 Local;
        for(int c=0; c<4; c++) { for(int r=0; r<4; r++) { Local.m[c*4+r] = T.m[0*4+r]*R.m[c*4+0] + T.m[1*4+r]*R.m[c*4+1] + T.m[2*4+r]*R.m[c*4+2] + T.m[3*4+r]*R.m[c*4+3]; } }
        out = Local; out.m[0] *= scale.x; out.m[5] *= scale.y; out.m[10] *= scale.z;
    }
};

class Mesh : public Object {
public:
    std::vector<Vector3> vertices; std::vector<Face3D> faces; std::vector<Edge3D> edges;
    Mesh(Object* parent = nullptr, Vector3 pos = Vector3(0,0,0)) : Object(parent, "Mesh", pos) {}
    ObjectType getType() override { return ObjectType::mesh; }
    void Clear() { vertices.clear(); faces.clear(); edges.clear(); }
    Mesh* Clone() { Mesh* copia = new Mesh(this->Parent, this->pos); copia->vertices = this->vertices; copia->faces = this->faces; copia->edges = this->edges; copia->scale = this->scale; copia->rot = this->rot; return copia; }
    
    bool CargarModeloOBJ(const std::string& ruta) {
        Clear(); std::ifstream file(ruta); if (!file.is_open()) return false;
        std::string line; SDL_Color defaultColor = {180, 180, 180, 255};
        while (std::getline(file, line)) {
            if (line.substr(0, 2) == "v ") {
                Vector3 v; sscanf(line.c_str(), "v %f %f %f", &v.x, &v.y, &v.z);
                vertices.push_back(v);
            } else if (line.substr(0, 2) == "f ") {
                uint16_t i1, i2, i3;
                sscanf(line.c_str(), "f %hu %hu %hu", &i1, &i2, &i3);
                faces.push_back({{(uint16_t)(i1-1), defaultColor}, {(uint16_t)(i2-1), defaultColor}, {(uint16_t)(i3-1), defaultColor}});
                edges.push_back({(uint16_t)(i1-1), (uint16_t)(i2-1)}); edges.push_back({(uint16_t)(i2-1), (uint16_t)(i3-1)}); edges.push_back({(uint16_t)(i3-1), (uint16_t)(i1-1)});
            }
        }
        file.close(); return true;
    }
    
    void GenerarCubo(SDL_Color c) {
        Clear(); vertices = { {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1}, {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1} };
        faces = {{{0,c},{1,c},{2,c}}, {{0,c},{2,c},{3,c}}, {{5,c},{4,c},{7,c}}, {{5,c},{7,c},{6,c}}, {{4,c},{0,c},{3,c}}, {{4,c},{3,c},{7,c}}, {{1,c},{5,c},{6,c}}, {{1,c},{6,c},{2,c}}, {{3,c},{2,c},{6,c}}, {{3,c},{6,c},{7,c}}, {{4,c},{5,c},{1,c}}, {{4,c},{1,c},{0,c}}};
        edges = { {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7} };
    }

    void GenerarEsfera(int lat, int lon, SDL_Color c) {
        Clear();
        for (int i = 0; i <= lat; i++) {
            float theta = i * M_PI / lat;
            for (int j = 0; j <= lon; j++) {
                float phi = j * 2 * M_PI / lon;
                vertices.push_back({cos(phi)*sin(theta), cos(theta), sin(phi)*sin(theta)});
            }
        }
        for (int i = 0; i < lat; i++) {
            for (int j = 0; j < lon; j++) {
                int first = (i * (lon + 1)) + j, second = first + lon + 1;
                faces.push_back({{(uint16_t)first,c}, {(uint16_t)second,c}, {(uint16_t)(first+1),c}});
                faces.push_back({{(uint16_t)second,c}, {(uint16_t)(second+1),c}, {(uint16_t)(first+1),c}});
                edges.push_back({(uint16_t)first, (uint16_t)second});
                edges.push_back({(uint16_t)second, (uint16_t)(first+1)});
                edges.push_back({(uint16_t)(first+1), (uint16_t)first});
            }
        }
    }
};

enum ExportFormat { FORMAT_OBJ, FORMAT_STL };

std::string GuardarEscena3D(Object* SceneRoot, ExportFormat formato) {
    time_t t = time(NULL); std::string ts = std::to_string(t);
    std::string filename = "fs:/vol/external01/WiiUCamera Files/Figura_" + ts + (formato == FORMAT_OBJ ? ".obj" : ".stl");
    std::ofstream file(filename); if (!file.is_open()) return "";
    if (formato == FORMAT_OBJ) {
        file << "# Creado con Wii U 3D Paint Editor\n"; int vOff = 1; 
        for (Object* obj : SceneRoot->Childrens) {
            if (obj->getType() == ObjectType::mesh && obj->visible) {
                Mesh* m = (Mesh*)obj; Matrix4 M; m->GetMatrix(M); file << "o Figura\n";
                for (auto& v : m->vertices) { Vector3 w = M * v; file << "v " << w.x << " " << w.y << " " << w.z << "\n"; }
                for (auto& f : m->faces) { file << "f " << (f.c1.vIndex+vOff) << " " << (f.c2.vIndex+vOff) << " " << (f.c3.vIndex+vOff) << "\n"; }
                vOff += m->vertices.size();
            }
        }
    } 
    else if (formato == FORMAT_STL) {
        file << "solid FiguraWiiU\n";
        for (Object* obj : SceneRoot->Childrens) {
            if (obj->getType() == ObjectType::mesh && obj->visible) {
                Mesh* m = (Mesh*)obj; Matrix4 M; m->GetMatrix(M);
                for (auto& f : m->faces) {
                    Vector3 v1 = M * m->vertices[f.c1.vIndex]; Vector3 v2 = M * m->vertices[f.c2.vIndex]; Vector3 v3 = M * m->vertices[f.c3.vIndex];
                    Vector3 u = v2 - v1, v = v3 - v1; Vector3 normal(u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z, u.x*v.y - u.y*v.x); normal = normal.Normalized();
                    file << "  facet normal " << normal.x << " " << normal.y << " " << normal.z << "\n    outer loop\n";
                    file << "      vertex " << v1.x << " " << v1.y << " " << v1.z << "\n      vertex " << v2.x << " " << v2.y << " " << v2.z << "\n      vertex " << v3.x << " " << v3.y << " " << v3.z << "\n    endloop\n  endfacet\n";
                }
            }
        }
        file << "endsolid FiguraWiiU\n";
    }
    file.close(); return filename;
}

struct Camera3D { Vector3 pivot = {0, 0, 0}; float distance = 10.0f; float yaw = 45.0f; float pitch = 30.0f; };

SDL_Point ProyectarA2D(Vector3 worldPos, const Camera3D& cam, float& outDepth) {
    Vector3 p = worldPos - cam.pivot;
    float rY = -cam.yaw * M_PI / 180.0f, rP = -cam.pitch * M_PI / 180.0f;
    float x1 = p.x * cos(rY) - p.z * sin(rY), z1 = p.x * sin(rY) + p.z * cos(rY);
    float y2 = p.y * cos(rP) - z1 * sin(rP), z2 = p.y * sin(rP) + z1 * cos(rP);
    z2 += cam.distance; outDepth = z2;
    float fov = 600.0f, zSafe = (z2 <= 0.1f) ? 0.1f : z2;
    return {(int)((x1 / zSafe) * fov) + 640, (int)((-y2 / zSafe) * fov) + 360};
}

SDL_Color AplicarSombra(SDL_Color baseColor, Vector3 v1, Vector3 v2, Vector3 v3) {
    Vector3 normal = (v2 - v1).Cross(v3 - v1).Normalized(); Vector3 lightDir = Vector3(0.5f, 1.0f, 0.8f).Normalized(); 
    float intensity = 0.3f + std::max(0.0f, normal.Dot(lightDir)) * 0.7f; 
    return { (Uint8)(baseColor.r * intensity), (Uint8)(baseColor.g * intensity), (Uint8)(baseColor.b * intensity), 255 };
}

void DibujarGizmoRotacion(SDL_Renderer* renderer, Object* obj, Camera3D& cam) {
    if(!obj) return;
    Matrix4 M; obj->GetMatrix(M); Vector3 centro = M * Vector3(0,0,0);
    float radio = 2.0f; int segmentos = 36;
    for(int i=0; i<segmentos; i++) {
        float a1 = (i / (float)segmentos) * 2 * M_PI, a2 = ((i+1) / (float)segmentos) * 2 * M_PI;
        float d1, d2; SDL_Point p1, p2;
        SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
        p1 = ProyectarA2D(centro + Vector3(0, cos(a1)*radio, sin(a1)*radio), cam, d1); p2 = ProyectarA2D(centro + Vector3(0, cos(a2)*radio, sin(a2)*radio), cam, d2);
        if(d1 > 0 && d2 > 0) SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
        SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255);
        p1 = ProyectarA2D(centro + Vector3(cos(a1)*radio, 0, sin(a1)*radio), cam, d1); p2 = ProyectarA2D(centro + Vector3(cos(a2)*radio, 0, sin(a2)*radio), cam, d2);
        if(d1 > 0 && d2 > 0) SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
        SDL_SetRenderDrawColor(renderer, 50, 50, 255, 255);
        p1 = ProyectarA2D(centro + Vector3(cos(a1)*radio, sin(a1)*radio, 0), cam, d1); p2 = ProyectarA2D(centro + Vector3(cos(a2)*radio, sin(a2)*radio, 0), cam, d2);
        if(d1 > 0 && d2 > 0) SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
    }
}

struct ContextoEditor3D { CAMHandle handle; void* workMem; uint8_t* rawBuffer; uint32_t* cleanBuffer; CAMSurface surface; bool exito; SDL_Texture* textura; };
static volatile bool e3dFrameListo = false;
static void CallbackEditor3D(CAMEventData *evento) { if (evento->eventType == CAMERA_DECODE_DONE) e3dFrameListo = true; }
void CerrarEditor3D(ContextoEditor3D* ctx) { if (ctx->exito) { CAMClose(ctx->handle); CAMExit(ctx->handle); } if (ctx->textura) SDL_DestroyTexture(ctx->textura); if (ctx->rawBuffer) free(ctx->rawBuffer); if (ctx->cleanBuffer) free(ctx->cleanBuffer); if (ctx->workMem) free(ctx->workMem); ctx->exito = false; }
ContextoEditor3D IniciarEditor3D(SDL_Renderer* renderer) {
    ContextoEditor3D ctx; memset(&ctx, 0, sizeof(ContextoEditor3D)); CAMError error = CAMERA_ERROR_OK; 
    CAMStreamInfo streamInfo; streamInfo.type = CAMERA_STREAM_TYPE_1; streamInfo.width = 640; streamInfo.height = 480;
    int workMemSize = CAMGetMemReq(&streamInfo); ctx.workMem = memalign(256, workMemSize); if(ctx.workMem) memset(ctx.workMem, 0, workMemSize);
    CAMSetupInfo setupInfo; memset(&setupInfo, 0, sizeof(CAMSetupInfo)); setupInfo.streamInfo = streamInfo; setupInfo.workMem.pMem = ctx.workMem; setupInfo.workMem.size = workMemSize; setupInfo.eventHandler = CallbackEditor3D; setupInfo.mode.fps = CAMERA_FPS_30;
    ctx.handle = CAMInit(0, &setupInfo, &error);
    if (error == CAMERA_ERROR_OK) {
        ctx.exito = true; CAMOpen(ctx.handle); ctx.rawBuffer = (uint8_t*)memalign(256, CAMERA_YUV_BUFFER_SIZE); ctx.cleanBuffer = (uint32_t*)memalign(256, 640 * 480 * 4);
        memset(&ctx.surface, 0, sizeof(CAMSurface)); ctx.surface.width = 640; ctx.surface.height = 480; ctx.surface.pitch = 768; ctx.surface.alignment = CAMERA_YUV_BUFFER_ALIGNMENT; ctx.surface.surfaceSize = CAMERA_YUV_BUFFER_SIZE; ctx.surface.surfaceBuffer = ctx.rawBuffer;
        CAMSubmitTargetSurface(ctx.handle, &ctx.surface); ctx.textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);
    } else { if(ctx.workMem) free(ctx.workMem); ctx.exito = false; }
    return ctx;
}

enum ToolMode { TOOL_TRANSLATE, TOOL_ROTATE, TOOL_SCALE, TOOL_COLOR, TOOL_SCULPT, TOOL_ADD_SHAPE, TOOL_DRAW_SHAPE };
enum EstadoEscaneo { FASE_FOTO, FASE_DIBUJO, FASE_VISOR_3D, FASE_GALERIA_3D, FASE_RECIBIR_3D };

static int MapearTouch(float val, float min, float max, float outMax, bool inv) { float pct = (val - min) / (max - min); if (pct < 0) pct = 0; if (pct > 1) pct = 1; if (inv) pct = 1.0f - pct; return (int)(pct * outMax); }
void DibujarBotonUI(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int w, int h, const char* texto, bool activo) {
    if (activo) SDL_SetRenderDrawColor(renderer, 232, 127, 250, 255); else SDL_SetRenderDrawColor(renderer, 60, 60, 65, 255);           
    SDL_Rect rect = {x, y, w, h}; SDL_RenderFillRect(renderer, &rect); SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &rect);
    if (font) { SDL_Surface* s = TTF_RenderText_Blended(font, texto, {255, 255, 255, 255}); if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); SDL_Rect txtRect = {x + (w - s->w) / 2, y + (h - s->h) / 2, s->w, s->h}; SDL_RenderCopy(renderer, t, NULL, &txtRect); SDL_FreeSurface(s); SDL_DestroyTexture(t); } }
}

struct Archivo3D { std::string nombre; std::string ruta; };
std::vector<Archivo3D> LeerGaleria3D() {
    std::vector<Archivo3D> lista;
    DIR* dir = opendir("fs:/vol/external01/WiiUCamera Files/");
    if (dir != nullptr) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string n = entry->d_name;
            if (n.find(".obj") != std::string::npos || n.find(".stl") != std::string::npos) {
                lista.push_back({n, "fs:/vol/external01/WiiUCamera Files/" + n});
            }
        }
        closedir(dir);
    }
    return lista;
}

int EjecutarGenerador3D(SDL_Renderer* renderer, TTF_Font* font, bool esIngles) {
    ContextoEditor3D ctx = IniciarEditor3D(renderer);
    if (!ctx.exito) return 0;
    IniciarServidor(); 

    bool ejecutando = true; EstadoEscaneo estado = FASE_FOTO;
    SDL_Texture* fotoEstatica = NULL; uint32_t* fotoBuffer = NULL; std::vector<SDL_Point> trazo2D;
    
    Object* SceneCollection = new Object(nullptr, "Scene");
    Mesh* objActivo = new Mesh(SceneCollection, Vector3(0, 0, 0));

    Camera3D viewportCam; ToolMode herramientaActual = TOOL_TRANSLATE;
    bool lockX = true, lockY = true, lockZ = true; float brushSize = 60.0f; SDL_Color colorPincel = {255, 50, 50, 255};
    ExportFormat fmtGuardado = FORMAT_OBJ; bool guardadoExitoso = false; int guardadoFrames = 0;
    int lastTouchX = 0, lastTouchY = 0; bool wasTouched = false; SDL_Rect rectPantalla = {160, 0, 960, 720}; 

    std::vector<Archivo3D> listaArchivos; int indexGaleria = 0;
    std::string qrPathActual = ""; SDL_Texture* texQR = nullptr;

    auto ExtruirTrazo = [&](Mesh* malla) {
        malla->Clear(); std::vector<SDL_Point> pts;
        for (size_t i = 0; i < trazo2D.size(); i++) {
            int sX = 0, sY = 0, c = 0;
            for (int j = (int)i - 4; j <= (int)i + 4; j++) { if (j >= 0 && j < (int)trazo2D.size()) { sX += trazo2D[j].x; sY += trazo2D[j].y; c++; } }
            pts.push_back({sX / c, sY / c});
        }
        int n = pts.size();
        for (int i = 0; i < n; i++) { float px = ((float)pts[i].x - 640.0f) / 300.0f; float py = ((float)(720 - pts[i].y) - 360.0f) / 300.0f; malla->vertices.push_back(Vector3(px, py, 0.4f)); }
        for (int i = 0; i < n; i++) { float px = ((float)pts[i].x - 640.0f) / 300.0f; float py = ((float)(720 - pts[i].y) - 360.0f) / 300.0f; malla->vertices.push_back(Vector3(px, py, -0.4f)); }
        int iFront = malla->vertices.size(); malla->vertices.push_back(Vector3(0,0,0)); int iBack = malla->vertices.size(); malla->vertices.push_back(Vector3(0,0,0));
        for(int i=0; i<n; i++) { malla->vertices[iFront] += malla->vertices[i]; malla->vertices[iBack] += malla->vertices[i + n]; }
        malla->vertices[iFront] = malla->vertices[iFront] * (1.0f / n); malla->vertices[iBack] = malla->vertices[iBack] * (1.0f / n);
        SDL_Color cW = {180, 180, 200, 255}; 
        for (int i = 0; i < n; i++) {
            uint16_t sig = (i + 1) % n; 
            malla->faces.push_back({ {(uint16_t)iFront, cW}, {(uint16_t)i, cW}, {sig, cW} }); malla->faces.push_back({ {(uint16_t)iBack, cW}, {(uint16_t)(sig + n), cW}, {(uint16_t)(i + n), cW} });
            malla->faces.push_back({ {(uint16_t)i, cW}, {(uint16_t)(i + n), cW}, {sig, cW} }); malla->faces.push_back({ {sig, cW}, {(uint16_t)(i + n), cW}, {(uint16_t)(sig + n), cW} });
            malla->edges.push_back({(uint16_t)i, sig}); malla->edges.push_back({(uint16_t)(i + n), (uint16_t)(sig + n)}); 
        }
    };

    while (ejecutando) {
        AtenderClientes(qrPathActual); 
        SDL_Event event; while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) ejecutando = false; }
        VPADStatus vpad; VPADReadError err; VPADRead(VPAD_CHAN_0, &vpad, 1, &err);

        if (estado == FASE_FOTO && e3dFrameListo) {
            DCInvalidateRange(ctx.rawBuffer, CAMERA_YUV_BUFFER_SIZE);
            for (int y = 0; y < 480; y += 2) {
                int fY1 = y * 768, fY2 = (y + 1) * 768, fOut1 = y * 640, fOut2 = (y + 1) * 640; int fUV = (768 * 480) + ((y / 2) * 768);
                for (int x = 0; x < 640; x += 2) {
                    int idx = fUV + x, u = ctx.rawBuffer[idx] - 128, v = ctx.rawBuffer[idx + 1] - 128;
                    int cR = (351 * v) >> 8, cG = ((86 * u) + (179 * v)) >> 8, cB = (444 * u) >> 8;
                    #define CL(v) (((v)>255)?255:(((v)<0)?0:(v)))
                    auto p = [&](int yv){ return (CL(yv+cR)<<24) | (CL(yv-cG)<<16) | (CL(yv+cB)<<8) | 0xFF; };
                    ctx.cleanBuffer[fOut1+x] = p(ctx.rawBuffer[fY1+x]); ctx.cleanBuffer[fOut1+x+1] = p(ctx.rawBuffer[fY1+x+1]);
                    ctx.cleanBuffer[fOut2+x] = p(ctx.rawBuffer[fY2+x]); ctx.cleanBuffer[fOut2+x+1] = p(ctx.rawBuffer[fY2+x+1]);
                }
            }
            SDL_UpdateTexture(ctx.textura, NULL, ctx.cleanBuffer, 640 * 4); e3dFrameListo = false; CAMSubmitTargetSurface(ctx.handle, &ctx.surface);
        }

        if (estado == FASE_FOTO) {
            if (vpad.trigger & VPAD_BUTTON_B) ejecutando = false;
            if (vpad.trigger & VPAD_BUTTON_A) {
                fotoBuffer = (uint32_t*)malloc(640 * 480 * 4); memcpy(fotoBuffer, ctx.cleanBuffer, 640 * 480 * 4);
                fotoEstatica = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);
                SDL_UpdateTexture(fotoEstatica, NULL, fotoBuffer, 640 * 4); estado = FASE_DIBUJO; 
            }
        } 
        else if (estado == FASE_DIBUJO) {
            if (vpad.trigger & VPAD_BUTTON_X) { trazo2D.clear(); objActivo->Clear(); if (fotoEstatica) { SDL_DestroyTexture(fotoEstatica); fotoEstatica = NULL; } if (fotoBuffer) { free(fotoBuffer); fotoBuffer = NULL; } estado = FASE_FOTO; }
            if (vpad.trigger & VPAD_BUTTON_A && trazo2D.size() > 2) { ExtruirTrazo(objActivo); estado = FASE_VISOR_3D; }
            if (vpad.tpNormal.touched) {
                int tX = MapearTouch(vpad.tpNormal.x, 100.0f, 3950.0f, 1280.0f, false), tY = MapearTouch(vpad.tpNormal.y, 100.0f, 3900.0f, 720.0f, true);
                if (trazo2D.empty() || sqrt(pow(tX-trazo2D.back().x,2) + pow(tY-trazo2D.back().y,2)) > 10.0f) trazo2D.push_back({tX, tY});
            }
        }
        else if (estado == FASE_GALERIA_3D) {
            if (vpad.trigger & VPAD_BUTTON_X) { estado = FASE_VISOR_3D; qrPathActual = ""; if(texQR) {SDL_DestroyTexture(texQR); texQR = nullptr;} }
            if (vpad.trigger & VPAD_BUTTON_UP) { indexGaleria--; if (indexGaleria < 0) indexGaleria = listaArchivos.size() - 1; qrPathActual = ""; }
            if (vpad.trigger & VPAD_BUTTON_DOWN) { indexGaleria++; if (indexGaleria >= (int)listaArchivos.size()) indexGaleria = 0; qrPathActual = ""; }
            
            if (vpad.trigger & VPAD_BUTTON_PLUS) {
                estado = FASE_RECIBIR_3D; qrPathActual = ""; uploadCompletado = false;
                std::string url = "http://" + myIPAddress + ":8080/receiver";
                QrCode qr = QrCode::encodeText(url.c_str(), QrCode::Ecc::MEDIUM);
                int size = qr.getSize(), s = 6; 
                SDL_Surface* sm = SDL_CreateRGBSurface(0, (size+2)*s, (size+2)*s, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
                SDL_FillRect(sm, NULL, SDL_MapRGB(sm->format, 255, 255, 255));
                for(int y=0; y<size; y++) { for(int x=0; x<size; x++) { if(qr.getModule(x, y)) { SDL_Rect r = {(x+1)*s, (y+1)*s, s, s}; SDL_FillRect(sm, &r, SDL_MapRGB(sm->format, 0, 0, 0)); } } }
                if(texQR) SDL_DestroyTexture(texQR); texQR = SDL_CreateTextureFromSurface(renderer, sm); SDL_FreeSurface(sm);
            }

            if (vpad.trigger & VPAD_BUTTON_A && !listaArchivos.empty()) {
                Mesh* importado = new Mesh(SceneCollection, Vector3(0,0,0));
                if (importado->CargarModeloOBJ(listaArchivos[indexGaleria].ruta)) {
                    objActivo = importado; estado = FASE_VISOR_3D; herramientaActual = TOOL_TRANSLATE;
                } else { delete importado; }
            }
            if (vpad.trigger & VPAD_BUTTON_Y && !listaArchivos.empty()) {
                qrPathActual = listaArchivos[indexGaleria].ruta;
                std::string url = "http://" + myIPAddress + ":8080";
                QrCode qr = QrCode::encodeText(url.c_str(), QrCode::Ecc::MEDIUM);
                int size = qr.getSize(), s = 6; 
                SDL_Surface* sm = SDL_CreateRGBSurface(0, (size+2)*s, (size+2)*s, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
                SDL_FillRect(sm, NULL, SDL_MapRGB(sm->format, 255, 255, 255));
                for(int y=0; y<size; y++) { for(int x=0; x<size; x++) { if(qr.getModule(x, y)) { SDL_Rect r = {(x+1)*s, (y+1)*s, s, s}; SDL_FillRect(sm, &r, SDL_MapRGB(sm->format, 0, 0, 0)); } } }
                if(texQR) SDL_DestroyTexture(texQR); texQR = SDL_CreateTextureFromSurface(renderer, sm); SDL_FreeSurface(sm);
            }
        }
        else if (estado == FASE_RECIBIR_3D) {
            if (!uploadCompletado) {
                if (vpad.trigger & VPAD_BUTTON_B) { 
                    estado = FASE_GALERIA_3D; listaArchivos = LeerGaleria3D(); indexGaleria = 0; if(texQR) {SDL_DestroyTexture(texQR); texQR = nullptr;} 
                }
            } else {
                if (vpad.trigger & VPAD_BUTTON_A) {
                    Mesh* importado = new Mesh(SceneCollection, Vector3(0,0,0));
                    if (importado->CargarModeloOBJ(archivoSubido)) {
                        objActivo = importado; estado = FASE_VISOR_3D; herramientaActual = TOOL_TRANSLATE;
                    } else { delete importado; estado = FASE_GALERIA_3D; }
                    uploadCompletado = false;
                    if(texQR) {SDL_DestroyTexture(texQR); texQR = nullptr;}
                    listaArchivos = LeerGaleria3D(); indexGaleria = 0;
                }
                if (vpad.trigger & VPAD_BUTTON_B) {
                    uploadCompletado = false;
                    estado = FASE_GALERIA_3D; listaArchivos = LeerGaleria3D(); indexGaleria = 0; if(texQR) {SDL_DestroyTexture(texQR); texQR = nullptr;}
                }
            }
        }
        else if (estado == FASE_VISOR_3D) {
            if (vpad.trigger & VPAD_BUTTON_B) ejecutando = false;
            if (guardadoExitoso) { guardadoFrames++; if(guardadoFrames > 60) { guardadoExitoso = false; guardadoFrames = 0; } }

            if (herramientaActual == TOOL_ADD_SHAPE) {
                if (vpad.trigger & VPAD_BUTTON_LEFT) { objActivo = new Mesh(SceneCollection, Vector3(0,0,0)); objActivo->GenerarCubo(colorPincel); herramientaActual = TOOL_TRANSLATE; }
                if (vpad.trigger & VPAD_BUTTON_RIGHT) { objActivo = new Mesh(SceneCollection, Vector3(0,0,0)); objActivo->GenerarEsfera(10, 10, colorPincel); herramientaActual = TOOL_TRANSLATE; }
                if (vpad.trigger & VPAD_BUTTON_UP) { herramientaActual = TOOL_DRAW_SHAPE; trazo2D.clear(); } 
            }
            if (herramientaActual == TOOL_DRAW_SHAPE) {
                if (vpad.trigger & VPAD_BUTTON_A && trazo2D.size() > 2) { objActivo = new Mesh(SceneCollection, Vector3(0,0,0)); ExtruirTrazo(objActivo); trazo2D.clear(); herramientaActual = TOOL_TRANSLATE; }
                if (vpad.trigger & VPAD_BUTTON_X) { trazo2D.clear(); herramientaActual = TOOL_TRANSLATE; }
            }

            if (herramientaActual == TOOL_COLOR || herramientaActual == TOOL_SCULPT) {
                if (vpad.hold & VPAD_BUTTON_LEFT) brushSize -= 1.0f; if (vpad.hold & VPAD_BUTTON_RIGHT) brushSize += 1.0f;
                if (brushSize < 10.0f) brushSize = 10.0f; if (brushSize > 250.0f) brushSize = 250.0f;
                if (herramientaActual == TOOL_COLOR && (vpad.trigger & VPAD_BUTTON_Y)) { for(auto& cara : objActivo->faces) cara.c1.color = cara.c2.color = cara.c3.color = colorPincel; }
            }

            if (abs(vpad.leftStick.x) > 0.1f) viewportCam.yaw -= vpad.leftStick.x * 4.0f;
            if (abs(vpad.rightStick.y) > 0.1f) viewportCam.pitch += vpad.rightStick.y * 4.0f;
            if (viewportCam.pitch > 89.0f) viewportCam.pitch = 89.0f; if (viewportCam.pitch < -89.0f) viewportCam.pitch = -89.0f;
            if (vpad.hold & VPAD_BUTTON_UP) viewportCam.distance -= 0.3f; if (vpad.hold & VPAD_BUTTON_DOWN) viewportCam.distance += 0.3f;

            if (vpad.tpNormal.touched) {
                int tX = MapearTouch(vpad.tpNormal.x, 100.0f, 3950.0f, 1280.0f, false), tY = MapearTouch(vpad.tpNormal.y, 100.0f, 3900.0f, 720.0f, true);
                if (!wasTouched) {
                    if (tX > 1100 && tY < 50) { 
                        listaArchivos = LeerGaleria3D(); indexGaleria = 0; estado = FASE_GALERIA_3D; qrPathActual = "";
                    }
                    else if (tX < 200) { 
                        if (tY >= 20 && tY <= 55) herramientaActual = TOOL_TRANSLATE;
                        else if (tY >= 60 && tY <= 95) herramientaActual = TOOL_ROTATE; 
                        else if (tY >= 100 && tY <= 135) herramientaActual = TOOL_SCALE;
                        else if (tY >= 140 && tY <= 175) herramientaActual = TOOL_COLOR;
                        else if (tY >= 180 && tY <= 215) herramientaActual = TOOL_SCULPT;
                        else if (tY >= 220 && tY <= 255) herramientaActual = TOOL_ADD_SHAPE;
                        else if (tY >= 260 && tY <= 295) { objActivo = objActivo->Clone(); objActivo->pos.x += 1.0f; herramientaActual = TOOL_TRANSLATE; }
                        else if (tY >= 300 && tY <= 335) {
                            if (SceneCollection->Childrens.size() > 1) {
                                auto it = std::find(SceneCollection->Childrens.begin(), SceneCollection->Childrens.end(), objActivo);
                                if (it != SceneCollection->Childrens.end()) SceneCollection->Childrens.erase(it); delete objActivo;
                                for(auto* c : SceneCollection->Childrens) { if(c->getType() == ObjectType::mesh) { objActivo = (Mesh*)c; break; } }
                            }
                        }
                        else if (tY >= 340 && tY <= 375) { GuardarEscena3D(SceneCollection, fmtGuardado); guardadoExitoso = true; }
                        else if (tY >= 380 && tY <= 415) { fmtGuardado = (fmtGuardado == FORMAT_OBJ) ? FORMAT_STL : FORMAT_OBJ; }
                        else if (tY >= 430 && tY <= 470) { if (tX >= 20 && tX <= 60) lockX = !lockX; else if (tX >= 70 && tX <= 110) lockY = !lockY; else if (tX >= 120 && tX <= 160) lockZ = !lockZ; }
                        else if (tY >= 485 && tY <= 525) { if (tX >= 20 && tX <= 60) colorPincel = {255, 50, 50, 255}; else if (tX >= 70 && tX <= 110) colorPincel = {50, 255, 50, 255}; else if (tX >= 120 && tX <= 160) colorPincel = {50, 50, 255, 255}; }
                    } else if (herramientaActual != TOOL_ADD_SHAPE && herramientaActual != TOOL_DRAW_SHAPE) {
                        
                        if (herramientaActual == TOOL_ROTATE && objActivo) {
                            Matrix4 M; objActivo->GetMatrix(M); Vector3 centro = M * Vector3(0,0,0);
                            float dX, dY, dZ;
                            SDL_Point pX = ProyectarA2D(centro + Vector3(0, 2.0f, 0), viewportCam, dX); 
                            SDL_Point pY = ProyectarA2D(centro + Vector3(2.0f, 0, 0), viewportCam, dY); 
                            SDL_Point pZ = ProyectarA2D(centro + Vector3(0, 0, 2.0f), viewportCam, dZ); 
                            
                            float distX = sqrt(pow(pX.x-tX,2) + pow(pX.y-tY,2));
                            float distY = sqrt(pow(pY.x-tX,2) + pow(pY.y-tY,2));
                            float distZ = sqrt(pow(pZ.x-tX,2) + pow(pZ.y-tY,2));
                            
                            if (distX < 50.0f || distY < 50.0f || distZ < 50.0f) {
                                lockX = false; lockY = false; lockZ = false;
                                if (distX < distY && distX < distZ) lockX = true; 
                                else if (distY < distX && distY < distZ) lockY = true; 
                                else lockZ = true; 
                            }
                        }

                        float minZ = 9999.0f; Object* tgt = nullptr;
                        for (Object* obj : SceneCollection->Childrens) {
                            if (obj->getType() == ObjectType::mesh) {
                                Mesh* m = (Mesh*)obj; Matrix4 M; m->GetMatrix(M);
                                for(auto& v : m->vertices) {
                                    float zD; SDL_Point p = ProyectarA2D(M * v, viewportCam, zD);
                                    if (zD > 0 && sqrt(pow(p.x-tX,2) + pow(p.y-tY,2)) < 50.0f) { if (zD < minZ) { minZ = zD; tgt = obj; } }
                                }
                            }
                        }
                        if (tgt) objActivo = (Mesh*)tgt;
                    }
                } else { 
                    if (tX > 200) { 
                        float dx = (tX - lastTouchX) * 0.02f, dy = (lastTouchY - tY) * 0.02f; 
                        float radYaw = viewportCam.yaw * M_PI / 180.0f;
                        Vector3 right(cos(radYaw), 0, -sin(radYaw)), up(0, 1, 0); 

                        if (herramientaActual == TOOL_TRANSLATE) {
                            if(lockX) objActivo->pos.x += (right.x * dx + up.x * dy) * (viewportCam.distance * 0.2f);
                            if(lockY) objActivo->pos.y += (right.y * dx + up.y * dy) * (viewportCam.distance * 0.2f);
                            if(lockZ) objActivo->pos.z += (right.z * dx + up.z * dy) * (viewportCam.distance * 0.2f);
                        } else if (herramientaActual == TOOL_ROTATE) {
                            if(lockX) objActivo->rot = objActivo->rot * Quaternion::FromAxisAngle(Vector3(1,0,0), dy * 100.0f);
                            if(lockY) objActivo->rot = objActivo->rot * Quaternion::FromAxisAngle(Vector3(0,1,0), dx * 100.0f);
                            if(lockZ && !lockY) objActivo->rot = objActivo->rot * Quaternion::FromAxisAngle(Vector3(0,0,1), dx * 100.0f);
                            objActivo->rot.Normalize();
                        } else if (herramientaActual == TOOL_SCALE) {
                            if (vpad.hold & VPAD_BUTTON_ZR) { float dS = (dx + dy) * 0.5f; if(lockX) objActivo->scale.x += dS; if(lockY) objActivo->scale.y += dS; if(lockZ) objActivo->scale.z += dS; }
                            else { if(lockX) objActivo->scale.x += dx * 0.5f; if(lockY) objActivo->scale.y += dy * 0.5f; if(lockZ && !lockY) objActivo->scale.z += dy * 0.5f; }
                            if(objActivo->scale.x < 0.05f) objActivo->scale.x = 0.05f; if(objActivo->scale.y < 0.05f) objActivo->scale.y = 0.05f; if(objActivo->scale.z < 0.05f) objActivo->scale.z = 0.05f;
                        } else if (herramientaActual == TOOL_SCULPT) {
                            bool desinflar = (vpad.hold & VPAD_BUTTON_ZL); float deltaZbrush = (desinflar ? -1.0f : 1.0f) * 0.03f; Matrix4 M; objActivo->GetMatrix(M);
                            for(size_t i=0; i<objActivo->vertices.size(); i++) {
                                float zDepth; SDL_Point sP = ProyectarA2D(M * objActivo->vertices[i], viewportCam, zDepth);
                                if(zDepth > 0) {
                                    float dist = sqrt(pow(sP.x - tX, 2) + pow(sP.y - tY, 2));
                                    if(dist < brushSize) {
                                        float falloff = (1.0f - (dist / brushSize)) * deltaZbrush;
                                        Vector3 normal = objActivo->vertices[i].Normalized();
                                        objActivo->vertices[i].x += normal.x * falloff; objActivo->vertices[i].y += normal.y * falloff; objActivo->vertices[i].z += normal.z * falloff;
                                    }
                                }
                            }
                        } else if (herramientaActual == TOOL_COLOR) {
                            Matrix4 M; objActivo->GetMatrix(M);
                            for(auto& cara : objActivo->faces) {
                                SDL_Point s1 = ProyectarA2D(M * objActivo->vertices[cara.c1.vIndex], viewportCam, dx), s2 = ProyectarA2D(M * objActivo->vertices[cara.c2.vIndex], viewportCam, dx), s3 = ProyectarA2D(M * objActivo->vertices[cara.c3.vIndex], viewportCam, dx);
                                if(sqrt(pow(s1.x-tX,2)+pow(s1.y-tY,2))<brushSize || sqrt(pow(s2.x-tX,2)+pow(s2.y-tY,2))<brushSize || sqrt(pow(s3.x-tX,2)+pow(s3.y-tY,2))<brushSize) { cara.c1.color = cara.c2.color = cara.c3.color = colorPincel; }
                            }
                        } else if (herramientaActual == TOOL_DRAW_SHAPE) {
                            if (trazo2D.empty() || hypot(tX-trazo2D.back().x, tY-trazo2D.back().y) > 10.0f) trazo2D.push_back({tX, tY});
                        }
                    }
                }
                lastTouchX = tX; lastTouchY = tY; wasTouched = true;
            } else { wasTouched = false; }
        }

        SDL_SetRenderDrawColor(renderer, 40, 40, 45, 255); SDL_RenderClear(renderer);

        if (estado == FASE_FOTO && ctx.textura) { SDL_RenderCopy(renderer, ctx.textura, NULL, &rectPantalla); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); SDL_Rect topBar = {0,0,1280,60}; SDL_RenderFillRect(renderer, &topBar); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); DibujarBotonUI(renderer, font, 300, 10, 300, 40, esIngles ? "(A) Capture" : "(A) Capturar", false); DibujarBotonUI(renderer, font, 650, 10, 300, 40, esIngles ? "(B) Exit to Menu" : "(B) Salir al Menu", false); } 
        else if (estado == FASE_DIBUJO && fotoEstatica) { SDL_SetTextureColorMod(fotoEstatica, 100, 100, 100); SDL_RenderCopy(renderer, fotoEstatica, NULL, &rectPantalla); SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255); if (trazo2D.size() > 1) SDL_RenderDrawLines(renderer, trazo2D.data(), trazo2D.size()); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); SDL_Rect topBar = {0,0,1280,60}; SDL_RenderFillRect(renderer, &topBar); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); DibujarBotonUI(renderer, font, 300, 10, 680, 40, esIngles ? "Trace with stylus -> (A) Extrude" : "Traza la figura con el lapiz -> (A) Extruir", false); }
        else if (estado == FASE_RECIBIR_3D) {
            SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255); SDL_Rect rBg = {0,0,1280,720}; SDL_RenderFillRect(renderer, &rBg);
            if (!uploadCompletado) {
                DibujarBotonUI(renderer, font, 340, 50, 600, 60, esIngles ? "Receive model from phone/PC (BETA)" : "Recibir Modelo desde Celular/PC (BETA)", false);
                DibujarBotonUI(renderer, font, 340, 140, 600, 40, esIngles ? "Scan QR to upload .obj/.stl" : "Escanea el QR para subir un .obj/.stl", false);
                if (texQR) { SDL_Rect qrRect = { (1280 - 300)/2, 220, 300, 300 }; SDL_RenderCopy(renderer, texQR, NULL, &qrRect); }
                DibujarBotonUI(renderer, font, 540, 600, 200, 50, esIngles ? "(B) Back" : "(B) Volver", false);
            } else {
                DibujarBotonUI(renderer, font, 340, 200, 600, 60, esIngles ? "Upload Complete!" : "Carga Completada!", true);
                DibujarBotonUI(renderer, font, 340, 300, 600, 40, esIngles ? "Do you want to edit this model? We don't recommend overly detailed things." : "Deseas editar este modelo? No recomendamos cosas muy detalladas", false);
                DibujarBotonUI(renderer, font, 340, 400, 250, 50, esIngles ? "(A) Edit" : "(A) Editar", false);
                DibujarBotonUI(renderer, font, 690, 400, 250, 50, esIngles ? "(B) Cancel" : "(B) Cancelar", false);
            }
        }
        else if (estado == FASE_GALERIA_3D) {
            SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255); SDL_Rect rBg = {0,0,1280,720}; SDL_RenderFillRect(renderer, &rBg);
            DibujarBotonUI(renderer, font, 400, 40, 480, 60, esIngles ? "3D Gallery" : "Galeria 3D", false);
            if (listaArchivos.empty()) { DibujarBotonUI(renderer, font, 400, 300, 480, 60, esIngles ? "Empty Folder" : "Carpeta Vacia", false); }
            else {
                for (int i = 0; i < (int)listaArchivos.size(); i++) {
                    int offsetY = 150 + (i - indexGaleria) * 70;
                    if (offsetY > 100 && offsetY < 600) DibujarBotonUI(renderer, font, 300, offsetY, 680, 50, listaArchivos[i].nombre.c_str(), i == indexGaleria);
                }
                DibujarBotonUI(renderer, font, 100, 650, 300, 40, esIngles ? "(A) Load Editor" : "(A) Cargar al Editor", false);
                DibujarBotonUI(renderer, font, 450, 650, 300, 40, esIngles ? "(Y) Share via QR" : "(Y) Compartir por QR", false);
                DibujarBotonUI(renderer, font, 800, 650, 300, 40, esIngles ? "(X) Back" : "(X) Volver", false);
                if (texQR) { SDL_Rect qrRect = {1000, 200, 200, 200}; SDL_RenderCopy(renderer, texQR, NULL, &qrRect); DibujarBotonUI(renderer, font, 1000, 410, 200, 30, esIngles ? "Scan to download!" : "Escanea para descargar!", false); }
            }
            DibujarBotonUI(renderer, font, 100, 40, 200, 40, esIngles ? "(+) Receive Model" : "(+) Recibir Modelo", false);
        }
        else if (estado == FASE_VISOR_3D) {
            SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
            for(int i = -10; i <= 10; i++) { float d; SDL_Point p1 = ProyectarA2D(Vector3(i, 0, -10), viewportCam, d); SDL_Point p2 = ProyectarA2D(Vector3(i, 0,  10), viewportCam, d); SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y); p1 = ProyectarA2D(Vector3(-10, 0, i), viewportCam, d); p2 = ProyectarA2D(Vector3( 10, 0, i), viewportCam, d); SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y); }

            struct RenderTri { SDL_Vertex v[3]; float depth; bool isActive; }; std::vector<RenderTri> bufferTriangulos;
            for (Object* obj : SceneCollection->Childrens) {
                if (obj->getType() == ObjectType::mesh && obj->visible) {
                    Mesh* malla = (Mesh*)obj; Matrix4 M; malla->GetMatrix(M); bool active = (malla == objActivo);
                    for (const auto& cara : malla->faces) {
                        Vector3 v1 = M * malla->vertices[cara.c1.vIndex], v2 = M * malla->vertices[cara.c2.vIndex], v3 = M * malla->vertices[cara.c3.vIndex];
                        float z1, z2, z3; SDL_Point s1 = ProyectarA2D(v1, viewportCam, z1), s2 = ProyectarA2D(v2, viewportCam, z2), s3 = ProyectarA2D(v3, viewportCam, z3);
                        if (z1 > 0.1f && z2 > 0.1f && z3 > 0.1f) {
                            SDL_Color finalColor = AplicarSombra(cara.c1.color, v1, v2, v3); RenderTri tri;
                            tri.v[0].position.x = s1.x; tri.v[0].position.y = s1.y; tri.v[0].color = finalColor;
                            tri.v[1].position.x = s2.x; tri.v[1].position.y = s2.y; tri.v[1].color = finalColor;
                            tri.v[2].position.x = s3.x; tri.v[2].position.y = s3.y; tri.v[2].color = finalColor;
                            tri.depth = (z1 + z2 + z3) / 3.0f; tri.isActive = active; bufferTriangulos.push_back(tri);
                        }
                    }
                }
            }
            std::sort(bufferTriangulos.begin(), bufferTriangulos.end(), [](const RenderTri& a, const RenderTri& b) { return a.depth > b.depth; });
            for(const auto& tri : bufferTriangulos) SDL_RenderGeometry(renderer, NULL, tri.v, 3, NULL, 0);

            for (Object* obj : SceneCollection->Childrens) {
                if (obj->getType() == ObjectType::mesh && obj->visible) {
                    Mesh* malla = (Mesh*)obj; Matrix4 M; malla->GetMatrix(M);
                    if (malla == objActivo) SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255); else SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); 
                    for (const auto& edge : malla->edges) {
                        float z1, z2; SDL_Point p1 = ProyectarA2D(M * malla->vertices[edge.v1], viewportCam, z1), p2 = ProyectarA2D(M * malla->vertices[edge.v2], viewportCam, z2);
                        if (z1 > 0.1f && z2 > 0.1f) SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
                    }
                }
            }
            
            if (herramientaActual == TOOL_ROTATE && objActivo) DibujarGizmoRotacion(renderer, objActivo, viewportCam);

            if (herramientaActual == TOOL_DRAW_SHAPE && trazo2D.size() > 1) { SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255); SDL_RenderDrawLines(renderer, trazo2D.data(), trazo2D.size()); }
            if((herramientaActual == TOOL_COLOR || herramientaActual == TOOL_SCULPT) && wasTouched && lastTouchX > 200) { SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_Rect brushRect = {lastTouchX - (int)brushSize, lastTouchY - (int)brushSize, (int)(brushSize*2), (int)(brushSize*2)}; SDL_RenderDrawRect(renderer, &brushRect); }

            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(renderer, 45, 45, 50, 240); SDL_Rect panelRect = {0, 0, 200, 720}; SDL_RenderFillRect(renderer, &panelRect); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            DibujarBotonUI(renderer, font, 20, 20,  160, 35, esIngles ? "Move" : "Mover", herramientaActual == TOOL_TRANSLATE);
            DibujarBotonUI(renderer, font, 20, 60,  160, 35, esIngles ? "Rotate" : "Rotar", herramientaActual == TOOL_ROTATE);
            DibujarBotonUI(renderer, font, 20, 100, 160, 35, esIngles ? "Scale" : "Escalar", herramientaActual == TOOL_SCALE);
            DibujarBotonUI(renderer, font, 20, 140, 160, 35, esIngles ? "Paint" : "Pintar", herramientaActual == TOOL_COLOR);
            DibujarBotonUI(renderer, font, 20, 180, 160, 35, esIngles ? "Sculpt" : "Esculpir", herramientaActual == TOOL_SCULPT);
            DibujarBotonUI(renderer, font, 20, 220, 160, 35, esIngles ? "+ Add 3D" : "+ Anadir 3D", herramientaActual == TOOL_ADD_SHAPE || herramientaActual == TOOL_DRAW_SHAPE);
            DibujarBotonUI(renderer, font, 20, 260, 160, 35, esIngles ? "Clone" : "Clonar", false);
            DibujarBotonUI(renderer, font, 20, 300, 160, 35, esIngles ? "Delete" : "Borrar", false);
            
            if (guardadoExitoso) DibujarBotonUI(renderer, font, 20, 340, 160, 35, "¡Guardado!", true);
            else DibujarBotonUI(renderer, font, 20, 340, 160, 35, esIngles ? "Save" : "Guardar Archivo", false);
            std::string textFmt = "FMT: "; textFmt += (fmtGuardado == FORMAT_OBJ) ? "OBJ" : "STL"; DibujarBotonUI(renderer, font, 20, 380, 160, 35, textFmt.c_str(), false);
            
            DibujarBotonUI(renderer, font, 20, 430,  45, 40, "<X>", lockX); DibujarBotonUI(renderer, font, 75, 430,  45, 40, "^Yv", lockY); DibujarBotonUI(renderer, font, 130, 430, 45, 40, "/Z/", lockZ);
            SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255); SDL_Rect r1={20,485,40,40}; SDL_RenderFillRect(renderer,&r1);
            SDL_SetRenderDrawColor(renderer, 50, 255, 50, 255); SDL_Rect r2={70,485,40,40}; SDL_RenderFillRect(renderer,&r2);
            SDL_SetRenderDrawColor(renderer, 50, 50, 255, 255); SDL_Rect r3={120,485,40,40}; SDL_RenderFillRect(renderer,&r3);
            SDL_SetRenderDrawColor(renderer, colorPincel.r, colorPincel.g, colorPincel.b, 255); SDL_Rect rC={20,535,140,10}; SDL_RenderFillRect(renderer,&rC);
            
            if(herramientaActual == TOOL_COLOR || herramientaActual == TOOL_SCULPT) { DibujarBotonUI(renderer, font, 20, 580, 160, 30, "<- Pincel ->", false); if (herramientaActual == TOOL_COLOR) DibujarBotonUI(renderer, font, 20, 620, 160, 30, "(Y) Rellenar", false); else DibujarBotonUI(renderer, font, 20, 620, 160, 30, "Hold (ZL) Hunde", false); } 
            else if (herramientaActual == TOOL_ADD_SHAPE) { DibujarBotonUI(renderer, font, 20, 580, 160, 30, "<- Cubo", false); DibujarBotonUI(renderer, font, 20, 620, 160, 30, "Esfera ->", false); DibujarBotonUI(renderer, font, 20, 660, 160, 30, "^ Dibuja", false); } 
            else if (herramientaActual == TOOL_DRAW_SHAPE) { DibujarBotonUI(renderer, font, 20, 580, 160, 30, "(A) Confirmar", false); DibujarBotonUI(renderer, font, 20, 620, 160, 30, "(X) Cancelar", false); }
            
            DibujarBotonUI(renderer, font, 1100, 10, 160, 40, esIngles ? "File Menu" : "Menu Archivos", false);
            DibujarBotonUI(renderer, font, 1100, 670, 160, 40, esIngles ? "(B) Exit" : "(B) Salir", false);
        }
        SDL_RenderPresent(renderer);
    }
    DetenerServidor(); 
    if (fotoEstatica) SDL_DestroyTexture(fotoEstatica); if (fotoBuffer) free(fotoBuffer); delete SceneCollection; CerrarEditor3D(&ctx); return 1;
}

#endif