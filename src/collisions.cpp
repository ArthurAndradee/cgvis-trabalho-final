#include "collisions.h"
#include <algorithm>
#include <cstdio>
#include <glm/geometric.hpp>

// Definições de IDs (replicadas aqui apenas para a lógica da função CheckEntityCollision)
#define ALIEN 2
#define BOX 3
#define PORTAL 7
#define AMMO_BOX 8
#define HEALTH_BOX 9

// A struct precisa estar definida aqui caso não tenha sido importada por outro cabeçalho
struct EntitySpawn {
    int type;
    float x, y, z;
    float scale;
    float hitCooldown;
    int behavior;
    float shootCooldown;
    int hp;
    float hitFlash;
};

// Instância real do vetor de triângulos
std::vector<MapTriangle> g_MapTriangles;

void AddTriangleToPhysicsMesh(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, std::string shapeName)
{
    glm::vec3 cross = glm::cross(v1 - v0, v2 - v0);
    if (glm::length(cross) < 0.0001f)
        return;
    glm::vec3 normal = glm::normalize(cross);

    MapTriangle tri;
    tri.v0 = v0;
    tri.v1 = v1;
    tri.v2 = v2;
    tri.normal = normal;
    tri.minX = std::min({v0.x, v1.x, v2.x});
    tri.maxX = std::max({v0.x, v1.x, v2.x});
    tri.minY = std::min({v0.y, v1.y, v2.y});
    tri.maxY = std::max({v0.y, v1.y, v2.y});
    tri.minZ = std::min({v0.z, v1.z, v2.z});
    tri.maxZ = std::max({v0.z, v1.z, v2.z});
    tri.shapeName = shapeName;

    g_MapTriangles.push_back(tri);
}

bool CheckAABB(glm::vec3 posA, glm::vec3 sizeA, glm::vec3 posB, glm::vec3 sizeB)
{
    bool collisionX = posA.x + sizeA.x >= posB.x - sizeB.x && posB.x + sizeB.x >= posA.x - sizeA.x;
    bool collisionY = posA.y + sizeA.y >= posB.y - sizeB.y && posB.y + sizeB.y >= posA.y - sizeA.y;
    bool collisionZ = posA.z + sizeA.z >= posB.z - sizeB.z && posB.z + sizeB.z >= posA.z - sizeA.z;
    return collisionX && collisionY && collisionZ;
}

float PointToSegmentDistance(glm::vec2 p, glm::vec2 a, glm::vec2 b)
{
    glm::vec2 ab = b - a;
    glm::vec2 ap = p - a;
    float dot_ab_ab = glm::dot(ab, ab);
    if (dot_ab_ab <= 0.0001f)
        return glm::length(p - a);
    float t = glm::dot(ap, ab) / dot_ab_ab;
    t = std::max(0.0f, std::min(1.0f, t));
    glm::vec2 closest = a + t * ab;
    return glm::length(p - closest);
}

bool RayIntersectsTriangle(glm::vec3 rayOrigin, glm::vec3 rayVector, const MapTriangle& inTriangle, float& outDistance) {
    const float EPSILON = 0.0000001;
    glm::vec3 vertex0 = inTriangle.v0;
    glm::vec3 vertex1 = inTriangle.v1;  
    glm::vec3 vertex2 = inTriangle.v2;
    glm::vec3 edge1, edge2, h, s, q;
    float a, f, u, v;
    
    edge1 = vertex1 - vertex0;
    edge2 = vertex2 - vertex0;
    h = glm::cross(rayVector, edge2);
    a = glm::dot(edge1, h);
    
    if (a > -EPSILON && a < EPSILON) return false;
    
    f = 1.0/a;
    s = rayOrigin - vertex0;
    u = f * glm::dot(s, h);
    if (u < 0.0 || u > 1.0) return false;
    
    q = glm::cross(s, edge1);
    v = f * glm::dot(rayVector, q);
    if (v < 0.0 || u + v > 1.0) return false;
    
    float t = f * glm::dot(edge2, q);
    if (t > EPSILON) {
        outDistance = t;
        return true;
    }
    return false;
}

float ResolveFloorHeight(float x, float y_current, float z)
{
    float bestY = -9999.0f;
    for (const auto &tri : g_MapTriangles)
    {
        if (tri.normal.y <= 0.5f) continue;
        if (x < tri.minX || x > tri.maxX || z < tri.minZ || z > tri.maxZ) continue; 

        float denom = (tri.v1.z - tri.v2.z) * (tri.v0.x - tri.v2.x) + (tri.v2.x - tri.v1.x) * (tri.v0.z - tri.v2.z);
        if (abs(denom) < 0.0001f) continue;

        float w1 = ((tri.v1.z - tri.v2.z) * (x - tri.v2.x) + (tri.v2.x - tri.v1.x) * (z - tri.v2.z)) / denom;
        float w2 = ((tri.v2.z - tri.v0.z) * (x - tri.v2.x) + (tri.v0.x - tri.v2.x) * (z - tri.v2.z)) / denom;
        float w3 = 1.0f - w1 - w2;

        if (w1 >= -0.01f && w2 >= -0.01f && w3 >= -0.01f)
        {
            float hitY = w1 * tri.v0.y + w2 * tri.v1.y + w3 * tri.v2.y;
            if (hitY > bestY && hitY <= y_current + 1.2f)
            { 
                bestY = hitY;
            }
        }
    }
    return bestY;
}

bool CheckWallCollision(float x, float y_foot, float z, float radius, float height)
{
    glm::vec2 p(x, z);
    float margin = radius * 2.0f;

    for (const auto &tri : g_MapTriangles)
    {
        if (abs(tri.normal.y) >= 0.5f) continue; 
        if (y_foot + height < tri.minY || y_foot > tri.maxY) continue; 
        if (x + margin < tri.minX || x - margin > tri.maxX || z + margin < tri.minZ || z - margin > tri.maxZ) continue;

        if (PointToSegmentDistance(p, glm::vec2(tri.v0.x, tri.v0.z), glm::vec2(tri.v1.x, tri.v1.z)) < radius ||
            PointToSegmentDistance(p, glm::vec2(tri.v1.x, tri.v1.z), glm::vec2(tri.v2.x, tri.v2.z)) < radius ||
            PointToSegmentDistance(p, glm::vec2(tri.v2.x, tri.v2.z), glm::vec2(tri.v0.x, tri.v0.z)) < radius)
        {
            return true;
        }
    }
    return false;
}

bool CheckWallCollisionStepAware(float x, float y_foot, float z, float radius, float height, float stepClearance)
{
    glm::vec2 p(x, z);
    float margin = radius * 2.0f;

    for (const auto &tri : g_MapTriangles)
    {
        if (abs(tri.normal.y) >= 0.5f) continue;
        if (y_foot + height < tri.minY || y_foot > tri.maxY) continue;
        if (tri.maxY <= y_foot + stepClearance) continue;
        if (x + margin < tri.minX || x - margin > tri.maxX || z + margin < tri.minZ || z - margin > tri.maxZ) continue;

        if (PointToSegmentDistance(p, glm::vec2(tri.v0.x, tri.v0.z), glm::vec2(tri.v1.x, tri.v1.z)) < radius ||
            PointToSegmentDistance(p, glm::vec2(tri.v1.x, tri.v1.z), glm::vec2(tri.v2.x, tri.v2.z)) < radius ||
            PointToSegmentDistance(p, glm::vec2(tri.v2.x, tri.v2.z), glm::vec2(tri.v0.x, tri.v0.z)) < radius)
        {
            return true;
        }
    }
    return false;
}

bool CheckEntityCollision(glm::vec3 nextPos, float playerRadius, float playerHeight, std::vector<EntitySpawn>& mapEntities, int& playerAmmo, const int maxAmmo, int& playerHp, const int maxHp)
{
    glm::vec3 pSize(playerRadius, playerHeight / 2.0f, playerRadius);
    glm::vec3 pPos(nextPos.x, nextPos.y - playerHeight / 2.0f, nextPos.z);

    for (auto &ent : mapEntities)
    {
        if (ent.type == 0 || ent.type == PORTAL || ent.type == ALIEN) continue;

        glm::vec3 eSize(0.3f, 0.8f, 0.3f);
        if (ent.type == BOX) eSize = glm::vec3(0.5f, 0.5f, 0.5f);
        else if (ent.type == AMMO_BOX || ent.type == HEALTH_BOX) eSize = glm::vec3(ent.scale, ent.scale, ent.scale);

        glm::vec3 ePos(ent.x, ent.y + eSize.y, ent.z);

        if (CheckAABB(pPos, pSize, ePos, eSize))
        {
            if (ent.type == AMMO_BOX)
            {
                if (playerAmmo < maxAmmo)
                {
                    playerAmmo += 15; 
                    if (playerAmmo > maxAmmo) playerAmmo = maxAmmo;
                    ent.type = 0; 
                    printf("Pegou Municao! Total: %d\n", playerAmmo);
                }
                return false; 
            }
            else if (ent.type == HEALTH_BOX)
            {
                if (playerHp < maxHp)
                {
                    playerHp += 30; 
                    if (playerHp > maxHp) playerHp = maxHp;
                    ent.type = 0; 
                    printf("Pegou Vida! HP: %d\n", playerHp);
                }
                return false; 
            }

            return true;
        }
    }
    return false;
}