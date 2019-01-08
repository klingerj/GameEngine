#pragma once

#include <chrono>

#include "../scene/MeshDataManager.h"

// Collision information
// TODO: expand this to store info for edge-edge collisions. Right now we are just checking vertex-face.
typedef struct je_collision_info_t {
    glm::vec4 minimumTranslation; // x, y, & z contains the direction, w contains the minimum penetration distance of the two object collision
    glm::vec3 point; // The point on the bounding box that the collision should be applied to 
} JECollisionInfo;
// Note: when checking CollisionInfo, if the w component of the minimumTranslation = -1.0f, it's a no collision

// Class to manage the game's physics update

class JEPhysicsManager {
private:
    std::shared_ptr<JEMeshDataManager> meshDataManager;

    using JE_TIMER = std::chrono::time_point<std::chrono::steady_clock>;
    const JE_TIMER m_startTime; // Start time of the physics system
    double m_currentTime; // Current time of the physics system
    const double m_updateRateInMilliseconds; // in ms
    const float m_updateRateFactor; // For physics integration
    uint32_t frameCtr;

    JECollisionInfo SAT(JE_OBB& obbA, JE_OBB& obbB, uint32_t indexA, uint32_t indexB); // TODO make me const

public:
    JEPhysicsManager() : m_startTime(std::chrono::high_resolution_clock::now()), m_currentTime(0.0), m_updateRateInMilliseconds(16.667), m_updateRateFactor(1.0f / 60.0f), frameCtr(0) {}
    ~JEPhysicsManager() {}

    void Initialize(const std::shared_ptr<JEMeshDataManager>& m);

    // Compute physics on the mesh data
    void Update();
};
