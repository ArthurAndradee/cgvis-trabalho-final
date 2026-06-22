#pragma once

#include <vector>
#include <string>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// Precisamos conhecer a estrutura EntitySpawn para checar as colisões com entidades
struct EntitySpawn;

struct MapTriangle {
    glm::vec3 v0, v1, v2;
    glm::vec3 normal;
    float minX, maxX, minY, maxY, minZ, maxZ;
    std::string shapeName; 
};

// Declaração do vetor global de triângulos da fase (será definido no .cpp)
extern std::vector<MapTriangle> g_MapTriangles;

// Funções de Construção e Matemática Básica
void AddTriangleToPhysicsMesh(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, std::string shapeName);
bool CheckAABB(glm::vec3 posA, glm::vec3 sizeA, glm::vec3 posB, glm::vec3 sizeB);
float PointToSegmentDistance(glm::vec2 p, glm::vec2 a, glm::vec2 b);
bool RayIntersectsTriangle(glm::vec3 rayOrigin, glm::vec3 rayVector, const MapTriangle& inTriangle, float& outDistance);

// Funções de Resolução de Colisão
float ResolveFloorHeight(float x, float y_current, float z);
bool CheckWallCollision(float x, float y_foot, float z, float radius, float height);
bool CheckWallCollisionStepAware(float x, float y_foot, float z, float radius, float height, float stepClearance);
bool CheckEntityCollision(glm::vec3 nextPos, float playerRadius, float playerHeight, std::vector<EntitySpawn>& mapEntities, int& playerAmmo, const int maxAmmo, int& playerHp, const int maxHp);