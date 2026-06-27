//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//    INF01047 Computação Gráfica e Visualização I
//               Prof. Eduardo Gastal
//
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_obj_loader.h>
#include <stb_image.h>
#include "utils.h"
#include "matrices.h"
#include "collisions.h"

// Definições de IDs
#define WALL 1
#define ALIEN 2
#define BOX 3
#define GUN 4
#define FLOOR 5
#define BULLET 6     // ID para o Projétil Laser
#define PORTAL 7     // ID para o Portal
#define AMMO_BOX 8   // ID para caixa de Munição
#define HEALTH_BOX 9 // ID para caixa de Vida
#define BUTTON 10    // ID para o botão de fim de fase

struct ObjModel
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    ObjModel(const char *filename, const char *basepath = NULL, bool triangulate = true)
    {
        printf("Carregando objetos do arquivo \"%s\"...\n", filename);
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL)
        {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos)
            {
                dirname = fullpath.substr(0, i + 1);
                basepath = dirname.c_str();
            }
        }
        std::string warn, err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);
        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());
        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");
        for (size_t shape = 0; shape < shapes.size(); ++shape)
        {
            if (shapes[shape].name.empty())
                throw std::runtime_error("Objeto sem nome.");
            printf("- Objeto interno encontrado: '%s'\n", shapes[shape].name.c_str());
        }
        printf("OK.\n");
    }
};

struct SceneObject
{
    std::string name;
    size_t first_index;
    size_t num_indices;
    GLenum rendering_mode;
    GLuint vertex_array_object_id;
    glm::vec3 bbox_min;
    glm::vec3 bbox_max;
    GLuint texture_id;
};

std::map<std::string, SceneObject> g_VirtualScene;
std::stack<glm::mat4> g_MatrixStack;
std::map<std::string, GLuint> g_TextureCache;
GLuint g_DefaultTexture = 0;
float g_ScreenRatio = 1.0f;
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false;
bool g_MiddleMouseButtonPressed = false;
float g_CameraTheta = 3.141592f; // spawn olhando p frente
float g_CameraPhi = 0.0f;
float g_CameraDistance = 3.5f;
bool  g_ThirdPerson = false; // Toggle de câmera (C)
bool  g_GameStarted = false; // Tela de início — true depois que o jogador aperta ENTER

// ============================================================================
// CONFIGURAÇÕES DA FASE E ENTIDADES
// ============================================================================

float g_MapScale = 0.50f;

// Localização Exata do Jogador
glm::vec4 g_CameraPosition = glm::vec4(-2.21f, -2.66f, -9.25f, 1.0f);

// HP do jogador
int g_PlayerHP = 100;
const int g_PlayerMaxHP = 100;

int g_PlayerAmmo = 250;           // Munição inicial
const int g_PlayerMaxAmmo = 100000; // Munição máxima que pode carregar

// Comportamento dos aliens
#define CHASER 0
#define SHOOTER 1

struct EntitySpawn
{
    int type;
    float x, y, z;
    float scale;
    float hitCooldown;   // segundos restantes ate poder bater no jogador de novo
    int behavior;        // CHASER ou SHOOTER (ignorado para nao-aliens)
    float shootCooldown; // segundos ate poder atirar de novo (apenas SHOOTER)
    int hp;              // pontos de vida (apenas aliens)
    float hitFlash;      // segundos de pisca-pisca vermelho restantes
};

std::vector<EntitySpawn> mapEntities = {
    { ALIEN,   4.27f, -1.16f, -14.56f, 0.5f, 0.0f, CHASER,  0.0f, 3, 0.0f },
    { ALIEN,  -0.34f, -1.51f, -21.68f, 0.5f, 0.0f, SHOOTER, 1.0f, 3, 0.0f },
    { ALIEN,   6.66f, -1.72f, -25.10f, 0.5f, 0.0f, CHASER,  0.0f, 3, 0.0f },
    { ALIEN,   6.92f, -1.09f, -33.12f, 0.5f, 0.0f, SHOOTER, 1.5f, 3, 0.0f },
    { ALIEN,   5.96f, -0.80f, -30.05f, 0.5f, 0.0f, CHASER,  0.0f, 3, 0.0f },
    { ALIEN,  -5.96f, -0.02f, -32.90f, 0.5f, 0.0f, SHOOTER, 2.0f, 3, 0.0f },
    { ALIEN,  -4.86f,  2.45f, -36.45f, 0.5f, 0.0f, CHASER,  0.0f, 3, 0.0f },
    { ALIEN,   4.77f,  0.59f, -43.51f, 0.5f, 0.0f, SHOOTER, 1.2f, 3, 0.0f },
    { ALIEN,   4.41f,  0.59f, -44.73f, 0.5f, 0.0f, CHASER,  0.0f, 3, 0.0f },
    { ALIEN,   2.10f,  0.49f, -44.06f, 0.5f, 0.0f, CHASER,  0.0f, 3, 0.0f },

    // --- 3 CAIXAS DE MUNIÇÃO RENDERIZADAS COM MODELO PADRÃO (BOX) ---
    { AMMO_BOX,   5.23f, -2.10f, -14.09f, 0.3f, 0.0f, 0, 0.0f, 0, 0.0f }, 
    { AMMO_BOX,  -4.44f, -2.10f, -25.50f, 0.3f, 0.0f, 0, 0.0f, 0, 0.0f },
    { AMMO_BOX,   7.31f, -1.60f, -31.83f, 0.3f, 0.0f, 0, 0.0f, 0, 0.0f },

    // --- 2 CAIXAS DE VIDA RENDERIZADAS COM MODELO DE PIZZA (PIZZAMODEL) ---
    { HEALTH_BOX, -8.21f, -0.60f, -42.91f, 0.3f, 0.0f, 0, 0.0f, 0, 0.0f },
    { HEALTH_BOX,  4.28f, -1.35f, -46.00f, 0.3f, 0.0f, 0, 0.0f, 0, 0.0f },

    // --- BOTÃO DE VITÓRIA ---
    { BUTTON,    12.94f, -7.00f, -32.23f, 0.4f, 0.0f, 0, 0.0f, 0, 0.0f }
}; // Spawns dos Aliens, do Portal e dos Itens Coletáveis (Coordenadas Atualizadas)

// --- ESTRUTURA E ARMAZENAMENTO DO PROJÉTIL (BEZIER) ---
struct Projectile
{
    bool active;
    float t;
    glm::vec4 p0, p1, p2, p3;
};
std::vector<Projectile> g_Projectiles;

// Projeteis dos inimigos (movimento linear lento)
struct EnemyProjectile
{
    bool active;
    glm::vec3 pos;
    glm::vec3 vel;
    float life; // segundos restantes antes de sumir
};
std::vector<EnemyProjectile> g_EnemyProjectiles;
// ------------------------------------------------------

bool g_WPressed = false;
bool g_APressed = false;
bool g_SPressed = false;
bool g_DPressed = false;
bool g_SpacePressed = false;
bool g_ShiftPressed = false;

float g_ForearmAngleZ = 0.0f;
float g_ForearmAngleX = 0.0f;
float g_TorsoPositionX = 0.0f;
float g_TorsoPositionY = 0.0f;

bool g_UsePerspectiveProjection = true;
bool g_ShowInfoText = true;
GLuint g_GpuProgramID = 0;
GLint g_model_uniform, g_view_uniform, g_projection_uniform, g_object_id_uniform, g_bbox_min_uniform, g_bbox_max_uniform, g_hit_flash_uniform, g_player_light_pos_uniform, g_player_light_intensity_uniform;
GLuint g_NumLoadedTextures = 0;

std::string nomeAlien;
std::string nomeBox;
std::string nomeGun;
std::string nomePlane;

void ErrorCallback(int error, const char *description) { fprintf(stderr, "ERROR: GLFW: %s\n", description); }

double g_LastCursorPosX, g_LastCursorPosY;
bool g_FirstMouse = true;

void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        g_LeftMouseButtonPressed = (action == GLFW_PRESS);
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        g_RightMouseButtonPressed = (action == GLFW_PRESS);
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        g_MiddleMouseButtonPressed = (action == GLFW_PRESS);
}

void CursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
    if (g_FirstMouse)
    {
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;
        g_FirstMouse = false;
    }
    float dx = xpos - g_LastCursorPosX;
    float dy = ypos - g_LastCursorPosY;
    g_CameraTheta -= 0.01f * dx;
    g_CameraPhi -= 0.01f * dy;
    float phimax = 3.141592f / 2.0f - 0.01f;
    float phimin = -phimax;
    if (g_CameraPhi > phimax)
        g_CameraPhi = phimax;
    if (g_CameraPhi < phimin)
        g_CameraPhi = phimin;
    g_LastCursorPosX = xpos;
    g_LastCursorPosY = ypos;
}

void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    g_CameraDistance -= 0.1f * yoffset;
    if (g_CameraDistance < 0.001f)
        g_CameraDistance = 0.001f;
}

void Correcao_KeyCallback(int key, int action, int mod);

// -----------------------------------------------------------------------

void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mod)
{
    Correcao_KeyCallback(key, action, mod);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
        g_UsePerspectiveProjection = true;
    if (key == GLFW_KEY_O && action == GLFW_PRESS)
        g_UsePerspectiveProjection = false;
    if (key == GLFW_KEY_H && action == GLFW_PRESS)
        g_ShowInfoText = !g_ShowInfoText;

    if (key == GLFW_KEY_W)
        g_WPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_S)
        g_SPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_A)
        g_APressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_D)
        g_DPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_SPACE)
        g_SpacePressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_LEFT_SHIFT)
        g_ShiftPressed = (action != GLFW_RELEASE);

    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS)
    {
        if (!g_GameStarted) g_GameStarted = true;
        g_AngleX = 0.0f;
        g_AngleY = 0.0f;
        g_AngleZ = 0.0f;
    }

    if (key == GLFW_KEY_G && action == GLFW_PRESS)
    {
        printf("\n[LOCALIZACAO] X: %.2f | Y: %.2f | Z: %.2f\n",
               g_CameraPosition.x, g_CameraPosition.y, g_CameraPosition.z);
    }

    // Alterna entre câmera primeira pessoa e terceira pessoa
    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        g_ThirdPerson = !g_ThirdPerson;
    }

    // --- FUNÇÃO DA TECLA T (RAYCAST DE TEXTURA) ---
 // --- FUNÇÃO DA TECLA T (RAYCAST DE TEXTURA) ---
    if (key == GLFW_KEY_T && action == GLFW_PRESS)
    {
        float y = sin(g_CameraPhi);
        float z = cos(g_CameraPhi) * cos(g_CameraTheta);
        float x = cos(g_CameraPhi) * sin(g_CameraTheta);
        glm::vec3 rayVector = glm::normalize(glm::vec3(x, y, z));
        glm::vec3 rayOrigin = glm::vec3(g_CameraPosition.x, g_CameraPosition.y, g_CameraPosition.z);

        float closestDistance = 99999.0f;
        std::string hitShapeName = "";

        // Correção do for loop para compatibilidade C++ antiga
        for (size_t i = 0; i < g_MapTriangles.size(); ++i) {
            float dist;
            if (RayIntersectsTriangle(rayOrigin, rayVector, g_MapTriangles[i], dist)) {
                if (dist < closestDistance && dist > 0.0f) {
                    closestDistance = dist;
                    hitShapeName = g_MapTriangles[i].shapeName;
                }
            }
        }

        if (hitShapeName != "") {
            GLuint texID = 0;
            if (g_VirtualScene.find(hitShapeName) != g_VirtualScene.end()) {
                texID = g_VirtualScene[hitShapeName].texture_id;
            }
            
            std::string texFilename = "Desconhecido/Cor Solida";
            // Correção do iterador do map
            for (std::map<std::string, GLuint>::iterator it = g_TextureCache.begin(); it != g_TextureCache.end(); ++it) {
                if (it->second == texID) {
                    texFilename = it->first;
                    break;
                }
            }
            
            printf("\n[RAYCAST] Olhando para Objeto: %s | Textura ID: %d | Arquivo: %s\n", hitShapeName.c_str(), texID, texFilename.c_str());
        } else {
            printf("\n[RAYCAST] Nao esta olhando para nenhuma parede (ou esta muito longe).\n");
        }
    }
}

void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4 &M);
void BuildTrianglesAndAddToVirtualScene(ObjModel *model, const char *basepath = NULL);
void BuildCylinder();
void ComputeNormals(ObjModel *model);
void LoadShadersFromFiles();
GLuint LoadTextureImage(const char *filename);
void DrawVirtualObject(const char *object_name);
GLuint LoadShader_Vertex(const char *filename);
GLuint LoadShader_Fragment(const char *filename);
void LoadShader(const char *filename, GLuint shader_id);
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id);
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow *window);
float TextRendering_CharWidth(GLFWwindow *window);
void TextRendering_PrintString(GLFWwindow *window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_SetColor(float r, float g, float b);
void TextRendering_ShowFramesPerSecond(GLFWwindow *window);

// ============================================================================
// HUD: shapes 2D coloridas (barras, ícones)
// ============================================================================
static GLuint g_HUDProgramID = 0;
static GLuint g_HUDVAO = 0, g_HUDVBO = 0;
static GLint g_HUDColorLoc = -1;

static const char *HUD_VS = "#version 330\nlayout(location=0) in vec2 p;\nvoid main(){ gl_Position = vec4(p, 0.0, 1.0); }\n";
static const char *HUD_FS = "#version 330\nuniform vec4 color;\nout vec4 fc;\nvoid main(){ fc = color; }\n";

void HUD_Init()
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &HUD_VS, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &HUD_FS, NULL);
    glCompileShader(fs);
    g_HUDProgramID = glCreateProgram();
    glAttachShader(g_HUDProgramID, vs);
    glAttachShader(g_HUDProgramID, fs);
    glLinkProgram(g_HUDProgramID);
    glDeleteShader(vs);
    glDeleteShader(fs);
    g_HUDColorLoc = glGetUniformLocation(g_HUDProgramID, "color");

    glGenVertexArrays(1, &g_HUDVAO);
    glGenBuffers(1, &g_HUDVBO);
    glBindVertexArray(g_HUDVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_HUDVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 256, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// x,y,w,h em coordenadas NDC (-1..1).
void HUD_DrawRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
    float v[12] = {
        x,
        y,
        x + w,
        y,
        x + w,
        y + h,
        x,
        y,
        x + w,
        y + h,
        x,
        y + h,
    };
    glUseProgram(g_HUDProgramID);
    glUniform4f(g_HUDColorLoc, r, g, b, a);
    glBindVertexArray(g_HUDVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_HUDVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// Coração desenhado com retângulos. (cx,cy) é o CENTRO. size = altura visual.
// O aspect compensa para os pixels parecerem quadrados em NDC.
void HUD_DrawHeart(float cx, float cy, float size, float aspect, float r, float g, float b)
{
    // Estilo "pixel art": dois lóbulos quadrados em cima e um V de retângulos embaixo.
    float h = size;
    float w = h / aspect; // largura igual à altura em pixels

    float lobeW = w * 0.45f;
    float lobeH = h * 0.40f;
    float top = cy + h * 0.35f;

    // Lóbulos (cantinhos arredondados ficam fora porque é pixel art)
    HUD_DrawRect(cx - w * 0.5f, top - lobeH, lobeW, lobeH, r, g, b, 1.0f);
    HUD_DrawRect(cx + w * 0.5f - lobeW, top - lobeH, lobeW, lobeH, r, g, b, 1.0f);

    // Faixa central que cobre o vão entre os lóbulos
    HUD_DrawRect(cx - w * 0.35f, top - lobeH - h * 0.05f, w * 0.7f, h * 0.20f, r, g, b, 1.0f);

    // V de retângulos descendo (largura cai linearmente)
    int N = 7;
    float startY = top - lobeH - h * 0.05f;
    float totalDown = h * 0.55f;
    float stepH = totalDown / (float)N;
    for (int i = 0; i < N; ++i)
    {
        float t = (float)(i + 1) / (float)N;
        float ww = w * (1.0f - t);
        HUD_DrawRect(cx - ww * 0.5f, startY - (i + 1) * stepH, ww, stepH + 0.002f, r, g, b, 1.0f);
    }
}

void BuildPhysicsMesh(const ObjModel &model, float scale)
{
    g_MapTriangles.clear();

    for (size_t shape_idx = 0; shape_idx < model.shapes.size(); ++shape_idx)
    {
        const auto &shape = model.shapes[shape_idx];

        if (shape.name == "bsp_model_5" || 
            shape.name == "bsp_model_6" ||
            shape.name == "bsp_model_29_23") {
            continue; 
        }

        std::string unique_name = shape.name + "_" + std::to_string(shape_idx);

        for (size_t f = 0; f < shape.mesh.indices.size() / 3; f++)
        {
            tinyobj::index_t i0 = shape.mesh.indices[3 * f + 0], i1 = shape.mesh.indices[3 * f + 1], i2 = shape.mesh.indices[3 * f + 2];
            glm::vec3 v0(model.attrib.vertices[3 * i0.vertex_index + 0], model.attrib.vertices[3 * i0.vertex_index + 1], model.attrib.vertices[3 * i0.vertex_index + 2]);
            glm::vec3 v1(model.attrib.vertices[3 * i1.vertex_index + 0], model.attrib.vertices[3 * i1.vertex_index + 1], model.attrib.vertices[3 * i1.vertex_index + 2]);
            glm::vec3 v2(model.attrib.vertices[3 * i2.vertex_index + 0], model.attrib.vertices[3 * i2.vertex_index + 1], model.attrib.vertices[3 * i2.vertex_index + 2]);
            
            // Agora delega o trabalho pesado e criação da struct para o arquivo collisions.cpp
            AddTriangleToPhysicsMesh(v0 * scale, v1 * scale, v2 * scale, unique_name);
        }
    }
    printf("- Malha de Fisica gerada via collisions.cpp\n");
}

// ============================================================================
void DrawModel(ObjModel *model)
{
    for (size_t i = 0; i < model->shapes.size(); ++i)
    {
        std::string unique_name = model->shapes[i].name + "_" + std::to_string(i);
        DrawVirtualObject(unique_name.c_str());
    }
}

void FramebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
    g_ScreenRatio = (float)width / height;
}

GLuint LoadTextureImage(const char *filename)
{
    if (g_TextureCache.find(filename) != g_TextureCache.end())
        return g_TextureCache[filename];
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);
    if (data == NULL)
    {
        return 0;
    }
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    g_TextureCache[filename] = texture_id;
    return texture_id;
}

void DrawVirtualObject(const char *object_name)
{
    if (g_VirtualScene.find(object_name) == g_VirtualScene.end())
        return;
    GLuint tex_id = g_VirtualScene[object_name].texture_id;
    if (tex_id == 0)
        tex_id = g_DefaultTexture;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage"), 0);
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);
    glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min;
    glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);
    glDrawElements(g_VirtualScene[object_name].rendering_mode, g_VirtualScene[object_name].num_indices, GL_UNSIGNED_INT, (void *)(g_VirtualScene[object_name].first_index * sizeof(GLuint)));
    glBindVertexArray(0);
}

void LoadShadersFromFiles()
{
    GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");
    if (g_GpuProgramID != 0)
        glDeleteProgram(g_GpuProgramID);
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);
    g_model_uniform = glGetUniformLocation(g_GpuProgramID, "model");
    g_view_uniform = glGetUniformLocation(g_GpuProgramID, "view");
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection");
    g_object_id_uniform = glGetUniformLocation(g_GpuProgramID, "object_id");
    g_bbox_min_uniform = glGetUniformLocation(g_GpuProgramID, "bbox_min");
    g_bbox_max_uniform = glGetUniformLocation(g_GpuProgramID, "bbox_max");
    g_hit_flash_uniform = glGetUniformLocation(g_GpuProgramID, "hit_flash");
    g_player_light_pos_uniform = glGetUniformLocation(g_GpuProgramID, "player_light_pos");
    g_player_light_intensity_uniform = glGetUniformLocation(g_GpuProgramID, "player_light_intensity");
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUseProgram(0);
}

void ComputeNormals(ObjModel *model)
{
    if (!model->attrib.normals.empty())
        return;
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            sgroup_ids.insert(model->shapes[shape].mesh.smoothing_group_ids[triangle]);
    }
    size_t num_vertices = model->attrib.vertices.size() / 3;
    model->attrib.normals.reserve(3 * num_vertices);
    for (const unsigned int &sgroup : sgroup_ids)
    {
        std::vector<int> num_triangles_per_vertex(num_vertices, 0);
        std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                if (model->shapes[shape].mesh.smoothing_group_ids[triangle] != sgroup)
                    continue;
                glm::vec4 vertices[3];
                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];
                    vertices[vertex] = glm::vec4(model->attrib.vertices[3 * idx.vertex_index + 0], model->attrib.vertices[3 * idx.vertex_index + 1], model->attrib.vertices[3 * idx.vertex_index + 2], 1.0);
                }
                const glm::vec4 n = crossproduct(vertices[1] - vertices[0], vertices[2] - vertices[0]);
                for (size_t vertex = 0; vertex < 3; ++vertex)
                {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];
                    num_triangles_per_vertex[idx.vertex_index] += 1;
                    vertex_normals[idx.vertex_index] += n;
                }
            }
        }
        std::vector<size_t> normal_indices(num_vertices, 0);
        for (size_t i = 0; i < vertex_normals.size(); ++i)
        {
            if (num_triangles_per_vertex[i] == 0)
                continue;
            glm::vec4 n = vertex_normals[i] / (float)num_triangles_per_vertex[i];
            n /= norm(n);
            model->attrib.normals.push_back(n.x);
            model->attrib.normals.push_back(n.y);
            model->attrib.normals.push_back(n.z);
            normal_indices[i] = (model->attrib.normals.size() / 3) - 1;
        }
        for (size_t shape = 0; shape < model->shapes.size(); ++shape)
        {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
            for (size_t triangle = 0; triangle < num_triangles; ++triangle)
            {
                if (model->shapes[shape].mesh.smoothing_group_ids[triangle] != sgroup)
                    continue;
                for (size_t vertex = 0; vertex < 3; ++vertex)
                    model->shapes[shape].mesh.indices[3 * triangle + vertex].normal_index = normal_indices[model->shapes[shape].mesh.indices[3 * triangle + vertex].vertex_index];
            }
        }
    }
}

void BuildTrianglesAndAddToVirtualScene(ObjModel *model, const char *basepath)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);
    std::vector<GLuint> indices;
    std::vector<float> model_coefficients;
    std::vector<float> normal_coefficients;
    std::vector<float> texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        // --- FILTRO DE EXCLUSÃO ---
        // Pula a renderização da porta para ela sumir visualmente
        if (model->shapes[shape].name == "bsp_model_5" || 
            model->shapes[shape].name == "bsp_model_6" ||
            model->shapes[shape].name == "bsp_model_29_23"
        ) {
            continue; 
        }

        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
        glm::vec3 bbox_min = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 bbox_max = glm::vec3(std::numeric_limits<float>::min());

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3 * triangle + vertex];

                size_t current_vertex = model_coefficients.size() / 4;
                indices.push_back((GLuint)current_vertex);

                float vx = model->attrib.vertices[3 * idx.vertex_index + 0];
                float vy = model->attrib.vertices[3 * idx.vertex_index + 1];
                float vz = model->attrib.vertices[3 * idx.vertex_index + 2];
                model_coefficients.push_back(vx);
                model_coefficients.push_back(vy);
                model_coefficients.push_back(vz);
                model_coefficients.push_back(1.0f);
                bbox_min.x = std::min(bbox_min.x, vx);
                bbox_min.y = std::min(bbox_min.y, vy);
                bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx);
                bbox_max.y = std::max(bbox_max.y, vy);
                bbox_max.z = std::max(bbox_max.z, vz);
                if (idx.normal_index != -1)
                {
                    normal_coefficients.push_back(model->attrib.normals[3 * idx.normal_index + 0]);
                    normal_coefficients.push_back(model->attrib.normals[3 * idx.normal_index + 1]);
                    normal_coefficients.push_back(model->attrib.normals[3 * idx.normal_index + 2]);
                    normal_coefficients.push_back(0.0f);
                }
                if (idx.texcoord_index != -1)
                {
                    texture_coefficients.push_back(model->attrib.texcoords[2 * idx.texcoord_index + 0]);
                    texture_coefficients.push_back(model->attrib.texcoords[2 * idx.texcoord_index + 1]);
                }
            }
        }

        if (indices.size() == first_index)
            continue;

        std::string unique_name = model->shapes[shape].name + "_" + std::to_string(shape);
        GLuint shape_texture_id = 0;
        if (basepath != NULL && !model->materials.empty() && !model->shapes[shape].mesh.material_ids.empty())
        {
            int mat_id = model->shapes[shape].mesh.material_ids[0];
            if (mat_id >= 0 && mat_id < model->materials.size() && !model->materials[mat_id].diffuse_texname.empty())
            {
                std::string fullpath = std::string(basepath) + model->materials[mat_id].diffuse_texname;
                shape_texture_id = LoadTextureImage(fullpath.c_str());
            }
        }
        SceneObject theobject;
        theobject.name = unique_name;
        theobject.first_index = first_index;
        theobject.num_indices = indices.size() - first_index;
        theobject.rendering_mode = GL_TRIANGLES;
        theobject.vertex_array_object_id = vertex_array_object_id;
        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;
        theobject.texture_id = shape_texture_id;
        g_VirtualScene[unique_name] = theobject;
    }
    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), model_coefficients.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    if (!normal_coefficients.empty())
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), normal_coefficients.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(1);
    }
    if (!texture_coefficients.empty())
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), texture_coefficients.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(2);
    }
    GLuint indices_id;
    glGenBuffers(1, &indices_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void BuildCylinder()
{
    std::vector<float> model_coefficients;
    std::vector<float> normal_coefficients;
    std::vector<float> texture_coefficients;
    std::vector<GLuint> indices;
    int sectors = 16;
    float radius = 1.0f;
    float height = 1.0f;
    for (int i = 0; i <= sectors; ++i)
    {
        float theta = 2.0f * 3.14159265359f * float(i) / float(sectors);
        float cx = radius * cos(theta);
        float cy = radius * sin(theta);
        model_coefficients.push_back(cx);
        model_coefficients.push_back(cy);
        model_coefficients.push_back(height / 2.0f);
        model_coefficients.push_back(1.0f);
        normal_coefficients.push_back(cx / radius);
        normal_coefficients.push_back(cy / radius);
        normal_coefficients.push_back(0.0f);
        normal_coefficients.push_back(0.0f);
        texture_coefficients.push_back(float(i) / sectors);
        texture_coefficients.push_back(1.0f);
        model_coefficients.push_back(cx);
        model_coefficients.push_back(cy);
        model_coefficients.push_back(-height / 2.0f);
        model_coefficients.push_back(1.0f);
        normal_coefficients.push_back(cx / radius);
        normal_coefficients.push_back(cy / radius);
        normal_coefficients.push_back(0.0f);
        normal_coefficients.push_back(0.0f);
        texture_coefficients.push_back(float(i) / sectors);
        texture_coefficients.push_back(0.0f);
    }
    for (int i = 0; i < sectors; ++i)
    {
        GLuint front1 = i * 2, back1 = i * 2 + 1, front2 = (i + 1) * 2, back2 = (i + 1) * 2 + 1;
        indices.push_back(front1);
        indices.push_back(back1);
        indices.push_back(front2);
        indices.push_back(back1);
        indices.push_back(back2);
        indices.push_back(front2);
    }
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);
    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), model_coefficients.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    GLuint VBO_normal_coefficients_id;
    glGenBuffers(1, &VBO_normal_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), normal_coefficients.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    GLuint VBO_texture_coefficients_id;
    glGenBuffers(1, &VBO_texture_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), texture_coefficients.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(2);
    GLuint indices_id;
    glGenBuffers(1, &indices_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
    SceneObject theobject;
    theobject.name = "laser_cylinder";
    theobject.first_index = 0;
    theobject.num_indices = indices.size();
    theobject.rendering_mode = GL_TRIANGLES;
    theobject.vertex_array_object_id = vertex_array_object_id;
    theobject.bbox_min = glm::vec3(-radius, -radius, -height / 2);
    theobject.bbox_max = glm::vec3(radius, radius, height / 2);
    theobject.texture_id = 0;
    g_VirtualScene["laser_cylinder"] = theobject;
}

GLuint LoadShader_Vertex(const char *filename)
{
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    LoadShader(filename, v);
    return v;
}
GLuint LoadShader_Fragment(const char *filename)
{
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    LoadShader(filename, f);
    return f;
}
void LoadShader(const char *filename, GLuint shader_id)
{
    std::ifstream file;
    try
    {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    }
    catch (std::exception &e)
    {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar *shader_string = str.c_str();
    const GLint length = static_cast<GLint>(str.length());
    glShaderSource(shader_id, 1, &shader_string, &length);
    glCompileShader(shader_id);
}

GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    GLuint program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);
    glLinkProgram(program_id);
    return program_id;
}

void TextRendering_ShowFramesPerSecond(GLFWwindow *window)
{
    if (!g_ShowInfoText)
        return;
    static float old_seconds = (float)glfwGetTime();
    static int ellapsed_frames = 0;
    static char buffer[20] = "?? fps";
    ellapsed_frames += 1;
    float seconds = (float)glfwGetTime();
    if (seconds - old_seconds > 1.0f)
    {
        snprintf(buffer, 20, "%.2f fps", ellapsed_frames / (seconds - old_seconds));
        old_seconds = seconds;
        ellapsed_frames = 0;
    }
    TextRendering_PrintString(window, buffer, 1.0f - (7 + 1) * TextRendering_CharWidth(window), 1.0f - TextRendering_LineHeight(window), 1.0f);
}

// ============================================================================
// MAIN LOOP
// ============================================================================
int main(int argc, char *argv[])
{
    int success = glfwInit();
    if (!success)
    {
        std::exit(EXIT_FAILURE);
    }

    glfwSetErrorCallback(ErrorCallback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Janela maximizada (não fullscreen): pega o tamanho do monitor pra criar
    // a janela já grande, e usa a hint MAXIMIZED pra o WM ocupar a área de
    // trabalho disponível mantendo borda/título e permitindo alt-tab normal.
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    GLFWwindow *window = glfwCreateWindow(mode->width, mode->height, "INF01047 - Quack", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        std::exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    // Pega o tamanho real do framebuffer (pode ser menor que o monitor por
    // causa de barra de tarefas / dock) e dispara o callback inicial.
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    FramebufferSizeCallback(window, fbw, fbh);

    LoadShadersFromFiles();

    g_DefaultTexture = LoadTextureImage("../../data/red_brick_diff_1k.jpg");

    ObjModel alienModel("../../assets/alien_obj/VG59BZPNHSI70DYK7XZUK7B4F.obj");
    ComputeNormals(&alienModel);
    BuildTrianglesAndAddToVirtualScene(&alienModel, "../../assets/alien_obj/");

    ObjModel boxModel("../../assets/box_obj/0WITZ8WLUCO2UQ5HBO68QE9ZR.obj");
    ComputeNormals(&boxModel);
    BuildTrianglesAndAddToVirtualScene(&boxModel, "../../assets/box_obj/");

    ObjModel gunModel("../../assets/gun_obj/4M495IHA13QVT7Z1F2JJ4T2OJ.obj");
    ComputeNormals(&gunModel);
    BuildTrianglesAndAddToVirtualScene(&gunModel, "../../assets/gun_obj/");

    ObjModel quakeMapModel("../../assets/quake-e1m1-the-slipgate-complex/source/e1m1/e1m1.obj");
    ComputeNormals(&quakeMapModel);
    BuildTrianglesAndAddToVirtualScene(&quakeMapModel, "../../assets/quake-e1m1-the-slipgate-complex/source/e1m1/");

    ObjModel scorpionModel("../../assets/scorpion_obj/HO4ZI3DHB4QGYROO5HFQBIK2R.obj");
    ComputeNormals(&scorpionModel);
    BuildTrianglesAndAddToVirtualScene(&scorpionModel, "../../assets/scorpion_obj/");

    ObjModel pizzaModel("../../assets/pizza-box_obj/IXTJN0D8HCSIG6PTOAWHF46UO.obj");
    ComputeNormals(&pizzaModel);
    BuildTrianglesAndAddToVirtualScene(&pizzaModel, "../../assets/pizza-box_obj/");

    ObjModel jillModel("../../assets/jill/RL5OZYCN4E44DVCXSULG0X7AV.obj");
    ComputeNormals(&jillModel);
    BuildTrianglesAndAddToVirtualScene(&jillModel, "../../assets/jill/");

    ObjModel projectileModel("../../assets/projectile/NODGA7BB7QGM8WII5GZU54ZQQ.obj");
    ComputeNormals(&projectileModel);
    BuildTrianglesAndAddToVirtualScene(&projectileModel, "../../assets/projectile/");

    ObjModel bulletModel("../../assets/bullet_obj/QVSGI1IBGLBEVYLUBSQFLVVQD.obj");
    ComputeNormals(&bulletModel);
    BuildTrianglesAndAddToVirtualScene(&bulletModel, "../../assets/bullet_obj/");

    ObjModel buttonModel("../../assets/button/AW89M644OEZNDE80N6LZ6EM2H.obj");
    ComputeNormals(&buttonModel);
    BuildTrianglesAndAddToVirtualScene(&buttonModel, "../../assets/button/");

    // PRÉ-COMPUTA A FÍSICA PARA 60FPS
    BuildPhysicsMesh(quakeMapModel, g_MapScale);

    BuildCylinder();

    TextRendering_Init();
    HUD_Init();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    int activeGun = 0;
    float recoilTimer = 0.0f;
    float RECOIL_DURATION = 0.25f;
    float RECOIL_DISTANCE = 0.6f;
    float muzzleFlashTimer = 0.0f; // segundos restantes do "boost" da headlamp ao atirar

    // Con
    glm::vec2 playerVelocityXZ(0.0f, 0.0f); // NOVO: Vetor de Inércia Lateral
    float playerVelocityY = 0.0f;
    const float GRAVITY = -15.0f;
    const float JUMP_FORCE = 6.0f;
    const float PLAYER_HEIGHT = 0.9f;
    const float PLAYER_RADIUS = 0.3f;
    const float STEP_HEIGHT = 0.55f; // Degraus/bumps menores que isso são auto-galgados
    float cameraYSmooth = 0.0f;      // Offset visual decaindo para suavizar subidas

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(g_GpuProgramID);
        glUniform1f(g_hit_flash_uniform, 0.0f); // default: sem flash

        // Headlamp do jogador. Intensidade base = 1; boost grande ao atirar
        // (muzzle flash) decai em ~0.12s. Posição segue a câmera.
        if (muzzleFlashTimer > 0.0f) {
            // deltaTime ainda não foi calculado para esse frame; decai depois
        }
        float playerLightIntensity = 1.0f + 5.0f * muzzleFlashTimer / 0.12f;
        glUniform3f(g_player_light_pos_uniform,
                    g_CameraPosition.x, g_CameraPosition.y, g_CameraPosition.z);
        glUniform1f(g_player_light_intensity_uniform, playerLightIntensity);

        static float lastTime = (float)glfwGetTime();
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // Decai o timer da muzzle flash (após deltaTime ser conhecido).
        if (muzzleFlashTimer > 0.0f) {
            muzzleFlashTimer -= deltaTime;
            if (muzzleFlashTimer < 0.0f) muzzleFlashTimer = 0.0f;
        }

        static bool playerWon = false;
        // Vitória: jogador precisa estar de pé EM CIMA do botão (XZ próximo,
        // Y dentro do alcance do step-up acima dele). Usa a posição real do
        // botão no mapa em vez de uma cópia hardcoded.
        for (const auto& ent : mapEntities) {
            if (ent.type != BUTTON) continue;
            float dx = g_CameraPosition.x - ent.x;
            float dz = g_CameraPosition.z - ent.z;
            float distXZ = sqrtf(dx*dx + dz*dz);
            float footY  = g_CameraPosition.y - 0.9f; // PLAYER_HEIGHT
            float dy     = footY - ent.y;
            // Raio do botão (~0.5 com scale 0.4) + raio do jogador (~0.3)
            const float TRIGGER_RADIUS = 0.8f;
            if (distXZ < TRIGGER_RADIUS && dy > -0.3f && dy < 1.5f && g_PlayerHP > 0) {
                playerWon = true;
            }
        }

        // GAME OVER ou VITÓRIA: congela a física (mantém renderização para mostrar tela)
        if (g_PlayerHP <= 0 || playerWon || !g_GameStarted)
        {
            deltaTime = 0.0f;
        }

        // ------------------------------------------------------------------------------------
        // VETORES DA CÂMERA DO JOGADOR

        // ------------------------------------------------------------------------------------
        // VETORES DA CÂMERA DO JOGADOR
        // ------------------------------------------------------------------------------------
        float y = sin(g_CameraPhi);
        float z = cos(g_CameraPhi) * cos(g_CameraTheta);
        float x = cos(g_CameraPhi) * sin(g_CameraTheta);

        glm::vec4 camera_view_vector = glm::vec4(x, y, z, 0.0f);
        glm::vec4 camera_up_vector = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        glm::vec4 camera_right_vector = crossproduct(camera_view_vector, camera_up_vector);
        camera_right_vector = camera_right_vector / norm(camera_right_vector);

        glm::vec3 forward_walk(camera_view_vector.x, 0.0f, camera_view_vector.z);
        if (glm::length(forward_walk) > 0.0f)
            forward_walk = glm::normalize(forward_walk);

        glm::vec3 right_walk(camera_right_vector.x, 0.0f, camera_right_vector.z);
        if (glm::length(right_walk) > 0.0f)
            right_walk = glm::normalize(right_walk);

// ------------------------------------------------------------------------------------
        // FÍSICA CLÁSSICA DO JOGADOR (Movimentação exata, sem deslize)
        // ------------------------------------------------------------------------------------
        float speed = 8.0f * deltaTime; 
        
        glm::vec3 nextPos = glm::vec3(g_CameraPosition.x, g_CameraPosition.y, g_CameraPosition.z);

        if (g_WPressed) nextPos += forward_walk * speed;
        if (g_SPressed) nextPos -= forward_walk * speed;
        if (g_APressed) nextPos -= right_walk * speed;
        if (g_DPressed) nextPos += right_walk * speed;

        // Calcula uma "velocidade" falsa apenas para a animação da arma continuar balançando
        glm::vec3 playerVelocityXZ(0.0f, 0.0f, 0.0f);
        if (g_WPressed) playerVelocityXZ += forward_walk * 8.0f;
        if (g_SPressed) playerVelocityXZ -= forward_walk * 8.0f;
        if (g_APressed) playerVelocityXZ -= right_walk * 8.0f;
        if (g_DPressed) playerVelocityXZ += right_walk * 8.0f;

        // FÍSICA 1: Colisão com paredes (com step-up estilo Quake)
        glm::vec3 nextPosFlat = nextPos;
        bool wasGrounded = (playerVelocityY == 0.0f); // estado pré-gravidade deste frame
        bool moved = false;

        // Anti-out-of-bounds: recusa qualquer XZ que não tenha chão em lugar
        // algum embaixo (ResolveFloorHeight devolve -9999 nesse caso). Vale
        // tanto no chão quanto no ar — pulos ou knockback não podem cuspir
        // o jogador no vazio. Cair de penhasco ainda funciona porque há chão
        // (mais baixo) embaixo do destino.
        auto destHasFloor = [&](float x, float z) -> bool {
            float fy = ResolveFloorHeight(x, g_CameraPosition.y, z);
            return fy > -9000.0f;
        };

        if (destHasFloor(nextPosFlat.x, nextPosFlat.z) &&
            !CheckWallCollision(nextPosFlat.x, nextPosFlat.y - PLAYER_HEIGHT, nextPosFlat.z, PLAYER_RADIUS, PLAYER_HEIGHT) &&
            !CheckEntityCollision(nextPosFlat, PLAYER_RADIUS, PLAYER_HEIGHT, mapEntities, g_PlayerAmmo, g_PlayerMaxAmmo, g_PlayerHP, g_PlayerMaxHP)) {
            g_CameraPosition.x = nextPosFlat.x;
            g_CameraPosition.z = nextPosFlat.z;
            moved = true;
        } else if (playerVelocityY <= 0.01f) {
            // No chão (ou caindo): tenta step-up / step-off
            float raisedFootY = (g_CameraPosition.y - PLAYER_HEIGHT) + STEP_HEIGHT;
            if (destHasFloor(nextPosFlat.x, nextPosFlat.z) &&
                !CheckWallCollision(nextPosFlat.x, raisedFootY, nextPosFlat.z, PLAYER_RADIUS, PLAYER_HEIGHT - STEP_HEIGHT) &&
                !CheckEntityCollision(glm::vec3(nextPosFlat.x, g_CameraPosition.y + STEP_HEIGHT, nextPosFlat.z), PLAYER_RADIUS, PLAYER_HEIGHT - STEP_HEIGHT, mapEntities, g_PlayerAmmo, g_PlayerMaxAmmo, g_PlayerHP, g_PlayerMaxHP)) {
                
                float candidateFloor = ResolveFloorHeight(nextPosFlat.x, raisedFootY, nextPosFlat.z);
                float currentFoot = g_CameraPosition.y - PLAYER_HEIGHT;
                float climb = candidateFloor - currentFoot;

                g_CameraPosition.x = nextPosFlat.x;
                g_CameraPosition.z = nextPosFlat.z;
                if (climb > 0.0f && climb <= STEP_HEIGHT + 0.01f) {
                    g_CameraPosition.y += climb;     
                    cameraYSmooth -= climb;          
                    playerVelocityY = 0.0f;
                }
                moved = true;
            }
        }
        (void)moved;

        // FÍSICA 2: Gravidade e Chão Otimizados
        float floorY = ResolveFloorHeight(g_CameraPosition.x, g_CameraPosition.y - PLAYER_HEIGHT, g_CameraPosition.z);

        // STEP-DOWN: se estava no chão e o piso "fugiu" para baixo (degrau descendo),
        // gruda no novo chão em vez de virar pulo. Só vale para descidas pequenas.
        float footNow = g_CameraPosition.y - PLAYER_HEIGHT;
        float dropToFloor = footNow - floorY;
        if (wasGrounded && !g_SpacePressed && dropToFloor > 0.02f && dropToFloor <= STEP_HEIGHT + 0.05f)
        {
            cameraYSmooth += dropToFloor; // Câmera fica "alta" e desce suave
            g_CameraPosition.y = floorY + PLAYER_HEIGHT;
            playerVelocityY = 0.0f;
            if (g_SpacePressed)
                playerVelocityY = JUMP_FORCE;
        }
        else
        {
            playerVelocityY += GRAVITY * deltaTime;
            float nextFootY = (g_CameraPosition.y + playerVelocityY * deltaTime) - PLAYER_HEIGHT;

            if (nextFootY <= floorY)
            {
                float prevFoot = g_CameraPosition.y - PLAYER_HEIGHT;
                float snap = floorY - prevFoot;
                // Se o chão subiu (rampa/degrau pequeno), suaviza visualmente
                if (snap > 0.02f && snap <= STEP_HEIGHT + 0.05f)
                {
                    cameraYSmooth -= snap;
                }
                g_CameraPosition.y = floorY + PLAYER_HEIGHT;
                playerVelocityY = 0.0f;
                if (g_SpacePressed)
                    playerVelocityY = JUMP_FORCE;
            }
            else
            {
                g_CameraPosition.y += playerVelocityY * deltaTime;
            }
        }

        // Decai o offset suave da câmera (em ~0.15s chega perto de zero)
        if (cameraYSmooth != 0.0f)
        {
            float decayPerSec = 8.0f; // maior = mais rápido
            float factor = expf(-decayPerSec * deltaTime);
            cameraYSmooth *= factor;
            if (fabsf(cameraYSmooth) < 0.001f)
                cameraYSmooth = 0.0f;
        }

        glm::vec4 cameraEye = g_CameraPosition;
        cameraEye.y += cameraYSmooth;

        // Câmera de terceira pessoa: posiciona o olho atrás do jogador, na
        // direção oposta ao view vector. Se houver uma parede entre o jogador
        // e o destino desejado, encurta a distância pra câmera não atravessar.
        if (g_ThirdPerson) {
            const float desiredDist = 3.5f;
            const float minDist     = 0.6f;
            const float yLift       = 0.8f;
            const float camRadius   = 0.25f;
            const float probeStep   = 0.15f;

            // Ponto-pivô: pé do jogador + uma altura de olho (igual ao 1ª pessoa)
            glm::vec3 pivot(g_CameraPosition.x, g_CameraPosition.y + yLift, g_CameraPosition.z);
            glm::vec3 back(-camera_view_vector.x, -camera_view_vector.y, -camera_view_vector.z);

            // Varre de minDist até desiredDist; para no primeiro ponto com parede.
            float chosenDist = desiredDist;
            for (float d = minDist; d <= desiredDist; d += probeStep) {
                glm::vec3 sample = pivot + back * d;
                if (CheckWallCollision(sample.x, sample.y - 0.5f, sample.z, camRadius, 1.0f)) {
                    chosenDist = std::max(minDist, d - probeStep);
                    break;
                }
                chosenDist = d;
            }

            cameraEye.x = pivot.x + back.x * chosenDist;
            cameraEye.y = pivot.y + back.y * chosenDist;
            cameraEye.z = pivot.z + back.z * chosenDist;
        }

        glm::mat4 viewMundo = Matrix_Camera_View(cameraEye, camera_view_vector, camera_up_vector);
        glm::mat4 projection = Matrix_Perspective(3.141592 / 3.0f, g_ScreenRatio, -0.1f, -500.0f);

        glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(viewMundo));
        glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(projection));

        glm::mat4 model = Matrix_Identity();

        model = Matrix_Translate(0.0f, 0.0f, 0.0f) * Matrix_Scale(g_MapScale, g_MapScale, g_MapScale);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, WALL);
        DrawModel(&quakeMapModel);

        // jill as third person model 
        if (g_ThirdPerson) {
            float bodyYaw = g_CameraTheta - 1.5708f; // rotaciona em pi/2
            float footY = g_CameraPosition.y - PLAYER_HEIGHT + cameraYSmooth;
            float jillHeight = 2.5f;
            float jillScale  = PLAYER_HEIGHT / jillHeight;
            model = Matrix_Translate(g_CameraPosition.x, footY + PLAYER_HEIGHT * 0.5f, g_CameraPosition.z)
                  * Matrix_Rotate_Y(bodyYaw)
                  * Matrix_Scale(jillScale, jillScale, jillScale);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, BOX);
            DrawModel(&jillModel);
        }

        // --- SEPARAÇÃO ALIEN x ALIEN (anti-overlap) ---
        // Empurra pares de aliens que estão muito próximos. Roda antes da IA.
        const float ALIEN_RADIUS = 0.45f;
        const float MIN_ALIEN_DIST = 2.0f * ALIEN_RADIUS;
        for (size_t i = 0; i < mapEntities.size(); ++i)
        {
            if (mapEntities[i].type != ALIEN)
                continue;
            for (size_t j = i + 1; j < mapEntities.size(); ++j)
            {
                if (mapEntities[j].type != ALIEN)
                    continue;
                float dx = mapEntities[j].x - mapEntities[i].x;
                float dz = mapEntities[j].z - mapEntities[i].z;
                float d2 = dx * dx + dz * dz;
                if (d2 >= MIN_ALIEN_DIST * MIN_ALIEN_DIST)
                    continue;
                float d = sqrtf(d2);
                if (d < 0.0001f)
                {
                    dx = 1.0f;
                    dz = 0.0f;
                    d = 1.0f;
                }
                float overlap = MIN_ALIEN_DIST - d;
                float nx = dx / d, nz = dz / d;
                float push = overlap * 0.5f;
                // Tenta separar respeitando paredes
                float aX = mapEntities[i].x - nx * push;
                float aZ = mapEntities[i].z - nz * push;
                float bX = mapEntities[j].x + nx * push;
                float bZ = mapEntities[j].z + nz * push;
                if (!CheckWallCollision(aX, mapEntities[i].y, aZ, 0.3f, 1.0f))
                {
                    mapEntities[i].x = aX;
                    mapEntities[i].z = aZ;
                }
                if (!CheckWallCollision(bX, mapEntities[j].y, bZ, 0.3f, 1.0f))
                {
                    mapEntities[j].x = bX;
                    mapEntities[j].z = bZ;
                }
            }
        }

        // --- ATUALIZANDO INIMIGOS ---
        for (auto &ent : mapEntities)
        {
            if (ent.type == 0)
                continue;

            // FÍSICA DOS ALIENS
            if (ent.type == ALIEN)
            {
                float alienFloorY = ResolveFloorHeight(ent.x, ent.y, ent.z);

                // NOVIDADE: Cuspida Anti-Clipping (Se o alien entrou na malha por acidente)
                if (ent.y < alienFloorY - 0.1f)
                {
                    ent.y = alienFloorY + 0.5f;
                }
                else if (ent.y > alienFloorY)
                {
                    ent.y += GRAVITY * deltaTime;
                    if (ent.y < alienFloorY)
                        ent.y = alienFloorY;
                }
                else
                {
                    ent.y = alienFloorY;
                }

                // IA de Perseguição
                float dirX = g_CameraPosition.x - ent.x;
                float dirZ = g_CameraPosition.z - ent.z;
                float dist = sqrt(dirX * dirX + dirZ * dirZ);

                float AGGRO_DISTANCE = 50.0f;
                float ENEMY_SPEED = 3.0f;
                float MELEE_RANGE = 1.0f;      // Distância para bater no jogador
                float BOUNCE_DIST = 1.5f;      // Quão longe o alien é empurrado
                float PLAYER_KNOCKBACK = 0.8f; // Quão longe o jogador é empurrado
                int ALIEN_DAMAGE = 10;         // Dano por colisão
                float HIT_COOLDOWN = 1.0f;     // Segundos entre golpes do mesmo alien
                float alienBobbingY = 0.0f;
                float alienWobbleZ = 0.0f;

                if (ent.hitCooldown > 0.0f)
                {
                    ent.hitCooldown -= deltaTime;
                    if (ent.hitCooldown < 0.0f)
                        ent.hitCooldown = 0.0f;
                }

                if (dist < AGGRO_DISTANCE && dist > 0.0001f)
                {
                    float ndirX = dirX / dist;
                    float ndirZ = dirZ / dist;

                    if (ent.behavior == CHASER)
                    {
                        // Só avança se ainda não está no alcance corpo a corpo
                        if (dist > MELEE_RANGE)
                        {
                            float stepLen = ENEMY_SPEED * deltaTime;

                            // Helper: testa se há um chão razoável em (x,z) e se não bate
                            // em parede. Permite descer pequenos degraus (ALIEN_STEP_DOWN)
                            // mas recusa o vazio (sem chão de jeito nenhum).
                            const float ALIEN_STEP_DOWN = 1.5f; // queda máxima por passo
                            auto tryStep = [&](float angleOffset, float &outX, float &outZ, float &outY) -> bool
                            {
                                float ca = cosf(angleOffset);
                                float sa = sinf(angleOffset);
                                float dX = ndirX * ca - ndirZ * sa;
                                float dZ = ndirX * sa + ndirZ * ca;
                                float nX = ent.x + dX * stepLen;
                                float nZ = ent.z + dZ * stepLen;

                                // Procura chão na nova posição (a partir de bem alto)
                                float searchFromY = ent.y + 1.0f;
                                float fY = ResolveFloorHeight(nX, searchFromY, nZ);
                                if (fY < -9000.0f)
                                    return false; // Vazio: recusa
                                if (fY > ent.y + STEP_HEIGHT + 0.05f)
                                    return false; // Subida grande demais
                                if (ent.y - fY > ALIEN_STEP_DOWN)
                                    return false; // Despenhadeiro

                                // Wall check step-aware: ignora "paredinhas" (risers de
                                // degrau pequeno) cuja altura cabe dentro do passo do
                                // alien — assim ele consegue descer escadas/curbs sem
                                // ficar travado na lateral do degrau.
                                if (CheckWallCollisionStepAware(nX, fY, nZ, 0.3f, 1.0f, STEP_HEIGHT))
                                    return false;

                                outX = nX;
                                outZ = nZ;
                                outY = fY;
                                return true;
                            };

                            // Tenta o caminho direto; se falhar, abre o leque até 90° pros
                            // dois lados pra contornar obstáculos.
                            const float angles[] = {
                                0.0f,
                                0.5236f, -0.5236f, // ±30°
                                1.0472f, -1.0472f, // ±60°
                                1.5708f, -1.5708f  // ±90°
                            };

                            float nX, nZ, nY;
                            for (float a : angles)
                            {
                                if (tryStep(a, nX, nZ, nY))
                                {
                                    ent.x = nX;
                                    ent.z = nZ;
                                    ent.y = nY;
                                    break;
                                }
                            }
                        }

                        // BATIDA: aplica knockback nos dois e dano no jogador
                        if (dist <= MELEE_RANGE && ent.hitCooldown == 0.0f)
                        {
                            if (g_PlayerHP > 0)
                            {
                                g_PlayerHP -= ALIEN_DAMAGE;
                                if (g_PlayerHP < 0)
                                    g_PlayerHP = 0;
                            }
                            ent.hitCooldown = HIT_COOLDOWN;

                            // Empurra alien para trás (sentido oposto ao jogador)
                            float bX = ent.x - ndirX * BOUNCE_DIST;
                            float bZ = ent.z - ndirZ * BOUNCE_DIST;
                            if (!CheckWallCollision(bX, ent.y, bZ, 0.3f, 1.0f))
                            {
                                ent.x = bX;
                                ent.z = bZ;
                            }

                            // empurra o jogador pra trás, mas só se tiver chão no destino
                            glm::vec3 pushPos = glm::vec3(
                                g_CameraPosition.x + ndirX * PLAYER_KNOCKBACK,
                                g_CameraPosition.y,
                                g_CameraPosition.z + ndirZ * PLAYER_KNOCKBACK
                            );
                            float pushFloorY = ResolveFloorHeight(pushPos.x, g_CameraPosition.y, pushPos.z);
                            if (pushFloorY > -9000.0f &&
                                !CheckWallCollision(pushPos.x, pushPos.y - PLAYER_HEIGHT, pushPos.z, PLAYER_RADIUS, PLAYER_HEIGHT) &&
                                !CheckEntityCollision(pushPos, PLAYER_RADIUS, PLAYER_HEIGHT, mapEntities, g_PlayerAmmo, g_PlayerMaxAmmo, g_PlayerHP, g_PlayerMaxHP)) {
                                g_CameraPosition.x = pushPos.x;
                                g_CameraPosition.z = pushPos.z;
                            }
                        }

                        float runAnimSpeed = 15.0f;
                        alienBobbingY = abs(sin(currentTime * runAnimSpeed)) * 0.15f;
                        alienWobbleZ = cos(currentTime * runAnimSpeed * 0.5f) * 0.15f;
                    }
                    else if (ent.behavior == SHOOTER)
                    {
                        // Atirador: parado, dispara projeteis lentos quando o cooldown zera
                        ent.shootCooldown -= deltaTime;
                        const float SHOOT_INTERVAL = 2.2f;
                        const float PROJECTILE_SPEED = 6.0f;
                        const float PROJECTILE_LIFE = 4.0f;
                        if (ent.shootCooldown <= 0.0f)
                        {
                            ent.shootCooldown = SHOOT_INTERVAL;

                            // Origem na altura do peito do alien
                            glm::vec3 origin(ent.x, ent.y + 0.9f, ent.z);
                            glm::vec3 target(g_CameraPosition.x, g_CameraPosition.y - 0.3f, g_CameraPosition.z);
                            glm::vec3 dir = target - origin;
                            float dlen = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                            if (dlen > 0.0001f)
                                dir /= dlen;

                            EnemyProjectile ep;
                            ep.active = true;
                            ep.pos = origin + dir * 0.6f; // sai um pouco à frente do alien
                            ep.vel = dir * PROJECTILE_SPEED;
                            ep.life = PROJECTILE_LIFE;
                            g_EnemyProjectiles.push_back(ep);
                        }
                        // Pequena animação parada (respiração)
                        alienBobbingY = sinf(currentTime * 2.0f) * 0.05f;
                    }
                }

                float angle = atan2(dirX, dirZ);
                
                // offsets diferentes pros modelos
                float modelYOffset = (ent.behavior == SHOOTER) ? 0.65f : 0.1f;
                model = Matrix_Translate(ent.x, ent.y + alienBobbingY + modelYOffset, ent.z) * Matrix_Rotate_Y(angle + 1.5708f) * Matrix_Rotate_Z(alienWobbleZ) * Matrix_Scale(ent.scale, ent.scale, ent.scale);

                // Pisca-pisca ao ser baleado: decai e alterna intensidade
                if (ent.hitFlash > 0.0f)
                {
                    ent.hitFlash -= deltaTime;
                    if (ent.hitFlash < 0.0f)
                        ent.hitFlash = 0.0f;
                }
                float flashIntensity = 0.0f;
                if (ent.hitFlash > 0.0f)
                {
                    // Pisca 3x na duração do flash (~12 Hz)
                    float strobe = 0.5f + 0.5f * sinf(ent.hitFlash * 75.0f);
                    flashIntensity = strobe * 0.85f;
                }
                glUniform1f(g_hit_flash_uniform, flashIntensity);

                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(g_object_id_uniform, ALIEN);
                // Mapeamento condicional do modelo
                if (ent.behavior == SHOOTER) {
                    DrawModel(&alienModel);
                } else {
                    DrawModel(&scorpionModel);
                }
                glUniform1f(g_hit_flash_uniform, 0.0f); // restaura para próximos draws
            }
            else if (ent.type == BOX)
            {
                float boxFloorY = ResolveFloorHeight(ent.x, ent.y, ent.z);
                if (ent.y > boxFloorY)
                {
                    ent.y += GRAVITY * deltaTime;
                    if (ent.y < boxFloorY)
                        ent.y = boxFloorY;
                }

                model = Matrix_Translate(ent.x, ent.y + 0.25f, ent.z) * Matrix_Scale(ent.scale, ent.scale, ent.scale);
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(g_object_id_uniform, BOX);
                DrawModel(&boxModel);
            }
            else if (ent.type == PORTAL)
            {
                model = Matrix_Translate(ent.x, ent.y + 1.0f, ent.z) * Matrix_Scale(1.0f, 2.0f, 0.1f);
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(g_object_id_uniform, PORTAL);
                DrawModel(&boxModel);
            }
            else if (ent.type == AMMO_BOX)
            {
                // Gruda no chão (procura piso a partir de bem alto pra não pegar o teto)
                float floor = ResolveFloorHeight(ent.x, ent.y + 2.0f, ent.z);
                if (floor > -9000.0f) ent.y = floor;
                model = Matrix_Translate(ent.x, ent.y + 0.15f, ent.z) * Matrix_Rotate_Y(currentTime * 2.0f) * Matrix_Scale(ent.scale, ent.scale, ent.scale);
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(g_object_id_uniform, AMMO_BOX);
                DrawModel(&boxModel); // Modelo padrão da caixa
            }
            else if (ent.type == HEALTH_BOX)
            {
                float floor = ResolveFloorHeight(ent.x, ent.y + 2.0f, ent.z);
                if (floor > -9000.0f) ent.y = floor;
                model = Matrix_Translate(ent.x, ent.y + 0.15f, ent.z) * Matrix_Rotate_Y(currentTime * 2.0f) * Matrix_Scale(ent.scale, ent.scale, ent.scale);
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(g_object_id_uniform, HEALTH_BOX);
                DrawModel(&pizzaModel); // Modelo da Caixa de Pizza
            }
            else if (ent.type == BUTTON)
            {
                // Botão de vitória: snap pro chão, mas a busca começa só um
                // pouquinho acima do spawn pra não pegar plataforma de cima
                // numa espiral. Pulsa de leve.
                float floor = ResolveFloorHeight(ent.x, ent.y + 0.1f, ent.z);
                if (floor > -9000.0f) ent.y = floor;
                float pulse = 1.0f + 0.05f * sinf(currentTime * 4.0f);
                model = Matrix_Translate(ent.x, ent.y + 0.05f, ent.z)
                      * Matrix_Scale(ent.scale * pulse, ent.scale * pulse, ent.scale * pulse);
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(g_object_id_uniform, BOX);
                DrawModel(&buttonModel);
            }
        }

        float recoilOffsetL = 0.0f;
        float recoilOffsetR = 0.0f;
        if (recoilTimer > 0.0f)
        {
            recoilTimer += deltaTime;
            if (recoilTimer >= RECOIL_DURATION)
            {
                recoilTimer = 0.0f;
                activeGun = 1 - activeGun;
            }
        }

        if (g_LeftMouseButtonPressed && recoilTimer == 0.0f && g_PlayerHP > 0 && g_PlayerAmmo > 0 && !playerWon)
        {
            recoilTimer += deltaTime;
            muzzleFlashTimer = 0.12f; // ~120ms de flash
            g_PlayerAmmo--; // Consome 1 bala

            Projectile proj;
            proj.active = true;
            proj.t = 0.0f;
            float sideOffset = (activeGun == 0) ? -0.15f : 0.15f;
            proj.p0 = g_CameraPosition + (camera_right_vector * sideOffset) + (camera_up_vector * -0.5f) + (camera_view_vector * 1.5f);
            proj.p3 = g_CameraPosition + (camera_view_vector * 30.0f);
            proj.p1 = proj.p0 + (camera_view_vector * 5.0f) + (camera_up_vector * 1.0f);
            proj.p2 = proj.p3 - (camera_view_vector * 10.0f) + (camera_up_vector * 1.0f);
            g_Projectiles.push_back(proj);
        }

        for (auto &p : g_Projectiles)
        {
            if (!p.active)
                continue;
            p.t += deltaTime * 2.5f;
            if (p.t >= 1.0f)
            {
                p.active = false;
                continue;
            }

            float u = 1.0f - p.t, tt = p.t * p.t, uu = u * u, uuu = uu * u, ttt = tt * p.t;
            glm::vec4 pos = (uuu * p.p0) + (3.0f * uu * p.t * p.p1) + (3.0f * u * tt * p.p2) + (ttt * p.p3);

            if (CheckWallCollision(pos.x, pos.y, pos.z, 0.1f, 0.1f))
            {
                p.active = false;
                continue;
            }

            glm::vec3 bulletSize(0.2f, 0.2f, 0.2f);
            glm::vec3 alienSize(0.5f, 1.0f, 0.5f);
            for (auto &ent : mapEntities)
            {
                if (ent.type == ALIEN)
                {
                    if (CheckAABB(glm::vec3(pos.x, pos.y, pos.z), bulletSize, glm::vec3(ent.x, ent.y, ent.z), alienSize))
                    {
                        p.active = false;
                        ent.hp -= 1;
                        ent.hitFlash = 0.25f; // segundos de pisca-pisca
                        if (ent.hp <= 0)
                        {
                            ent.type = 0; // morre
                        }
                        break;
                    }
                }
            }
            if (!p.active)
                continue;

            glm::vec4 tangent = (3.0f * uu * (p.p1 - p.p0)) + (6.0f * u * p.t * (p.p2 - p.p1)) + (3.0f * tt * (p.p3 - p.p2));
            tangent.w = 0.0f;
            float mag = sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
            float yaw = 0.0f, pitch = 0.0f;
            if (mag > 0.0001f)
            {
                glm::vec4 dir = tangent / mag;
                yaw = atan2(dir.x, dir.z);
                pitch = asin(-dir.y);
            }

            // O Rotate_Y(1.5708f) vira ela em 90 graus caso o .obj original esteja "de lado". Pode ser removido se ela voar torta.
            model = Matrix_Translate(pos.x, pos.y, pos.z) 
                  * Matrix_Rotate_Y(yaw) 
                  * Matrix_Rotate_X(pitch + 1.5708f)
                  * Matrix_Scale(0.05f, 0.05f, 0.05f); 
                  
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            
            // Alterar o ID para não cair na cor pura "Amarelo Fluorescente" que criamos no Shader para o laser
            glUniform1i(g_object_id_uniform, BOX); 
            DrawModel(&bulletModel); // Desenha a bala 3D em vez do cilindro
        }
        g_Projectiles.erase(std::remove_if(g_Projectiles.begin(), g_Projectiles.end(), [](const Projectile &p)
                                           { return !p.active; }),
                            g_Projectiles.end());

        // --- ATUALIZAÇÃO DOS PROJETEIS DOS INIMIGOS ---
        {
            const float ENEMY_PROJECTILE_RADIUS = 0.18f;
            const int ENEMY_PROJECTILE_DAMAGE = 8;
            glm::vec3 playerCenter(g_CameraPosition.x, g_CameraPosition.y - PLAYER_HEIGHT * 0.5f, g_CameraPosition.z);

            for (auto &ep : g_EnemyProjectiles)
            {
                if (!ep.active)
                    continue;
                ep.pos += ep.vel * deltaTime;
                ep.life -= deltaTime;
                if (ep.life <= 0.0f)
                {
                    ep.active = false;
                    continue;
                }

                if (CheckWallCollision(ep.pos.x, ep.pos.y, ep.pos.z, 0.1f, 0.1f))
                {
                    ep.active = false;
                    continue;
                }

                // Colisão com o jogador (cápsula simplificada)
                float pdx = ep.pos.x - playerCenter.x;
                float pdz = ep.pos.z - playerCenter.z;
                float pdy = ep.pos.y - playerCenter.y;
                float r = ENEMY_PROJECTILE_RADIUS + PLAYER_RADIUS;
                if (pdx * pdx + pdz * pdz <= r * r && fabsf(pdy) <= PLAYER_HEIGHT * 0.6f)
                {
                    if (g_PlayerHP > 0)
                    {
                        g_PlayerHP -= ENEMY_PROJECTILE_DAMAGE;
                        if (g_PlayerHP < 0)
                            g_PlayerHP = 0;
                    }
                    ep.active = false;
                    continue;
                }

                // Render: bolinha alongada na direção do movimento
                float vmag = sqrtf(ep.vel.x * ep.vel.x + ep.vel.y * ep.vel.y + ep.vel.z * ep.vel.z);
                float yaw = 0.0f, pitch = 0.0f;
                if (vmag > 0.0001f)
                {
                    glm::vec3 vd = ep.vel / vmag;
                    yaw = atan2f(vd.x, vd.z);
                    pitch = asinf(-vd.y);
                }
                
                model = Matrix_Translate(ep.pos.x, ep.pos.y, ep.pos.z)
                      * Matrix_Rotate_Y(yaw) * Matrix_Rotate_X(pitch)
                      * Matrix_Scale(0.08f, 0.08f, 0.08f);
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(g_object_id_uniform, BOX);
                DrawModel(&projectileModel);
            }
            g_EnemyProjectiles.erase(
                std::remove_if(g_EnemyProjectiles.begin(), g_EnemyProjectiles.end(),
                               [](const EnemyProjectile &e)
                               { return !e.active; }),
                g_EnemyProjectiles.end());
        }

        // ====================================================================
        // ANIMAÇÃO PROCEDURAL DA ARMA (SWAY / BOBBING / RECOIL)
        // ====================================================================
        if (recoilTimer > 0.0f)
        {
            float t = recoilTimer / RECOIL_DURATION;
            float currentRecoil = (t < 0.2f) ? (t / 0.2f) * RECOIL_DISTANCE : ((1.0f - t) / 0.8f) * RECOIL_DISTANCE;
            if (activeGun == 0)
                recoilOffsetL = currentRecoil;
            else
                recoilOffsetR = currentRecoil;
        }

        // 1. Calcula a intensidade do movimento do jogador (0.0 a 1.0)
        float currentSpeed = glm::length(playerVelocityXZ);
        float speedNormalized = std::min(1.0f, currentSpeed / 8.0f); // 8.0f é o MAX_SPEED

        // 2. Animação de respiração (Sway) - Suave, ocorre mesmo parado
        float swayTimer = currentTime * 1.5f;
        float swayX = cos(swayTimer) * 0.02f;
        float swayY = sin(swayTimer * 2.0f) * 0.02f;

        // 3. Animação de Passos (Bobbing) - Rápida e ampla, ocorre ao andar
        float bobTimer = currentTime * 12.0f;      // Frequência da passada
        float bobAmount = 0.15f * speedNormalized; // Multiplicador de intensidade

        // Faz um "oito" perfeito
        float bobX = cos(bobTimer * 0.5f) * bobAmount;
        float bobY = abs(sin(bobTimer)) * bobAmount * 0.5f;

        // 4. Junta tudo no Offset final da arma
        float gunOffsetX = swayX + bobX;
        float gunOffsetY = swayY + bobY;

        glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(Matrix_Identity()));
        glDisable(GL_DEPTH_TEST);

        // Em terceira pessoa, esconde as armas em "viewmodel" (elas pertencem
        // ao corpo do jogador, não à câmera). HUD continua sendo desenhado.
        if (!g_ThirdPerson) {
            // Aplica o Offset calculado (gunOffsetX, gunOffsetY) à translação base da arma esquerda
            model = Matrix_Translate(-0.4f + gunOffsetX, -2.0f + gunOffsetY, -2.0f + recoilOffsetL) * Matrix_Rotate_Y(3.141592f + 0.01f) * Matrix_Rotate_X(0.20f) * Matrix_Scale(2.2f, 2.2f, 2.2f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, GUN);
            DrawModel(&gunModel);

            // Aplica o Offset calculado (gunOffsetX, gunOffsetY) à translação base da arma direita
            model = Matrix_Translate(0.4f + gunOffsetX, -2.0f + gunOffsetY, -2.0f + recoilOffsetR) * Matrix_Rotate_Y(3.141592f - 0.01f) * Matrix_Rotate_X(0.20f) * Matrix_Scale(2.2f, 2.2f, 2.2f);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, GUN);
            DrawModel(&gunModel);
        }

        glEnable(GL_DEPTH_TEST);
        TextRendering_ShowFramesPerSecond(window);

        // HUD: barra de HP estilo Quake
        {
            int win_w, win_h;
            glfwGetWindowSize(window, &win_w, &win_h);
            float aspect = (float)win_w / (float)win_h;

            float pct = (float)g_PlayerHP / (float)g_PlayerMaxHP;
            if (pct < 0.0f)
                pct = 0.0f;
            if (pct > 1.0f)
                pct = 1.0f;

            // Barra: menor, no canto inferior esquerdo
            float marginY = 0.04f;
            float barH = 0.06f;
            float barX = -0.85f;
            float barY = -1.0f + marginY;
            float barW = 0.55f;

            // Fundo com transparência (estilo HUD escuro)
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Fundo da barra
            HUD_DrawRect(barX, barY, barW, barH, 0.05f, 0.06f, 0.08f, 0.85f);

            // Preenchimento azul (mesmo verde-azulado da imagem de referência)
            float fillW = barW * pct;
            HUD_DrawRect(barX, barY, fillW, barH, 0.30f, 0.55f, 0.85f, 0.95f);

            // Borda inferior/superior (linhas finas)
            HUD_DrawRect(barX, barY, barW, 0.005f, 1.0f, 1.0f, 1.0f, 0.45f);
            HUD_DrawRect(barX, barY + barH, barW, 0.005f, 1.0f, 1.0f, 1.0f, 0.25f);

            // Coração à esquerda
            float heartCX = barX - 0.03f;
            float heartCY = barY + barH * 0.5f;
            float heartH = barH * 1.5f;
            HUD_DrawHeart(heartCX, heartCY, heartH, aspect, 0.30f, 0.85f, 0.55f);

            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);

            // 1. Número de HP grande sobre a barra
            char hpStr[16];
            snprintf(hpStr, sizeof(hpStr), "%d", g_PlayerHP);
            TextRendering_PrintString(window, hpStr, barX + 0.012f, barY - 0.012f, 1.8f);

            // 2. Contador de Munição (Posicionado logo acima do HP)
            char ammoStr[32];
            snprintf(ammoStr, sizeof(ammoStr), "MUNICAO: %d / %d", g_PlayerAmmo, g_PlayerMaxAmmo);
            // barY + 0.09f coloca o texto um pouco acima da barra de vida. A escala 1.2f deixa a fonte num tamanho agradável.
            TextRendering_SetColor(1.0f, 1.0f, 1.0f); // branco para munição
            TextRendering_PrintString(window, ammoStr, barX, barY + 0.09f, 1.2f);

            TextRendering_SetColor(0.0f, 0.0f, 0.0f); // restaura preto pro FPS no topo

            if (g_PlayerHP <= 0)
            {
                TextRendering_SetColor(1.0f, 0.2f, 0.2f);
                TextRendering_PrintString(window, "YOU DIED", -0.22f, 0.05f, 3.0f);
                TextRendering_SetColor(0.0f, 0.0f, 0.0f);
            } 
            else if (playerWon) 
            {
                TextRendering_SetColor(0.2f, 1.0f, 0.2f); // Verde
                TextRendering_PrintString(window, "GAME WON!", -0.22f, 0.05f, 3.0f);
                TextRendering_SetColor(0.0f, 0.0f, 0.0f);
            }
        }

        // Tela de início: overlay escuro + título + "PRESS ENTER TO START".
        // Fica por cima de tudo até o ENTER ser pressionado.
        if (!g_GameStarted)
        {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            HUD_DrawRect(-1.0f, -1.0f, 2.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.75f);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);

            TextRendering_SetColor(1.0f, 1.0f, 1.0f);
            TextRendering_PrintString(window, "QUACK",           -0.25f,  0.20f, 4.0f);
            TextRendering_PrintString(window, "PRESS ENTER TO START", -0.30f, -0.10f, 2.0f);
            TextRendering_PrintString(window, "WASD - move    MOUSE - look    LMB - shoot",     -0.45f, -0.35f, 1.2f);
            TextRendering_PrintString(window, "SPACE - jump    C - third person    ESC - quit", -0.45f, -0.45f, 1.2f);
            TextRendering_SetColor(0.0f, 0.0f, 0.0f);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}