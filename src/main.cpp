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

// Definições de IDs
#define WALL  1
#define ALIEN 2
#define BOX   3
#define GUN   4
#define FLOOR 5
#define BULLET 6 // ID para o Projétil Laser
#define PORTAL 7 // ID para o Portal

struct ObjModel {
    tinyobj::attrib_t                 attrib;
    std::vector<tinyobj::shape_t>     shapes;
    std::vector<tinyobj::material_t>  materials;

    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true) {
        printf("Carregando objetos do arquivo \"%s\"...\n", filename);
        std::string fullpath(filename);
        std::string dirname;
        if (basepath == NULL) {
            auto i = fullpath.find_last_of("/");
            if (i != std::string::npos) {
                dirname = fullpath.substr(0, i+1);
                basepath = dirname.c_str();
            }
        }
        std::string warn, err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, basepath, triangulate);
        if (!err.empty()) fprintf(stderr, "\n%s\n", err.c_str());
        if (!ret) throw std::runtime_error("Erro ao carregar modelo.");
        for (size_t shape = 0; shape < shapes.size(); ++shape) {
            if (shapes[shape].name.empty()) throw std::runtime_error("Objeto sem nome.");
            printf("- Objeto interno encontrado: '%s'\n", shapes[shape].name.c_str());
        }
        printf("OK.\n");
    }
};

struct SceneObject {
    std::string  name;        
    size_t       first_index; 
    size_t       num_indices; 
    GLenum       rendering_mode; 
    GLuint       vertex_array_object_id; 
    glm::vec3    bbox_min; 
    glm::vec3    bbox_max;
    GLuint       texture_id;
};

std::map<std::string, SceneObject> g_VirtualScene;
std::stack<glm::mat4>  g_MatrixStack;
std::map<std::string, GLuint> g_TextureCache; 
GLuint g_DefaultTexture = 0;
float g_ScreenRatio = 1.0f;
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false; 
bool g_MiddleMouseButtonPressed = false; 
float g_CameraTheta = 0.0f; 
float g_CameraPhi = 0.0f;   
float g_CameraDistance = 3.5f; 

// ============================================================================
// CONFIGURAÇÕES DA FASE E ENTIDADES
// ============================================================================

float g_MapScale = 0.50f; 

// Localização Exata do Jogador
glm::vec4 g_CameraPosition = glm::vec4(-2.21f, -2.66f, -9.25f, 1.0f);

struct EntitySpawn {
    int type;
    float x, y, z;
    float scale;
};

// Spawns dos Aliens e do Portal (Y Aumentado em +1.0f para caírem de pé)
std::vector<EntitySpawn> mapEntities = {
    { ALIEN,   4.27f, -1.16f, -14.56f, 0.5f },
    { ALIEN,  -0.34f, -1.51f, -21.68f, 0.5f },
    { ALIEN,   6.66f, -1.72f, -25.10f, 0.5f },
    { ALIEN,   6.92f, -1.09f, -33.12f, 0.5f },
    { ALIEN,   5.96f, -0.80f, -30.05f, 0.5f },
    { ALIEN,  -5.96f, -0.02f, -32.90f, 0.5f },
    { ALIEN,  -4.86f,  2.45f, -36.45f, 0.5f },
    { ALIEN,   4.77f,  0.59f, -43.51f, 0.5f },
    { ALIEN,   4.41f,  0.59f, -44.73f, 0.5f },
    { ALIEN,   2.10f,  0.49f, -44.06f, 0.5f },
    { PORTAL,  7.22f,  0.69f, -44.22f, 1.0f } 
};

// --- ESTRUTURA E ARMAZENAMENTO DO PROJÉTIL (BEZIER) ---
struct Projectile {
    bool active;
    float t; 
    glm::vec4 p0, p1, p2, p3; 
};
std::vector<Projectile> g_Projectiles;
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
GLint g_model_uniform, g_view_uniform, g_projection_uniform, g_object_id_uniform, g_bbox_min_uniform, g_bbox_max_uniform;
GLuint g_NumLoadedTextures = 0;

std::string nomeAlien;
std::string nomeBox;
std::string nomeGun;
std::string nomePlane;

void ErrorCallback(int error, const char* description) { fprintf(stderr, "ERROR: GLFW: %s\n", description); }

double g_LastCursorPosX, g_LastCursorPosY;
bool g_FirstMouse = true;

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) g_LeftMouseButtonPressed = (action == GLFW_PRESS);
    if (button == GLFW_MOUSE_BUTTON_RIGHT) g_RightMouseButtonPressed = (action == GLFW_PRESS);
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) g_MiddleMouseButtonPressed = (action == GLFW_PRESS);
}

void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (g_FirstMouse) {
        g_LastCursorPosX = xpos; g_LastCursorPosY = ypos; g_FirstMouse = false;
    }
    float dx = xpos - g_LastCursorPosX;
    float dy = ypos - g_LastCursorPosY;
    g_CameraTheta -= 0.01f * dx; 
    g_CameraPhi   -= 0.01f * dy; 
    float phimax = 3.141592f / 2.0f - 0.01f; 
    float phimin = -phimax;
    if (g_CameraPhi > phimax) g_CameraPhi = phimax;
    if (g_CameraPhi < phimin) g_CameraPhi = phimin;
    g_LastCursorPosX = xpos; g_LastCursorPosY = ypos;
}

void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    g_CameraDistance -= 0.1f*yoffset;
    if (g_CameraDistance < 0.001f) g_CameraDistance = 0.001f;
}

void Correcao_KeyCallback(int key, int action, int mod);

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mod) {
    Correcao_KeyCallback(key, action, mod);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GL_TRUE);
    if (key == GLFW_KEY_P && action == GLFW_PRESS) g_UsePerspectiveProjection = true;
    if (key == GLFW_KEY_O && action == GLFW_PRESS) g_UsePerspectiveProjection = false;
    if (key == GLFW_KEY_H && action == GLFW_PRESS) g_ShowInfoText = !g_ShowInfoText;
    
    if (key == GLFW_KEY_W) g_WPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_S) g_SPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_A) g_APressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_D) g_DPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_SPACE) g_SpacePressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_LEFT_SHIFT) g_ShiftPressed = (action != GLFW_RELEASE);

    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
        g_AngleX = 0.0f; g_AngleY = 0.0f; g_AngleZ = 0.0f;
    }

    if (key == GLFW_KEY_L && action == GLFW_PRESS) {
        printf("\n[LOCALIZACAO] X: %.2f | Y: %.2f | Z: %.2f\n", 
               g_CameraPosition.x, g_CameraPosition.y, g_CameraPosition.z);
    }
}

void PushMatrix(glm::mat4 M); 
void PopMatrix(glm::mat4& M); 
void BuildTrianglesAndAddToVirtualScene(ObjModel* model, const char* basepath = NULL); 
void BuildCylinder(); 
void ComputeNormals(ObjModel* model); 
void LoadShadersFromFiles(); 
GLuint LoadTextureImage(const char* filename); 
void DrawVirtualObject(const char* object_name); 
GLuint LoadShader_Vertex(const char* filename);   
GLuint LoadShader_Fragment(const char* filename); 
void LoadShader(const char* filename, GLuint shader_id); 
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); 
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow* window);
float TextRendering_CharWidth(GLFWwindow* window);
void TextRendering_PrintString(GLFWwindow* window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

// ============================================================================
// SISTEMA DE FÍSICA OTIMIZADO (COLLISION MESH)
// ============================================================================
struct MapTriangle {
    glm::vec3 v0, v1, v2;
    glm::vec3 normal;
    float minX, maxX, minY, maxY, minZ, maxZ;
};
std::vector<MapTriangle> g_MapTriangles;

// Converte a malha pesada do Obj para um formato super-leve de cálculos de física
void BuildPhysicsMesh(const ObjModel& model, float scale) {
    g_MapTriangles.clear();
    
    // Mesmas coordenadas da parede que sumiu
    float delMinX = 1.0f, delMaxX = 3.0f;
    float delMinY = -2.5f, delMaxY = 0.5f;
    float delMinZ = -28.5f, delMaxZ = -27.0f;


    for (const auto& shape : model.shapes) {
        for (size_t f = 0; f < shape.mesh.indices.size() / 3; f++) {
            tinyobj::index_t i0 = shape.mesh.indices[3*f+0], i1 = shape.mesh.indices[3*f+1], i2 = shape.mesh.indices[3*f+2];
            glm::vec3 v0(model.attrib.vertices[3*i0.vertex_index+0], model.attrib.vertices[3*i0.vertex_index+1], model.attrib.vertices[3*i0.vertex_index+2]);
            glm::vec3 v1(model.attrib.vertices[3*i1.vertex_index+0], model.attrib.vertices[3*i1.vertex_index+1], model.attrib.vertices[3*i1.vertex_index+2]);
            glm::vec3 v2(model.attrib.vertices[3*i2.vertex_index+0], model.attrib.vertices[3*i2.vertex_index+1], model.attrib.vertices[3*i2.vertex_index+2]);
            v0 *= scale; v1 *= scale; v2 *= scale;

            // --- FILTRO DE COLISÃO ---
            bool deletarColisao = false;
            glm::vec3 verts[3] = {v0, v1, v2};
            for(int v=0; v<3; v++) {
                if (verts[v].x >= delMinX && verts[v].x <= delMaxX && 
                    verts[v].y >= delMinY && verts[v].y <= delMaxY && 
                    verts[v].z >= delMinZ && verts[v].z <= delMaxZ) {
                    deletarColisao = true;
                    break;
                }
            }
            if(deletarColisao) continue; 
            // -------------------------

            glm::vec3 cross = glm::cross(v1 - v0, v2 - v0);
            if(glm::length(cross) < 0.0001f) continue;
            glm::vec3 normal = glm::normalize(cross);

            MapTriangle tri;
            tri.v0 = v0; tri.v1 = v1; tri.v2 = v2; tri.normal = normal;
            tri.minX = std::min({v0.x, v1.x, v2.x}); tri.maxX = std::max({v0.x, v1.x, v2.x});
            tri.minY = std::min({v0.y, v1.y, v2.y}); tri.maxY = std::max({v0.y, v1.y, v2.y});
            tri.minZ = std::min({v0.z, v1.z, v2.z}); tri.maxZ = std::max({v0.z, v1.z, v2.z});
            
            g_MapTriangles.push_back(tri);
        }
    }
    printf("- Malha de Fisica gerada com %lu triangulos.\n", g_MapTriangles.size());
}

bool CheckAABB(glm::vec3 posA, glm::vec3 sizeA, glm::vec3 posB, glm::vec3 sizeB) {
    bool collisionX = posA.x + sizeA.x >= posB.x - sizeB.x && posB.x + sizeB.x >= posA.x - sizeA.x;
    bool collisionY = posA.y + sizeA.y >= posB.y - sizeB.y && posB.y + sizeB.y >= posA.y - sizeA.y;
    bool collisionZ = posA.z + sizeA.z >= posB.z - sizeB.z && posB.z + sizeB.z >= posA.z - sizeA.z;
    return collisionX && collisionY && collisionZ;
}

float PointToSegmentDistance(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
    glm::vec2 ab = b - a;
    glm::vec2 ap = p - a;
    float dot_ab_ab = glm::dot(ab, ab);
    if (dot_ab_ab <= 0.0001f) return glm::length(p - a);
    float t = glm::dot(ap, ab) / dot_ab_ab;
    t = std::max(0.0f, std::min(1.0f, t));
    glm::vec2 closest = a + t * ab;
    return glm::length(p - closest);
}

// Scanner OTIMIZADO 60 FPS: Encontra o Chão usando as pré-computações
float ResolveFloorHeight(float x, float y_current, float z) {
    float bestY = -9999.0f;
    for (const auto& tri : g_MapTriangles) {
        if (tri.normal.y <= 0.5f) continue; // Pula paredes verticais e tetos
        if (x < tri.minX || x > tri.maxX || z < tri.minZ || z > tri.maxZ) continue; // Pula se estiver longe (Culling O(1))

        float denom = (tri.v1.z - tri.v2.z) * (tri.v0.x - tri.v2.x) + (tri.v2.x - tri.v1.x) * (tri.v0.z - tri.v2.z);
        if (abs(denom) < 0.0001f) continue;
        
        float w1 = ((tri.v1.z - tri.v2.z) * (x - tri.v2.x) + (tri.v2.x - tri.v1.x) * (z - tri.v2.z)) / denom;
        float w2 = ((tri.v2.z - tri.v0.z) * (x - tri.v2.x) + (tri.v0.x - tri.v2.x) * (z - tri.v2.z)) / denom;
        float w3 = 1.0f - w1 - w2;

        if (w1 >= -0.01f && w2 >= -0.01f && w3 >= -0.01f) { 
            float hitY = w1 * tri.v0.y + w2 * tri.v1.y + w3 * tri.v2.y;
            if (hitY > bestY && hitY <= y_current + 1.2f) { // Sobe escadas e rampas
                bestY = hitY;
            }
        }
    }
    return bestY;
}

// Scanner OTIMIZADO 60 FPS: Bate em paredes
bool CheckWallCollision(float x, float y_foot, float z, float radius, float height) {
    glm::vec2 p(x, z);
    float margin = radius * 2.0f;
    
    for (const auto& tri : g_MapTriangles) {
        if (abs(tri.normal.y) >= 0.5f) continue; // Pula chão e teto
        if (y_foot + height < tri.minY || y_foot > tri.maxY) continue; // Pula se estiver alto/baixo demais (Culling)
        if (x + margin < tri.minX || x - margin > tri.maxX || z + margin < tri.minZ || z - margin > tri.maxZ) continue; 

        if (PointToSegmentDistance(p, glm::vec2(tri.v0.x, tri.v0.z), glm::vec2(tri.v1.x, tri.v1.z)) < radius ||
            PointToSegmentDistance(p, glm::vec2(tri.v1.x, tri.v1.z), glm::vec2(tri.v2.x, tri.v2.z)) < radius ||
            PointToSegmentDistance(p, glm::vec2(tri.v2.x, tri.v2.z), glm::vec2(tri.v0.x, tri.v0.z)) < radius) {
            return true; 
        }
    }
    return false;
}

// Verifica colisão do Jogador com Caixas 
bool CheckEntityCollision(glm::vec3 nextPos, float playerRadius, float playerHeight) {
    glm::vec3 pSize(playerRadius, playerHeight/2.0f, playerRadius);
    glm::vec3 pPos(nextPos.x, nextPos.y - playerHeight/2.0f, nextPos.z);

    for (const auto& ent : mapEntities) {
        if (ent.type != 0 && ent.type != PORTAL && ent.type != ALIEN) { 
            glm::vec3 eSize(0.3f, 0.8f, 0.3f);
            if (ent.type == BOX) eSize = glm::vec3(0.5f, 0.5f, 0.5f);
            glm::vec3 ePos(ent.x, ent.y + eSize.y, ent.z); 

            if (CheckAABB(pPos, pSize, ePos, eSize)) return true;
        }
    }
    return false;
}

// ============================================================================
void DrawModel(ObjModel* model) {
    for (size_t i = 0; i < model->shapes.size(); ++i) {
        std::string unique_name = model->shapes[i].name + "_" + std::to_string(i);
        DrawVirtualObject(unique_name.c_str());
    }
}

void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height); g_ScreenRatio = (float)width / height;
}

GLuint LoadTextureImage(const char* filename) {
    if (g_TextureCache.find(filename) != g_TextureCache.end()) return g_TextureCache[filename];
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);
    if ( data == NULL ) { return 0; }
    GLuint texture_id; glGenTextures(1, &texture_id); glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D); stbi_image_free(data); g_TextureCache[filename] = texture_id; 
    return texture_id;
}

void DrawVirtualObject(const char* object_name) {
    if (g_VirtualScene.find(object_name) == g_VirtualScene.end()) return; 
    GLuint tex_id = g_VirtualScene[object_name].texture_id;
    if (tex_id == 0) tex_id = g_DefaultTexture; 
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex_id);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage"), 0);
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);
    glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min; glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f); glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);
    glDrawElements(g_VirtualScene[object_name].rendering_mode, g_VirtualScene[object_name].num_indices, GL_UNSIGNED_INT, (void*)(g_VirtualScene[object_name].first_index * sizeof(GLuint)));
    glBindVertexArray(0);
}

void LoadShadersFromFiles() {
    GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");
    if ( g_GpuProgramID != 0 ) glDeleteProgram(g_GpuProgramID);
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);
    g_model_uniform      = glGetUniformLocation(g_GpuProgramID, "model"); g_view_uniform       = glGetUniformLocation(g_GpuProgramID, "view"); 
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); g_object_id_uniform  = glGetUniformLocation(g_GpuProgramID, "object_id"); 
    g_bbox_min_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_min"); g_bbox_max_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_max");
    glUseProgram(g_GpuProgramID); glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUseProgram(0);
}

void ComputeNormals(ObjModel* model) {
    if ( !model->attrib.normals.empty() ) return;
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
        for (size_t triangle = 0; triangle < num_triangles; ++triangle) sgroup_ids.insert(model->shapes[shape].mesh.smoothing_group_ids[triangle]);
    }
    size_t num_vertices = model->attrib.vertices.size() / 3; model->attrib.normals.reserve( 3*num_vertices );
    for (const unsigned int & sgroup : sgroup_ids) {
        std::vector<int> num_triangles_per_vertex(num_vertices, 0); std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));
        for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
            for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
                if (model->shapes[shape].mesh.smoothing_group_ids[triangle] != sgroup) continue;
                glm::vec4  vertices[3];
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    vertices[vertex] = glm::vec4(model->attrib.vertices[3*idx.vertex_index+0], model->attrib.vertices[3*idx.vertex_index+1], model->attrib.vertices[3*idx.vertex_index+2], 1.0);
                }
                const glm::vec4 n = crossproduct(vertices[1]-vertices[0], vertices[2]-vertices[0]);
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    num_triangles_per_vertex[idx.vertex_index] += 1; vertex_normals[idx.vertex_index] += n;
                }
            }
        }
        std::vector<size_t> normal_indices(num_vertices, 0);
        for (size_t i = 0; i < vertex_normals.size(); ++i) {
            if (num_triangles_per_vertex[i] == 0) continue;
            glm::vec4 n = vertex_normals[i] / (float)num_triangles_per_vertex[i]; n /= norm(n);
            model->attrib.normals.push_back(n.x); model->attrib.normals.push_back(n.y); model->attrib.normals.push_back(n.z);
            normal_indices[i] = (model->attrib.normals.size() / 3) - 1;
        }
        for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
            for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
                if (model->shapes[shape].mesh.smoothing_group_ids[triangle] != sgroup) continue;
                for (size_t vertex = 0; vertex < 3; ++vertex) model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index = normal_indices[model->shapes[shape].mesh.indices[3*triangle + vertex].vertex_index];
            }
        }
    }
}

void BuildTrianglesAndAddToVirtualScene(ObjModel* model, const char* basepath) {
    GLuint vertex_array_object_id; glGenVertexArrays(1, &vertex_array_object_id); glBindVertexArray(vertex_array_object_id);
    std::vector<GLuint> indices; std::vector<float>  model_coefficients; std::vector<float>  normal_coefficients; std::vector<float>  texture_coefficients;
    
    // Coordenadas passadas para deletar a parede:
    // [LOCALIZACAO] X: 1.29 | Y: -2.05 | Z: -27.43
    // [LOCALIZACAO] X: 2.70 | Y: 0.05 | Z: -23.44
    // Vamos criar uma caixa que envolva esses pontos com uma pequena folga
    float delMinX = 1.0f, delMaxX = 3.0f;
    float delMinY = -2.5f, delMaxY = 0.5f;
    float delMinZ = -28.5f, delMaxZ = -27.0f;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
        size_t first_index = indices.size(); size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
        glm::vec3 bbox_min = glm::vec3(std::numeric_limits<float>::max()); glm::vec3 bbox_max = glm::vec3(std::numeric_limits<float>::min());
        
        for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
            
            // --- NOVO: FILTRO PARA DELETAR A PAREDE ---
            bool deletarTriangulo = false;
            for(size_t v_test = 0; v_test < 3; ++v_test) {
                tinyobj::index_t idx_test = model->shapes[shape].mesh.indices[3*triangle + v_test];
                float test_x = model->attrib.vertices[3*idx_test.vertex_index + 0] * g_MapScale;
                float test_y = model->attrib.vertices[3*idx_test.vertex_index + 1] * g_MapScale;
                float test_z = model->attrib.vertices[3*idx_test.vertex_index + 2] * g_MapScale;

                if (test_x >= delMinX && test_x <= delMaxX && 
                    test_y >= delMinY && test_y <= delMaxY && 
                    test_z >= delMinZ && test_z <= delMaxZ) {
                    deletarTriangulo = true;
                    break;
                }
            }
            if(deletarTriangulo) continue; // Pula este triângulo, abrindo um buraco no mapa visual!
            // ------------------------------------------

            for (size_t vertex = 0; vertex < 3; ++vertex) {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                
                // Em vez de adicionar direto, precisamos criar os índices sequencialmente baseados no que sobrou
                // Para não quebrar a ordem do OpenGL, adicionamos "vértices unrolled" (duplicando dados, mas é seguro)
                size_t current_vertex = model_coefficients.size() / 4;
                indices.push_back(current_vertex);

                float vx = model->attrib.vertices[3*idx.vertex_index + 0]; float vy = model->attrib.vertices[3*idx.vertex_index + 1]; float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                model_coefficients.push_back(vx); model_coefficients.push_back(vy); model_coefficients.push_back(vz); model_coefficients.push_back(1.0f);
                bbox_min.x = std::min(bbox_min.x, vx); bbox_min.y = std::min(bbox_min.y, vy); bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx); bbox_max.y = std::max(bbox_max.y, vy); bbox_max.z = std::max(bbox_max.z, vz);
                if ( idx.normal_index != -1 ) { normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 0]); normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 1]); normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 2]); normal_coefficients.push_back(0.0f); }
                if ( idx.texcoord_index != -1 ) { texture_coefficients.push_back(model->attrib.texcoords[2*idx.texcoord_index + 0]); texture_coefficients.push_back(model->attrib.texcoords[2*idx.texcoord_index + 1]); }
            }
        }
        
        // Se a malha inteira foi deletada e sobrou com 0 polígonos, não criamos objeto
        if(indices.size() == first_index) continue;

        std::string unique_name = model->shapes[shape].name + "_" + std::to_string(shape);
        GLuint shape_texture_id = 0;
        if (basepath != NULL && !model->materials.empty() && !model->shapes[shape].mesh.material_ids.empty()) {
            int mat_id = model->shapes[shape].mesh.material_ids[0];
            if (mat_id >= 0 && mat_id < model->materials.size() && !model->materials[mat_id].diffuse_texname.empty()) {
                std::string fullpath = std::string(basepath) + model->materials[mat_id].diffuse_texname;
                shape_texture_id = LoadTextureImage(fullpath.c_str());
            }
        }
        SceneObject theobject; theobject.name = unique_name; theobject.first_index = first_index; theobject.num_indices = indices.size() - first_index; theobject.rendering_mode = GL_TRIANGLES;       
        theobject.vertex_array_object_id = vertex_array_object_id; theobject.bbox_min = bbox_min; theobject.bbox_max = bbox_max; theobject.texture_id = shape_texture_id; g_VirtualScene[unique_name] = theobject;
    }
    GLuint VBO_model_coefficients_id; glGenBuffers(1, &VBO_model_coefficients_id); glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id); glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), model_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(0);
    if ( !normal_coefficients.empty() ) { GLuint VBO_normal_coefficients_id; glGenBuffers(1, &VBO_normal_coefficients_id); glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id); glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), normal_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(1); }
    if ( !texture_coefficients.empty() ) { GLuint VBO_texture_coefficients_id; glGenBuffers(1, &VBO_texture_coefficients_id); glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id); glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), texture_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(2); }
    GLuint indices_id; glGenBuffers(1, &indices_id); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id); glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW); glBindVertexArray(0);
}

void BuildCylinder() {
    std::vector<float>  model_coefficients; std::vector<float>  normal_coefficients; std::vector<float>  texture_coefficients; std::vector<GLuint> indices;
    int sectors = 16; float radius = 1.0f; float height = 1.0f;
    for(int i = 0; i <= sectors; ++i) {
        float theta = 2.0f * 3.14159265359f * float(i) / float(sectors); float cx = radius * cos(theta); float cy = radius * sin(theta);
        model_coefficients.push_back(cx); model_coefficients.push_back(cy); model_coefficients.push_back(height/2.0f); model_coefficients.push_back(1.0f);
        normal_coefficients.push_back(cx/radius); normal_coefficients.push_back(cy/radius); normal_coefficients.push_back(0.0f); normal_coefficients.push_back(0.0f);
        texture_coefficients.push_back(float(i)/sectors); texture_coefficients.push_back(1.0f);
        model_coefficients.push_back(cx); model_coefficients.push_back(cy); model_coefficients.push_back(-height/2.0f); model_coefficients.push_back(1.0f);
        normal_coefficients.push_back(cx/radius); normal_coefficients.push_back(cy/radius); normal_coefficients.push_back(0.0f); normal_coefficients.push_back(0.0f);
        texture_coefficients.push_back(float(i)/sectors); texture_coefficients.push_back(0.0f);
    }
    for(int i = 0; i < sectors; ++i) {
        GLuint front1 = i * 2, back1  = i * 2 + 1, front2 = (i + 1) * 2, back2  = (i + 1) * 2 + 1;
        indices.push_back(front1); indices.push_back(back1); indices.push_back(front2); indices.push_back(back1);  indices.push_back(back2); indices.push_back(front2);
    }
    GLuint vertex_array_object_id; glGenVertexArrays(1, &vertex_array_object_id); glBindVertexArray(vertex_array_object_id);
    GLuint VBO_model_coefficients_id; glGenBuffers(1, &VBO_model_coefficients_id); glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id); glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), model_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(0);
    GLuint VBO_normal_coefficients_id; glGenBuffers(1, &VBO_normal_coefficients_id); glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id); glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), normal_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(1);
    GLuint VBO_texture_coefficients_id; glGenBuffers(1, &VBO_texture_coefficients_id); glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id); glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), texture_coefficients.data(), GL_STATIC_DRAW); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0); glEnableVertexAttribArray(2);
    GLuint indices_id; glGenBuffers(1, &indices_id); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id); glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW); glBindVertexArray(0);
    SceneObject theobject; theobject.name = "laser_cylinder"; theobject.first_index = 0; theobject.num_indices = indices.size(); theobject.rendering_mode = GL_TRIANGLES; theobject.vertex_array_object_id = vertex_array_object_id; theobject.bbox_min = glm::vec3(-radius, -radius, -height/2); theobject.bbox_max = glm::vec3(radius, radius, height/2); theobject.texture_id = 0; g_VirtualScene["laser_cylinder"] = theobject;
}

GLuint LoadShader_Vertex(const char* filename) { GLuint v = glCreateShader(GL_VERTEX_SHADER); LoadShader(filename, v); return v; }
GLuint LoadShader_Fragment(const char* filename) { GLuint f = glCreateShader(GL_FRAGMENT_SHADER); LoadShader(filename, f); return f; }
void LoadShader(const char* filename, GLuint shader_id) {
    std::ifstream file;
    try { file.exceptions(std::ifstream::failbit); file.open(filename); } 
    catch ( std::exception& e ) { fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename); std::exit(EXIT_FAILURE); }
    std::stringstream shader; shader << file.rdbuf(); std::string str = shader.str();
    const GLchar* shader_string = str.c_str(); const GLint length = static_cast<GLint>(str.length());
    glShaderSource(shader_id, 1, &shader_string, &length); glCompileShader(shader_id);
}

GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id) {
    GLuint program_id = glCreateProgram(); glAttachShader(program_id, vertex_shader_id); glAttachShader(program_id, fragment_shader_id); glLinkProgram(program_id); return program_id;
}

void TextRendering_ShowFramesPerSecond(GLFWwindow* window) {
    if ( !g_ShowInfoText ) return;
    static float old_seconds = (float)glfwGetTime(); static int ellapsed_frames = 0; static char buffer[20] = "?? fps";
    ellapsed_frames += 1; float seconds = (float)glfwGetTime();
    if ( seconds - old_seconds > 1.0f ) { snprintf(buffer, 20, "%.2f fps", ellapsed_frames / (seconds - old_seconds)); old_seconds = seconds; ellapsed_frames = 0; }
    TextRendering_PrintString(window, buffer, 1.0f-(7 + 1)*TextRendering_CharWidth(window), 1.0f-TextRendering_LineHeight(window), 1.0f);
}

// ============================================================================
// MAIN LOOP
// ============================================================================
int main(int argc, char* argv[])
{
    int success = glfwInit();
    if (!success) { std::exit(EXIT_FAILURE); }

    glfwSetErrorCallback(ErrorCallback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "INF01047 - Quack", NULL, NULL);
    if (!window) { glfwTerminate(); std::exit(EXIT_FAILURE); }

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); 
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 800, 600); 

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

    // PRÉ-COMPUTA A FÍSICA PARA 60FPS
    BuildPhysicsMesh(quakeMapModel, g_MapScale);

    BuildCylinder();

    TextRendering_Init();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    int activeGun = 0;             
    float recoilTimer = 0.0f;      
    float RECOIL_DURATION = 0.25f; 
    float RECOIL_DISTANCE = 0.6f;  

    // Constantes de Física
    float playerVelocityY = 0.0f;
    const float GRAVITY = -15.0f;
    const float JUMP_FORCE = 6.0f;
    const float PLAYER_HEIGHT = 1.2f; 
    const float PLAYER_RADIUS = 0.3f; 

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(g_GpuProgramID);

        static float lastTime = (float)glfwGetTime();
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        float y = sin(g_CameraPhi);
        float z = cos(g_CameraPhi) * cos(g_CameraTheta);
        float x = cos(g_CameraPhi) * sin(g_CameraTheta);
        
        glm::vec4 camera_view_vector = glm::vec4(x, y, z, 0.0f); 
        glm::vec4 camera_up_vector   = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f); 
        glm::vec4 camera_right_vector = crossproduct(camera_view_vector, camera_up_vector);
        camera_right_vector = camera_right_vector / norm(camera_right_vector); 

        glm::vec3 forward_walk(camera_view_vector.x, 0.0f, camera_view_vector.z);
        if(glm::length(forward_walk) > 0.0f) forward_walk = glm::normalize(forward_walk);

        glm::vec3 right_walk(camera_right_vector.x, 0.0f, camera_right_vector.z);
        if(glm::length(right_walk) > 0.0f) right_walk = glm::normalize(right_walk);

        float speed = 8.0f * deltaTime; 
        
        glm::vec3 nextPos = glm::vec3(g_CameraPosition.x, g_CameraPosition.y, g_CameraPosition.z);

        if (g_WPressed) nextPos += forward_walk * speed;
        if (g_SPressed) nextPos -= forward_walk * speed;
        if (g_APressed) nextPos -= right_walk * speed;
        if (g_DPressed) nextPos += right_walk * speed;

        // FÍSICA 1: Colisão RÁPIDA com as paredes do mapa
        if (!CheckWallCollision(nextPos.x, nextPos.y - PLAYER_HEIGHT, nextPos.z, PLAYER_RADIUS, PLAYER_HEIGHT) &&
            !CheckEntityCollision(nextPos, PLAYER_RADIUS, PLAYER_HEIGHT)) {
            g_CameraPosition.x = nextPos.x;
            g_CameraPosition.z = nextPos.z;
        }

        // FÍSICA 2: Gravidade e Chão Otimizados
        float floorY = ResolveFloorHeight(g_CameraPosition.x, g_CameraPosition.y - PLAYER_HEIGHT, g_CameraPosition.z);
        
        playerVelocityY += GRAVITY * deltaTime;
        float nextFootY = (g_CameraPosition.y + playerVelocityY * deltaTime) - PLAYER_HEIGHT;

        if (nextFootY <= floorY) { 
            g_CameraPosition.y = floorY + PLAYER_HEIGHT;
            playerVelocityY = 0.0f;
            if (g_SpacePressed) playerVelocityY = JUMP_FORCE; 
        } else {
            g_CameraPosition.y += playerVelocityY * deltaTime; 
        }

        glm::mat4 viewMundo = Matrix_Camera_View(g_CameraPosition, camera_view_vector, camera_up_vector);
        glm::mat4 projection = Matrix_Perspective(3.141592 / 3.0f, g_ScreenRatio, -0.1f, -500.0f);

        glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(viewMundo));
        glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(projection));

        glm::mat4 model = Matrix_Identity();

        model = Matrix_Translate(0.0f, 0.0f, 0.0f) * Matrix_Scale(g_MapScale, g_MapScale, g_MapScale);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, WALL); 
        DrawModel(&quakeMapModel);

        // --- ATUALIZANDO INIMIGOS ---
        for (auto& ent : mapEntities) {
            if (ent.type == 0) continue; 

            // FÍSICA DOS ALIENS 
            if (ent.type == ALIEN) {
                float alienFloorY = ResolveFloorHeight(ent.x, ent.y, ent.z);
                
                // NOVIDADE: Cuspida Anti-Clipping (Se o alien entrou na malha por acidente)
                if (ent.y < alienFloorY - 0.1f) {
                    ent.y = alienFloorY + 0.5f; 
                } else if (ent.y > alienFloorY) {
                    ent.y += GRAVITY * deltaTime; 
                    if (ent.y < alienFloorY) ent.y = alienFloorY;
                } else {
                    ent.y = alienFloorY; 
                }

                // IA de Perseguição
                float dirX = g_CameraPosition.x - ent.x;
                float dirZ = g_CameraPosition.z - ent.z;
                float dist = sqrt(dirX * dirX + dirZ * dirZ);
                
                float AGGRO_DISTANCE = 50.0f; 
                float ENEMY_SPEED = 3.0f;     
                float alienBobbingY = 0.0f;   
                float alienWobbleZ = 0.0f;    

                if (dist < AGGRO_DISTANCE) {
                    dirX /= dist; dirZ /= dist;
                    float nX = ent.x + dirX * ENEMY_SPEED * deltaTime;
                    float nZ = ent.z + dirZ * ENEMY_SPEED * deltaTime;
                    
                    if (!CheckWallCollision(nX, ent.y, nZ, 0.3f, 1.0f)) {
                        ent.x = nX;
                        ent.z = nZ;
                    }

                    float runAnimSpeed = 15.0f; 
                    alienBobbingY = abs(sin(currentTime * runAnimSpeed)) * 0.15f; 
                    alienWobbleZ = cos(currentTime * runAnimSpeed * 0.5f) * 0.15f; 
                }
                
                float angle = atan2(dirX, dirZ);
                // OFFSET DE 0.65f ADICIONADO PARA NÃO FICAR ENTERRADO
                model = Matrix_Translate(ent.x, ent.y + alienBobbingY + 0.65f, ent.z) 
                      * Matrix_Rotate_Y(angle + 1.5708f) 
                      * Matrix_Rotate_Z(alienWobbleZ)
                      * Matrix_Scale(ent.scale, ent.scale, ent.scale);
                      
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(g_object_id_uniform, ALIEN);
                DrawModel(&alienModel); 
            }
            else if (ent.type == BOX) {
                float boxFloorY = ResolveFloorHeight(ent.x, ent.y, ent.z);
                if (ent.y > boxFloorY) { ent.y += GRAVITY * deltaTime; if (ent.y < boxFloorY) ent.y = boxFloorY; }
                
                model = Matrix_Translate(ent.x, ent.y + 0.25f, ent.z) * Matrix_Scale(ent.scale, ent.scale, ent.scale);
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(g_object_id_uniform, BOX);
                DrawModel(&boxModel);
            }
            else if (ent.type == PORTAL) {
                model = Matrix_Translate(ent.x, ent.y + 1.0f, ent.z) * Matrix_Scale(1.0f, 2.0f, 0.1f);
                glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                glUniform1i(g_object_id_uniform, PORTAL);
                DrawModel(&boxModel);
            }
        }
        
        float recoilOffsetL = 0.0f; float recoilOffsetR = 0.0f;
        if (recoilTimer > 0.0f) { recoilTimer += deltaTime; if (recoilTimer >= RECOIL_DURATION) { recoilTimer = 0.0f; activeGun = 1 - activeGun; } }

        if (g_LeftMouseButtonPressed && recoilTimer == 0.0f) {
            recoilTimer += deltaTime; 
            Projectile proj; proj.active = true; proj.t = 0.0f;
            float sideOffset = (activeGun == 0) ? -0.15f : 0.15f;
            proj.p0 = g_CameraPosition + (camera_right_vector * sideOffset) + (camera_up_vector * -0.5f) + (camera_view_vector * 1.5f); 
            proj.p3 = g_CameraPosition + (camera_view_vector * 30.0f);
            proj.p1 = proj.p0 + (camera_view_vector * 5.0f) + (camera_up_vector * 1.0f);
            proj.p2 = proj.p3 - (camera_view_vector * 10.0f) + (camera_up_vector * 1.0f);
            g_Projectiles.push_back(proj);
        }

        for (auto& p : g_Projectiles) {
            if (!p.active) continue;
            p.t += deltaTime * 2.5f; 
            if (p.t >= 1.0f) { p.active = false; continue; }
            
            float u = 1.0f - p.t, tt = p.t * p.t, uu = u * u, uuu = uu * u, ttt = tt * p.t;
            glm::vec4 pos = (uuu * p.p0) + (3.0f * uu * p.t * p.p1) + (3.0f * u * tt * p.p2) + (ttt * p.p3);
            
            if (CheckWallCollision(pos.x, pos.y, pos.z, 0.1f, 0.1f)) {
                p.active = false;
                continue;
            }

            glm::vec3 bulletSize(0.2f, 0.2f, 0.2f); glm::vec3 alienSize(0.5f, 1.0f, 0.5f);
            for (auto& ent : mapEntities) {
                if (ent.type == ALIEN) {
                    if (CheckAABB(glm::vec3(pos.x, pos.y, pos.z), bulletSize, glm::vec3(ent.x, ent.y, ent.z), alienSize)) {
                        p.active = false; 
                        ent.type = 0; 
                        break; 
                    }
                }
            }
            if (!p.active) continue;

            glm::vec4 tangent = (3.0f * uu * (p.p1 - p.p0)) + (6.0f * u * p.t * (p.p2 - p.p1)) + (3.0f * tt * (p.p3 - p.p2));
            tangent.w = 0.0f; float mag = sqrt(tangent.x*tangent.x + tangent.y*tangent.y + tangent.z*tangent.z);
            float yaw = 0.0f, pitch = 0.0f;
            if (mag > 0.0001f) { glm::vec4 dir = tangent / mag; yaw = atan2(dir.x, dir.z); pitch = asin(-dir.y); }
            
            model = Matrix_Translate(pos.x, pos.y, pos.z) * Matrix_Rotate_Y(yaw) * Matrix_Rotate_X(pitch) * Matrix_Scale(0.02f, 0.02f, 0.6f); 
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model)); glUniform1i(g_object_id_uniform, BULLET); 
            DrawVirtualObject("laser_cylinder");
        }
        g_Projectiles.erase(std::remove_if(g_Projectiles.begin(), g_Projectiles.end(), [](const Projectile& p) { return !p.active; }), g_Projectiles.end());

        if (recoilTimer > 0.0f) {
            float t = recoilTimer / RECOIL_DURATION; 
            float currentRecoil = (t < 0.2f) ? (t / 0.2f) * RECOIL_DISTANCE : ((1.0f - t) / 0.8f) * RECOIL_DISTANCE;
            if (activeGun == 0) recoilOffsetL = currentRecoil; else recoilOffsetR = currentRecoil;
        }

        glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(Matrix_Identity()));
        glDisable(GL_DEPTH_TEST);

        model = Matrix_Translate(-0.4f, -2.0f, -2.0f + recoilOffsetL) * Matrix_Rotate_Y(3.141592f + 0.01f) * Matrix_Rotate_X(0.20f) * Matrix_Scale(2.2f, 2.2f, 2.2f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model)); glUniform1i(g_object_id_uniform, GUN); DrawModel(&gunModel);

        model = Matrix_Translate(0.4f, -2.0f, -2.0f + recoilOffsetR) * Matrix_Rotate_Y(3.141592f - 0.01f) * Matrix_Rotate_X(0.20f) * Matrix_Scale(2.2f, 2.2f, 2.2f);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model)); glUniform1i(g_object_id_uniform, GUN); DrawModel(&gunModel);

        glEnable(GL_DEPTH_TEST);
        TextRendering_ShowFramesPerSecond(window);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}