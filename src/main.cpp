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

const int MAP_SIZE = 10;
int gameMap[MAP_SIZE][MAP_SIZE] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 2, 0, 0, 0, 3, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 2, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 3, 0, 0, 0, 2, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

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

// --- CONFIGURAÇÕES DO QUACK ---
// 1. Escala do Mapa (Aumentada para o mapa ficar bem maior)
float g_MapScale = 0.50f; 

// 2. Spawn inicial do Jogador (Ajuste X, Y e Z para nascer onde quiser)
glm::vec4 g_CameraPosition = glm::vec4(0.0f, 2.0f, 5.0f, 1.0f);

// 3. Sistema de Spawn Exato para Itens e Inimigos
struct EntitySpawn {
    int type;
    float x, y, z;
    float scale;
};

// Adicione quantos elementos quiser na posição exata que desejar
std::vector<EntitySpawn> mapEntities = {
    { ALIEN,  10.0f,  0.0f, -5.0f,  0.5f },
    { ALIEN, -15.0f,  0.0f,  8.0f,  0.5f },
    { BOX,     5.0f,  0.25f, 2.0f,  0.5f },
    { BOX,    -2.0f,  0.25f, 12.0f, 0.5f }
};

bool g_WPressed = false;
bool g_APressed = false;
bool g_SPressed = false;
bool g_DPressed = false;
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
}

void PushMatrix(glm::mat4 M); 
void PopMatrix(glm::mat4& M); 
// CORRIGIDO: Adicionado o basepath para ler o .mtl corretamente
void BuildTrianglesAndAddToVirtualScene(ObjModel* model, const char* basepath = NULL); 
void ComputeNormals(ObjModel* model); 
void LoadShadersFromFiles(); 
// CORRIGIDO: A função agora retorna GLuint na declaração para bater com a implementação!
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

void DrawModel(ObjModel* model) {
    for (size_t i = 0; i < model->shapes.size(); ++i) {
        std::string unique_name = model->shapes[i].name + "_" + std::to_string(i);
        DrawVirtualObject(unique_name.c_str());
    }
}

void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    g_ScreenRatio = (float)width / height;
}

GLuint LoadTextureImage(const char* filename) {
    // Se a textura já foi carregada antes, devolvemos ela da memória!
    if (g_TextureCache.find(filename) != g_TextureCache.end()) 
        return g_TextureCache[filename];

    printf("Carregando imagem \"%s\"... ", filename);
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);
    
    if ( data == NULL ) { 
        printf("AVISO: Imagem nao encontrada. O objeto ficara sem textura.\n"); 
        return 0; // Se não achar a imagem, retorna 0 (evita crash do jogo)
    }
    printf("OK (%dx%d).\n", width, height);

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    
    g_TextureCache[filename] = texture_id; // Salva no cache
    return texture_id;
}

void DrawVirtualObject(const char* object_name) {
    if (g_VirtualScene.find(object_name) == g_VirtualScene.end()) return; 

    // --- MAGIA ACONTECENDO AQUI ---
    // Resgatamos a textura exata salva para este pedaço do modelo 3D
    GLuint tex_id = g_VirtualScene[object_name].texture_id;
    if (tex_id == 0) tex_id = g_DefaultTexture; // Usa a textura padrão se der erro
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage"), 0);
    // ------------------------------

    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);
    glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min;
    glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(g_bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(g_bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);
    glDrawElements(g_VirtualScene[object_name].rendering_mode, g_VirtualScene[object_name].num_indices, GL_UNSIGNED_INT, (void*)(g_VirtualScene[object_name].first_index * sizeof(GLuint)));
    glBindVertexArray(0);
}

void LoadShadersFromFiles() {
    GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");
    if ( g_GpuProgramID != 0 ) glDeleteProgram(g_GpuProgramID);
    g_GpuProgramID = CreateGpuProgram(vertex_shader_id, fragment_shader_id);
    g_model_uniform      = glGetUniformLocation(g_GpuProgramID, "model"); 
    g_view_uniform       = glGetUniformLocation(g_GpuProgramID, "view"); 
    g_projection_uniform = glGetUniformLocation(g_GpuProgramID, "projection"); 
    g_object_id_uniform  = glGetUniformLocation(g_GpuProgramID, "object_id"); 
    g_bbox_min_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_min");
    g_bbox_max_uniform   = glGetUniformLocation(g_GpuProgramID, "bbox_max");
    glUseProgram(g_GpuProgramID);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage2"), 2);
    glUniform1i(glGetUniformLocation(g_GpuProgramID, "TextureImage3"), 3);
    glUseProgram(0);
}

void ComputeNormals(ObjModel* model) {
    if ( !model->attrib.normals.empty() ) return;
    std::set<unsigned int> sgroup_ids;
    for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
        for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
            unsigned int sgroup = model->shapes[shape].mesh.smoothing_group_ids[triangle];
            sgroup_ids.insert(sgroup);
        }
    }
    size_t num_vertices = model->attrib.vertices.size() / 3;
    model->attrib.normals.reserve( 3*num_vertices );
    for (const unsigned int & sgroup : sgroup_ids) {
        std::vector<int> num_triangles_per_vertex(num_vertices, 0);
        std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));
        for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
            for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
                unsigned int sgroup_tri = model->shapes[shape].mesh.smoothing_group_ids[triangle];
                if (sgroup_tri != sgroup) continue;
                glm::vec4  vertices[3];
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    vertices[vertex] = glm::vec4(model->attrib.vertices[3*idx.vertex_index+0], model->attrib.vertices[3*idx.vertex_index+1], model->attrib.vertices[3*idx.vertex_index+2], 1.0);
                }
                const glm::vec4 n = crossproduct(vertices[1]-vertices[0], vertices[2]-vertices[0]);
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                    num_triangles_per_vertex[idx.vertex_index] += 1;
                    vertex_normals[idx.vertex_index] += n;
                }
            }
        }
        std::vector<size_t> normal_indices(num_vertices, 0);
        for (size_t i = 0; i < vertex_normals.size(); ++i) {
            if (num_triangles_per_vertex[i] == 0) continue;
            glm::vec4 n = vertex_normals[i] / (float)num_triangles_per_vertex[i];
            n /= norm(n);
            model->attrib.normals.push_back(n.x); model->attrib.normals.push_back(n.y); model->attrib.normals.push_back(n.z);
            normal_indices[i] = (model->attrib.normals.size() / 3) - 1;
        }
        for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
            size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
            for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
                if (model->shapes[shape].mesh.smoothing_group_ids[triangle] != sgroup) continue;
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index = normal_indices[model->shapes[shape].mesh.indices[3*triangle + vertex].vertex_index];
                }
            }
        }
    }
}

void BuildTrianglesAndAddToVirtualScene(ObjModel* model, const char* basepath) {
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);
    std::vector<GLuint> indices;
    std::vector<float>  model_coefficients;
    std::vector<float>  normal_coefficients;
    std::vector<float>  texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape) {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();
        glm::vec3 bbox_min = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 bbox_max = glm::vec3(std::numeric_limits<float>::min());

        for (size_t triangle = 0; triangle < num_triangles; ++triangle) {
            for (size_t vertex = 0; vertex < 3; ++vertex) {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                indices.push_back(first_index + 3*triangle + vertex);
                float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                model_coefficients.push_back(vx); model_coefficients.push_back(vy); model_coefficients.push_back(vz); model_coefficients.push_back(1.0f);
                bbox_min.x = std::min(bbox_min.x, vx); bbox_min.y = std::min(bbox_min.y, vy); bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx); bbox_max.y = std::max(bbox_max.y, vy); bbox_max.z = std::max(bbox_max.z, vz);
                if ( idx.normal_index != -1 ) {
                    normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 0]);
                    normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 1]);
                    normal_coefficients.push_back(model->attrib.normals[3*idx.normal_index + 2]);
                    normal_coefficients.push_back(0.0f);
                }
                if ( idx.texcoord_index != -1 ) {
                    texture_coefficients.push_back(model->attrib.texcoords[2*idx.texcoord_index + 0]);
                    texture_coefficients.push_back(model->attrib.texcoords[2*idx.texcoord_index + 1]);
                }
            }
        }
        size_t last_index = indices.size() - 1;
        std::string unique_name = model->shapes[shape].name + "_" + std::to_string(shape);

        // --- LÊ O ARQUIVO DE MATERIAL E CARREGA A IMAGEM CORRETA DA PASTA ---
        GLuint shape_texture_id = 0;
        if (basepath != NULL && !model->materials.empty() && !model->shapes[shape].mesh.material_ids.empty()) {
            int mat_id = model->shapes[shape].mesh.material_ids[0];
            if (mat_id >= 0 && mat_id < model->materials.size()) {
                std::string texname = model->materials[mat_id].diffuse_texname;
                if (!texname.empty()) {
                    std::string fullpath = std::string(basepath) + texname;
                    shape_texture_id = LoadTextureImage(fullpath.c_str());
                }
            }
        }

        SceneObject theobject;
        theobject.name           = unique_name;
        theobject.first_index    = first_index;
        theobject.num_indices    = indices.size() - first_index;
        theobject.rendering_mode = GL_TRIANGLES;       
        theobject.vertex_array_object_id = vertex_array_object_id;
        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;
        theobject.texture_id = shape_texture_id; // Salva a textura do Quake!
        
        g_VirtualScene[unique_name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    if ( !normal_coefficients.empty() ) {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(1);
    }
    if ( !texture_coefficients.empty() ) {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(2);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    glBindVertexArray(0);
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
    GLuint program_id = glCreateProgram();
    glAttachShader(program_id, vertex_shader_id); glAttachShader(program_id, fragment_shader_id); glLinkProgram(program_id);
    return program_id;
}

void TextRendering_ShowFramesPerSecond(GLFWwindow* window) {
    if ( !g_ShowInfoText ) return;
    static float old_seconds = (float)glfwGetTime();
    static int   ellapsed_frames = 0;
    static char  buffer[20] = "?? fps";
    ellapsed_frames += 1;
    float seconds = (float)glfwGetTime();
    if ( seconds - old_seconds > 1.0f ) {
        snprintf(buffer, 20, "%.2f fps", ellapsed_frames / (seconds - old_seconds));
        old_seconds = seconds; ellapsed_frames = 0;
    }
    TextRendering_PrintString(window, buffer, 1.0f-(7 + 1)*TextRendering_CharWidth(window), 1.0f-TextRendering_LineHeight(window), 1.0f);
}

// ============================================================================
// PARTE 3: MAIN E LÓGICA DO JOGO
// TUDO ACIMA DESTA LINHA NÃO PRECISA MAIS SER ALTERADO!
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

    // Carregamos a textura de tijolos como um fallback padrão
    g_DefaultTexture = LoadTextureImage("../../data/red_brick_diff_1k.jpg");

    // Modelos (Repare no segundo parâmetro apontando para a pasta das texturas)
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

    TextRendering_Init();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    int activeGun = 0;             
    float recoilTimer = 0.0f;      
    float RECOIL_DURATION = 0.25f; 
    float RECOIL_DISTANCE = 0.6f;  

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

        float speed = 8.0f * deltaTime; 
        
        if (g_WPressed) g_CameraPosition += camera_view_vector * speed;
        if (g_SPressed) g_CameraPosition -= camera_view_vector * speed;
        if (g_APressed) g_CameraPosition -= camera_right_vector * speed;
        if (g_DPressed) g_CameraPosition += camera_right_vector * speed;

        glm::mat4 viewMundo = Matrix_Camera_View(g_CameraPosition, camera_view_vector, camera_up_vector);

        glm::mat4 projection;
        float nearplane = -0.1f;  
        float farplane  = -500.0f; // Bem maior para não cortar o mapa do Quake
        float field_of_view = 3.141592 / 3.0f;
        projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);

        glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(viewMundo));
        glUniformMatrix4fv(g_projection_uniform, 1, GL_FALSE, glm::value_ptr(projection));

        // --- DESENHANDO O MAPA DO QUAKE ---
        glm::mat4 model = Matrix_Translate(0.0f, 0.0f, 0.0f) * Matrix_Scale(g_MapScale, g_MapScale, g_MapScale);
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, WALL); 
        DrawModel(&quakeMapModel);

        // --- DESENHANDO INIMIGOS E CAIXAS PELAS COORDENADAS ---
        for (const auto& ent : mapEntities) {
            model = Matrix_Translate(ent.x, ent.y, ent.z) * Matrix_Scale(ent.scale, ent.scale, ent.scale);
            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            
            if (ent.type == ALIEN) {
                glUniform1i(g_object_id_uniform, ALIEN);
                DrawModel(&alienModel); 
            }
            else if (ent.type == BOX) {
                glUniform1i(g_object_id_uniform, BOX);
                DrawModel(&boxModel);
            }
        }

        // ====================================================================
        // RENDERIZAÇÃO DA ARMA COM ANIMAÇÃO DE RECÚO INTERCALADO
        // ====================================================================
        float recoilOffsetL = 0.0f;
        float recoilOffsetR = 0.0f;

        if (recoilTimer > 0.0f) {
            recoilTimer += deltaTime;
            if (recoilTimer >= RECOIL_DURATION) {
                recoilTimer = 0.0f;        
                activeGun = 1 - activeGun; 
            }
        }

        if (g_LeftMouseButtonPressed && recoilTimer == 0.0f) {
            recoilTimer += deltaTime; 
        }

        if (recoilTimer > 0.0f) {
            float t = recoilTimer / RECOIL_DURATION; 
            float currentRecoil = 0.0f;

            if (t < 0.2f) { 
                currentRecoil = (t / 0.2f) * RECOIL_DISTANCE;
            } else { 
                currentRecoil = ((1.0f - t) / 0.8f) * RECOIL_DISTANCE;
            }

            if (activeGun == 0) {
                recoilOffsetL = currentRecoil;
            } else {
                recoilOffsetR = currentRecoil;
            }
        }

        glm::mat4 viewArma = Matrix_Identity(); 
        glUniformMatrix4fv(g_view_uniform, 1, GL_FALSE, glm::value_ptr(viewArma));

        glm::mat4 gunScale = Matrix_Scale(2.2f, 2.2f, 2.2f); 
        float inclinacaoParaCima = 0.20f; 
        
        glDisable(GL_DEPTH_TEST);

        glm::mat4 gunAlignL = Matrix_Rotate_Y(3.141592f + 0.01f) * Matrix_Rotate_X(inclinacaoParaCima); 
        model = Matrix_Translate(-0.4f, -2.0f, -2.0f + recoilOffsetL) * gunAlignL * gunScale;
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, GUN);
        DrawModel(&gunModel);

        glm::mat4 gunAlignR = Matrix_Rotate_Y(3.141592f - 0.01f) * Matrix_Rotate_X(inclinacaoParaCima); 
        model = Matrix_Translate(0.4f, -2.0f, -2.0f + recoilOffsetR) * gunAlignR * gunScale;
        glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(g_object_id_uniform, GUN);
        DrawModel(&gunModel);

        glEnable(GL_DEPTH_TEST);

        TextRendering_ShowFramesPerSecond(window);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}