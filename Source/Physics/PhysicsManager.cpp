#ifndef JOE_ENGINE_SIMD_NONE
#include <immintrin.h>
#endif

#include "glm/gtc/epsilon.hpp"
#include "glm/gtx/norm.hpp"

#include "JoeEngineConfig.h"
#include "PhysicsManager.h"
#include "../Utils/ThreadPool.h"

namespace JoeEngine {
    void JEPhysicsManager::Initialize() {
        m_startTime = std::chrono::high_resolution_clock::now();
    }

    typedef struct particle_update_data_t {
        JEParticleSystem* particleSystem;
        float dt;
        uint32_t startIdx;
        uint32_t endIdx;
        bool complete;
    } ParticleUpdateData;

    // Multithreading functions for particle updates
    void UpdateParticleSystems_MT(void* data) {
        ParticleUpdateData* particleData = (ParticleUpdateData*)data;
        
        std::vector<glm::vec3>& positions = particleData->particleSystem->GetPositionData().GetData();
        std::vector<glm::vec3>& velocities = particleData->particleSystem->GetVelocityData().GetData();
        std::vector<glm::vec3>& accels = particleData->particleSystem->GetAccelData().GetData();
        std::vector<float>& lifetimes = particleData->particleSystem->GetLifetimeData().GetData();

        // Update velocities
        for (uint32_t i = particleData->startIdx; i < particleData->endIdx; ++i) {
            velocities[i] += particleData->dt * accels[i];
        }

        // Update positions
        for (uint32_t i = particleData->startIdx; i < particleData->endIdx; ++i) {
            positions[i] += particleData->dt * velocities[i];
        }

        const float lifetimeDecrement = particleData->dt * 1000;
        for (uint32_t i = particleData->startIdx; i < particleData->endIdx; ++i) {
            lifetimes[i] -= lifetimeDecrement;
        }

        particleData->complete = true;
    }
    
    void JEPhysicsManager::UpdateParticleSystems(std::vector<JEParticleSystem>& particleSystems) {
        JE_TIME currentTime = std::chrono::high_resolution_clock::now();
        const uint32_t elapsedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_startTime).count();
        
        if (elapsedMillis >= m_updateRateMillis) {
            m_startTime = currentTime;

            // Update particles
            for (uint32_t j = 0; j < particleSystems.size(); ++j) {
                JEParticleSystem& particleSystem = particleSystems[j];

                std::vector<glm::vec3>& positions = particleSystem.m_positionData.GetData();
                std::vector<glm::vec3>& velocities = particleSystem.m_velocityData.GetData();
                std::vector<glm::vec3>& accels = particleSystem.m_accelData.GetData();
                std::vector<float>& lifetimes = particleSystem.m_lifetimeData.GetData();

                const bool multithread = true;

                if (multithread) {
                    const uint32_t numParticlesPerGroup = 10000;
                    const uint32_t numGroups = particleSystem.m_settings.numParticles / numParticlesPerGroup;
                    
                    std::vector<ParticleUpdateData> particleUpdateDataList;
                    particleUpdateDataList.reserve(numGroups);
                    for (uint32_t i = 0; i < numGroups; ++i) {
                        ParticleUpdateData particleUpdate;
                        particleUpdate.complete = false;
                        particleUpdate.dt = m_updateDt;
                        particleUpdate.startIdx = i * numParticlesPerGroup;
                        particleUpdate.endIdx = particleUpdate.startIdx + numParticlesPerGroup;
                        particleUpdate.particleSystem = &particleSystem;
                        particleUpdateDataList.push_back(particleUpdate);
                        JEThreadPoolInstance.EnqueueJob({ UpdateParticleSystems_MT, particleUpdateDataList.data() + i });
                    }

                    // Integrate any remaining particles on this thread
                    for (uint32_t i = numParticlesPerGroup * numGroups; i < particleSystem.m_settings.numParticles; ++i) {
                        velocities[i] += accels[i] * m_updateDt;
                    }

                    for (uint32_t i = numParticlesPerGroup * numGroups; i < particleSystem.m_settings.numParticles; ++i) {
                        positions[i] += velocities[i] * m_updateDt;
                    }

                    for (uint32_t i = numParticlesPerGroup * numGroups; i < particleSystem.m_settings.numParticles; ++i) {
                        lifetimes[i] -= m_updateRateMillis;
                    }

                    // Busy-wait for the thread jobs to complete
                    {
                        //ScopedTimer<float> timer("Busy-wait for particle update threads to complete");
                        while (true) {
                            uint32_t numJobsComplete = 0;
                            for (uint32_t i = 0; i < particleUpdateDataList.size(); ++i) {
                                // If any job is not yet complete, start waiting again
                                if (particleUpdateDataList[i].complete) {
                                    ++numJobsComplete;
                                }
                            }

                            // All jobs completed
                            if (numJobsComplete == particleUpdateDataList.size()) {
                                break;
                            }
                        }
                    }
                } else {

                    #ifdef JOE_ENGINE_SIMD_AVX
                    // TODO: AVX implementation for particle updates
                    #endif

                    #ifdef JOE_ENGINE_SIMD_AVX2
                    // Use AVX2 SIMD commanda to integrate multiple particles at a time
                    const uint8_t groupSize = 2;
                    uint32_t numGroups = particleSystem.m_settings.numParticles / groupSize;
                    for (uint32_t i = 0; i < numGroups; ++i) {
                        uint32_t offset = i * groupSize;
                        // Create vector for dt float
                        __m256 dtData = _mm256_set1_ps(m_updateDt);

                        // Copy velocity and acceleration data into registers
                        __m256 velData = _mm256_setr_ps(velocities[offset].x, velocities[offset].y, velocities[offset].z,
                            velocities[offset + 1].x, velocities[offset + 1].y, velocities[offset + 1].z, 0.0f, 0.0f);
                        __m256 accelData = _mm256_setr_ps(accels[offset].x, accels[offset].y, accels[offset].z,
                            accels[offset + 1].x, accels[offset + 1].y, accels[offset + 1].z, 0.0f, 0.0f);

                        // Scale acceleration by dt
                        __m256 accelDt = _mm256_mul_ps(dtData, accelData);
                        // Add result to velocity
                        __m256 velDataUpdated = _mm256_add_ps(accelDt, velData);

                        // Copy position data
                        __m256 posData = _mm256_setr_ps(positions[offset].x, positions[offset].y, positions[offset].z,
                            positions[offset + 1].x, positions[offset + 1].y, positions[offset + 1].z, 0.0f, 0.0f);
                        // Scale velocity by dt
                        __m256 velDt = _mm256_mul_ps(dtData, velDataUpdated);
                        // Add result to position
                        __m256 posDataUpdated = _mm256_add_ps(velDt, posData);

                        // Copy results back to particle system
                        float *velDataPtr = (float*)&velDataUpdated;
                        velocities[offset].x = velDataPtr[0];
                        velocities[offset].y = velDataPtr[1];
                        velocities[offset].z = velDataPtr[2];
                        velocities[offset + 1].x = velDataPtr[3];
                        velocities[offset + 1].y = velDataPtr[4];
                        velocities[offset + 1].z = velDataPtr[5];

                        float *posDataPtr = (float*)&posDataUpdated;
                        positions[offset].x = posDataPtr[0];
                        positions[offset].y = posDataPtr[1];
                        positions[offset].z = posDataPtr[2];
                        positions[offset + 1].x = posDataPtr[3];
                        positions[offset + 1].y = posDataPtr[4];
                        positions[offset + 1].z = posDataPtr[5];
                    }

                    // Integrate leftover particles
                    if (particleSystem.m_settings.numParticles % groupSize != 0) {
                        uint32_t i = numGroups * groupSize;
                        for (; i < particleSystem.m_settings.numParticles; ++i) {
                            velocities[i] += accels[i] * m_updateDt;
                            positions[i] += velocities[i] * m_updateDt;
                        }
                    }

                    // Update particle lifetimes
                    // TODO: make this use SIMD
                    std::vector<float>& lifetimes = particleSystem.m_lifetimeData.GetData();
                    for (uint32_t i = 0; i < particleSystem.m_settings.numParticles; ++i) {
                        lifetimes[i] -= m_updateRateMillis;
                    }
                    #endif

                    #ifdef JOE_ENGINE_SIMD_NONE
                    // Update velocities
                    for (uint32_t i = 0; i < particleSystem.m_settings.numParticles; ++i) {
                        velocities[i] += accels[i] * m_updateDt;
                    }

                    // Update positions
                    for (uint32_t i = 0; i < particleSystem.m_settings.numParticles; ++i) {
                        positions[i] += velocities[i] * m_updateDt;
                    }

                    // Update particle lifetimes
                    std::vector<float>& lifetimes = particleSystem.m_lifetimeData.GetData();
                    for (uint32_t i = 0; i < particleSystem.m_settings.numParticles; ++i) {
                        lifetimes[i] -= m_updateRateMillis;
                    }
                    #endif
                }
            }
        }
    }
}

/*
namespace JoeEngine {
    void JEPhysicsManager::Initialize(const std::shared_ptr<JEMeshDataManager>& m) {
        m_startTime = std::chrono::high_resolution_clock::now();
        m_meshDataManager = m;
    }

    void JEPhysicsManager::Update() {
        const JE_TIMER globalTime = std::chrono::high_resolution_clock::now();
        double currentTimeDelta = std::chrono::duration<double, std::chrono::milliseconds::period>(globalTime - m_startTime).count();
        if (m_currentTime == 0.0) { // Set current time to be 
            m_currentTime = currentTimeDelta;
        }
        if (currentTimeDelta > m_currentTime) {
            auto& meshPhysicsData = m_meshDataManager->GetMeshData_Physics();
            for (uint32_t i = 0; i < m_meshDataManager->GetNumMeshes(); ++i) { // TODO: Process multiple meshes at a time with AVX
                meshPhysicsData.obbs[i].center = meshPhysicsData.positions[i];
                // Integration
                if (!(meshPhysicsData.freezeStates[i] & JE_PHYSICS_FREEZE_POSITION)) {
                    meshPhysicsData.velocities[i] += meshPhysicsData.accelerations[i] * m_updateRateFactor;
                    meshPhysicsData.positions[i] += meshPhysicsData.velocities[i] * m_updateRateFactor;
                }

                // Compute forces
                glm::vec3 force = glm::vec3(0.0f, -9.80665f, 0.0f);
                const float mass = 1.0f; // TODO change to use mass data

                // Inertia Tensor
                glm::mat3 inertiaTensor_body = glm::mat3(1.0f);
                // Hard coded inertia tensor for rectangular prism
                inertiaTensor_body[0][0] = 0.08333333f * mass * (meshPhysicsData.obbs[i].e.y * meshPhysicsData.obbs[i].e.y +
                    meshPhysicsData.obbs[i].e.z * meshPhysicsData.obbs[i].e.z);
                inertiaTensor_body[1][1] = 0.08333333f * mass * (meshPhysicsData.obbs[i].e.x * meshPhysicsData.obbs[i].e.x +
                    meshPhysicsData.obbs[i].e.z * meshPhysicsData.obbs[i].e.z);
                inertiaTensor_body[2][2] = 0.08333333f * mass * (meshPhysicsData.obbs[i].e.x * meshPhysicsData.obbs[i].e.x +
                    meshPhysicsData.obbs[i].e.y * meshPhysicsData.obbs[i].e.y);
                const glm::mat3 inertiaTensor_bodyInverse = glm::inverse(inertiaTensor_body);
                const glm::mat3 inertiaTensor_Inverse = meshPhysicsData.rotations[i] * inertiaTensor_bodyInverse * glm::transpose(meshPhysicsData.rotations[i]);

                // Initialize torque
                glm::vec3 torque = glm::vec3(0.0f, 0.0f, 0.0f);

                glm::vec3 impulse = glm::vec3(0.0f);
                // Compute collisions between each OBB in the scene
                JECollisionInfo collInfo;
                collInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
                collInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
                uint32_t numCollisions = 0;
                for (uint32_t j = 0; j < m_meshDataManager->GetNumMeshes(); ++j) {
                    if ((!(meshPhysicsData.freezeStates[i] & (JE_PHYSICS_FREEZE_POSITION | JE_PHYSICS_FREEZE_ROTATION))) && i != j) {
                        JECollisionInfo collisionInfo = SAT(meshPhysicsData.obbs[i], meshPhysicsData.obbs[j], i, j);
                        if (!(glm::length(glm::vec3(collisionInfo.minimumTranslation)) == 0.0f && collisionInfo.minimumTranslation.w == -1.0f)) {

                            // Rotational impulse
                            const float impulseNum = std::max(0.0f, (-1.5f * glm::dot(meshPhysicsData.velocities[i], glm::vec3(collisionInfo.minimumTranslation))));
                            glm::vec3 cp1 = glm::cross(collisionInfo.point, glm::vec3(collisionInfo.minimumTranslation));
                            glm::vec3 cp2 = inertiaTensor_Inverse * cp1;
                            glm::vec3 cp3 = glm::cross(cp2, collisionInfo.point);
                            glm::vec3 impulseDenom = ((1.0f / meshPhysicsData.masses[i]) * cp3);
                            if (std::fabsf(impulseDenom.x) < 0.02) {
                                impulseDenom.x = 0.0f;
                            }
                            if (std::fabsf(impulseDenom.y) < 0.02) {
                                impulseDenom.y = 0.0f;
                            }
                            if (std::fabsf(impulseDenom.z) < 0.02) {
                                impulseDenom.z = 0.0f;
                            }
                            for (uint32_t k = 0; k < 3; ++k) { // Account for zeroes in the denom
                                if (impulseDenom[k] == 0.0f) {
                                    impulse[k] = 0.0f;
                                } else {
                                    impulse[k] = impulseNum / impulseDenom[k];
                                }
                            }
                            torque += glm::cross(collisionInfo.point, impulse);
                            if (std::isnan(torque.x) || std::isnan(torque.y) || std::isnan(torque.z)) {
                                torque += glm::vec3(1.0f);
                            }

                            // Linear impulse
                            const glm::vec3 momentum = meshPhysicsData.masses[i] * meshPhysicsData.velocities[i];
                            const glm::vec3 linearImpulse = glm::max(0.0f, -1.3f * glm::dot(momentum, glm::vec3(collisionInfo.minimumTranslation))) * glm::vec3(collisionInfo.minimumTranslation) / meshPhysicsData.masses[i];
                            meshPhysicsData.velocities[i] += linearImpulse;

                            // Positional penetration correction
                            meshPhysicsData.positions[i] += glm::vec3(collisionInfo.minimumTranslation) * -collisionInfo.minimumTranslation.w * 1.0f;
                            collInfo.point += collisionInfo.point;
                            collInfo.minimumTranslation += collisionInfo.minimumTranslation;
                            ++numCollisions;
                            //break; // Limits to computing collisions with a maximum of one other object.
                        }
                    }
                }
                if (numCollisions > 0) {
                    collInfo.point /= numCollisions;
                    collInfo.minimumTranslation /= (float)numCollisions;
                }

                // TODO: compute angular velocity, rotation matrix, etc
                if (!(meshPhysicsData.freezeStates[i] & JE_PHYSICS_FREEZE_ROTATION)) {
                    meshPhysicsData.angularMomentums[i] += torque * m_updateRateFactor;
                    meshPhysicsData.angularMomentums[i] *= 0.95f;
                    glm::vec3 angularVelocity = inertiaTensor_Inverse * meshPhysicsData.angularMomentums[i];
                    float angle = std::sqrt(glm::dot(angularVelocity, angularVelocity)) * m_updateRateFactor;
                    glm::vec3 angularComp = 0.4f * -glm::cross(angularVelocity, collInfo.point); //TODO: figure why this is negative'd
                    meshPhysicsData.velocities[i] += angularComp * glm::dot(meshPhysicsData.velocities[i], glm::vec3(collInfo.minimumTranslation));

                    // Update model matrices
                    glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(meshPhysicsData.positions[i]));
                    glm::mat4 rotation = glm::mat4(1.0f);
                    if (std::fabsf(angle) > 0.000000953674316f) { // 1/2^(20)
                        rotation = glm::rotate(glm::mat4(meshPhysicsData.rotations[i]), angle, angularVelocity);
                    }
                    //if (!(glm::length(glm::vec3(collInfo.minimumTranslation)) == 0.0f && collInfo.minimumTranslation.w == -1.0f)) {
                    meshPhysicsData.velocities[i] *= 0.998;
                    //}

                    meshPhysicsData.rotations[i] = glm::mat3(rotation);
                    meshPhysicsData.obbs[i].u[0] = meshPhysicsData.rotations[i][0]; // TODO: this might be wrong w/ the columns idk
                    meshPhysicsData.obbs[i].u[1] = meshPhysicsData.rotations[i][1];
                    meshPhysicsData.obbs[i].u[2] = meshPhysicsData.rotations[i][2];

                    m_meshDataManager->SetModelMatrix(translation * rotation, i);

                    // Update acceleration w/ force
                    glm::vec3 acceleration = force;
                    meshPhysicsData.accelerations[i] = acceleration;
                }
            }
            m_currentTime += m_updateRateInMilliseconds;
            ++m_frameCtr;
        }
    }

    // Checks for intersection between two oriented bounding boxes
    // TODO: change back to const references
    JECollisionInfo JEPhysicsManager::SAT(JE_OBB& obbA, JE_OBB& obbB, uint32_t indexA, uint32_t indexB) {
        JECollisionInfo collisionInfo = {};
        collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -999999.0f);

        const glm::vec3 obbCenterDiff = obbB.center - obbA.center;

        obbA.e *= m_meshDataManager->GetMeshData_Physics().scales[indexA];
        obbB.e *= m_meshDataManager->GetMeshData_Physics().scales[indexB];

        // List of points that are used in a bounding box for collision detection
        // Each point is a potential point of penetration and therefore collision
        // The points are the corners and edge midpoints of a (1, 1, 1)-scale cube
        constexpr uint32_t numOBBPoints = 50;
        const std::array<glm::vec3, numOBBPoints> obbPoints = { glm::vec3(-1.0f, -1.0f, -1.0f), // First, the 8 corners
                                                                glm::vec3(-1.0f, -1.0f,  1.0f),
                                                                glm::vec3(-1.0f,  1.0f, -1.0f),
                                                                glm::vec3(-1.0f,  1.0f,  1.0f),
                                                                glm::vec3(1.0f, -1.0f, -1.0f),
                                                                glm::vec3(1.0f, -1.0f,  1.0f),
                                                                glm::vec3(1.0f,  1.0f, -1.0f),
                                                                glm::vec3(1.0f,  1.0f,  1.0f),
                                                                glm::vec3(1.0f, -1.0f,  0.0f), // Second, midpoints of each edge
                                                                glm::vec3(-1.0f, -1.0f,  0.0f),
                                                                glm::vec3(1.0f,  1.0f,  0.0f),
                                                                glm::vec3(-1.0f,  1.0f,  0.0f),
                                                                glm::vec3(0.0f, -1.0f,  1.0f),
                                                                glm::vec3(0.0f, -1.0f, -1.0f),
                                                                glm::vec3(0.0f,  1.0f,  1.0f),
                                                                glm::vec3(0.0f,  1.0f, -1.0f),
                                                                glm::vec3(1.0f,  0.0f,  1.0f),
                                                                glm::vec3(1.0f,  0.0f, -1.0f),
                                                                glm::vec3(-1.0f,  0.0f,  1.0f),
                                                                glm::vec3(-1.0f,  0.0f, -1.0f), // Third, center of each plane
                                                                glm::vec3(0.0f,  1.0f,  0.0f),
                                                                glm::vec3(0.0f, -1.0f,  0.0f),
                                                                glm::vec3(1.0f,  0.0f,  0.0f),
                                                                glm::vec3(-1.0f,  0.0f,  0.0f),
                                                                glm::vec3(0.0f,  0.0f,  1.0f),
                                                                glm::vec3(0.0f,  0.0f, -1.0f),
                                                                glm::vec3(1.0f, 0.5f, 0.0f), // Fourth, the points on each plane between the center of the plane and the edge midpoints
                                                                glm::vec3(1.0f, -0.5f, 0.0f),
                                                                glm::vec3(1.0f, 0.0f, 0.5f),
                                                                glm::vec3(1.0f, 0.0f, -0.5f),
                                                                glm::vec3(-1.0f, 0.5f, 0.0f),
                                                                glm::vec3(-1.0f, -0.5f, 0.0f),
                                                                glm::vec3(-1.0f, 0.0f, 0.5f),
                                                                glm::vec3(-1.0f, 0.0f, -0.5f),
                                                                glm::vec3(0.0f, 0.5f, 1.0f),
                                                                glm::vec3(0.0f, -0.5f, 1.0f),
                                                                glm::vec3(0.5f, 0.0f, 1.0f),
                                                                glm::vec3(-0.5f, 0.0f, 1.0f),
                                                                glm::vec3(0.0f, 0.5f, -1.0f),
                                                                glm::vec3(0.0f, -0.5f, -1.0f),
                                                                glm::vec3(0.5f, 0.0f, -1.0f),
                                                                glm::vec3(-0.5f, 0.0f, -1.0f),
                                                                glm::vec3(0.5f, 1.0f, 0.0f),
                                                                glm::vec3(-0.5f, 1.0f, 0.0f),
                                                                glm::vec3(0.0f, 1.0f, 0.5f),
                                                                glm::vec3(0.0f, 1.0f, -0.5f),
                                                                glm::vec3(0.5f, -1.0f, 0.0f),
                                                                glm::vec3(-0.5f, -1.0f, 0.0f),
                                                                glm::vec3(0.0f, -1.0f, 0.5f),
                                                                glm::vec3(0.0f, -1.0f, -0.5f) };

        std::array<glm::vec3, numOBBPoints> obbPointsTransformed_A;
        glm::mat4 translationA = glm::translate(glm::mat4(1.0f), obbA.center);
        glm::mat4 transformA = translationA * glm::mat4(glm::vec4(obbA.u[0], 0.0f), glm::vec4(obbA.u[1], 1.0f), glm::vec4(obbA.u[2], 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        for (uint32_t i = 0; i < numOBBPoints; ++i) {
            obbPointsTransformed_A[i] = glm::vec3(m_meshDataManager->GetModelMatrix(indexA) * glm::vec4(obbPoints[i] * obbA.e, 1.0f));
        }

        // Ericson OBB SAT test
        float ra, rb;
        glm::mat3 rot, absRot;

        for (uint32_t i = 0; i < 3; ++i) {
            for (uint32_t j = 0; j < 3; ++j) {
                rot[i][j] = glm::dot(obbA.u[i], obbB.u[j]);
            }
        }
        const glm::vec3 diffProj = glm::vec3(glm::dot(obbCenterDiff, obbA.u[0]), glm::dot(obbCenterDiff, obbA.u[1]), glm::dot(obbCenterDiff, obbA.u[2]));

        for (uint32_t i = 0; i < 3; ++i) {
            for (uint32_t j = 0; j < 3; ++j) {
                absRot[i][j] = std::fabsf(rot[i][j]) + 0.000000953674316f;
            }
        }

        // Test OBB A's axes
        for (uint32_t i = 0; i < 3; ++i) {
            ra = obbA.e[i];
            rb = obbB.e[0] * absRot[i][0] + obbB.e[1] * absRot[i][1] + obbB.e[2] * absRot[i][2];
            if (std::fabsf(diffProj[i]) > ra + rb) {
                collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
                collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
                obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
                obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
                return collisionInfo;
            }
        }

        // Test OBB B's axes
        for (uint32_t i = 0; i < 3; ++i) {
            ra = obbA.e[0] * absRot[0][i] + obbA.e[1] * absRot[1][i] + obbA.e[2] * absRot[2][i];
            rb = obbB.e[i];
            if (std::fabsf(diffProj.x * rot[0][i] +
                diffProj.y * rot[1][i] +
                diffProj.z * rot[2][i]) > ra + rb) {
                collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
                collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
                obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
                obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
                return collisionInfo;
            }
        }

        // Test A0 x B0
        ra = obbA.e[1] * absRot[2][0] + obbA.e[2] * absRot[1][0];
        rb = obbB.e[1] * absRot[0][2] + obbB.e[2] * absRot[0][1];
        if (std::fabsf(diffProj.z * rot[1][0] -
            diffProj.y * rot[2][0]) > ra + rb) {
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }

        // Test A0 x B1
        ra = obbA.e[1] * absRot[2][1] + obbA.e[2] * absRot[1][1];
        rb = obbB.e[0] * absRot[0][2] + obbB.e[2] * absRot[0][0];
        if (std::fabsf(diffProj.z * rot[1][1] -
            diffProj.y * rot[2][1]) > ra + rb) {
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }

        // Test A0 x B2
        ra = obbA.e[1] * absRot[2][2] + obbA.e[2] * absRot[1][2];
        rb = obbB.e[0] * absRot[0][1] + obbB.e[1] * absRot[0][0];
        if (std::fabsf(diffProj.z * rot[1][2] -
            diffProj.y * rot[2][2]) > ra + rb) {
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }

        // Test A1 x B0
        ra = obbA.e[0] * absRot[2][0] + obbA.e[2] * absRot[0][0];
        rb = obbB.e[1] * absRot[1][2] + obbB.e[2] * absRot[1][1];
        if (std::fabsf(diffProj.x * rot[2][0] -
            diffProj.z * rot[0][0]) > ra + rb) {
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }

        // Test A1 x B1
        ra = obbA.e[0] * absRot[2][1] + obbA.e[2] * absRot[0][1];
        rb = obbB.e[0] * absRot[1][2] + obbB.e[2] * absRot[1][0];
        if (std::fabsf(diffProj.x * rot[2][1] -
            diffProj.z * rot[0][1]) > ra + rb) {
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }

        // Test A1 x B2
        ra = obbA.e[0] * absRot[2][2] + obbA.e[2] * absRot[0][2];
        rb = obbB.e[0] * absRot[1][1] + obbB.e[1] * absRot[1][0];
        if (std::fabsf(diffProj.x * rot[2][2] -
            diffProj.z * rot[0][2]) > ra + rb) {
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }

        // Test A2 x B0
        ra = obbA.e[0] * absRot[1][0] + obbA.e[1] * absRot[0][0];
        rb = obbB.e[1] * absRot[2][2] + obbB.e[2] * absRot[2][1];
        if (std::fabsf(diffProj.y * rot[0][0] -
            diffProj.x * rot[1][0]) > ra + rb) {
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }

        // Test A2 x B1
        ra = obbA.e[0] * absRot[1][1] + obbA.e[1] * absRot[0][1];
        rb = obbB.e[0] * absRot[2][2] + obbB.e[2] * absRot[2][0];
        if (std::fabsf(diffProj.y * rot[0][1] -
            diffProj.x * rot[1][1]) > ra + rb) {
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }

        // Test A2 x B2
        ra = obbA.e[0] * absRot[1][2] + obbA.e[1] * absRot[0][2];
        rb = obbB.e[0] * absRot[2][1] + obbB.e[1] * absRot[2][0];
        if (std::fabsf(diffProj.y * rot[0][2] -
            diffProj.x * rot[1][2]) > ra + rb) {
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }

        // If we made it this far, the two OBB's must be colliding
        // So, we will compute the minimum translation amount and direction
        // This involves iterating over all 6 planes of OBB B and checking for OBB A's penetration into OBB B
        const std::array<glm::vec3, 6> planePoints = { obbB.center + obbB.u[0] * obbB.e.x,
                                                       obbB.center - obbB.u[0] * obbB.e.x,
                                                       obbB.center + obbB.u[1] * obbB.e.y,
                                                       obbB.center - obbB.u[1] * obbB.e.y,
                                                       obbB.center + obbB.u[2] * obbB.e.z,
                                                       obbB.center - obbB.u[2] * obbB.e.z };

        glm::vec3 directions = glm::vec3(0.0f);
        glm::vec3 points = glm::vec3(0.0f);
        uint32_t numDirs = 0;
        uint32_t numPts = 0;

        std::array<uint32_t, numOBBPoints> ptsInOrOut;
        for (uint32_t o = 0; o < numOBBPoints; ++o) { ptsInOrOut[o] = true; }
        for (uint32_t i = 0; i < 6; ++i) {
            const uint32_t odd = i & 0x1;
            const glm::vec3& currentPlanePoint = planePoints[i] * m_meshDataManager->GetMeshData_Physics().scales[indexB];
            const glm::vec3 planeNormal = glm::normalize(currentPlanePoint - obbB.center);

            // Check each point in OBB A for penetration into B
            for (uint32_t j = 0; j < numOBBPoints; ++j) {
                const glm::vec3 point = obbPointsTransformed_A[j] - currentPlanePoint;
                const float proj = glm::dot(point, planeNormal);

                if (proj > 0.05f) {
                    ptsInOrOut[j] = false;
                }
            }
        }

        for (uint32_t i = 0; i < 6; ++i) {
            const uint32_t odd = i & 0x1;
            const glm::vec3& currentPlanePoint = planePoints[i] * m_meshDataManager->GetMeshData_Physics().scales[indexB];
            const glm::vec3 planeNormal = glm::normalize(currentPlanePoint - obbB.center);

            // Check each point in OBB A for penetration into B
            for (uint32_t j = 0; j < numOBBPoints; ++j) {
                const glm::vec3 point = obbPointsTransformed_A[j] - currentPlanePoint;
                const float proj = glm::dot(point, planeNormal);

                if (!ptsInOrOut[j]) { continue; }

                // If point is on the back side of the plane and is "more positive" than the previous stored min
                if ((proj < 0.0f) && (proj > collisionInfo.minimumTranslation.w)) {
                    collisionInfo.minimumTranslation = glm::vec4(planeNormal, proj);
                    collisionInfo.point = currentPlanePoint;
                }

            }
        }
        //obbA.e /= meshDataManager->GetMeshData_Physics().scales[indexA];
        //obbB.e /= meshDataManager->GetMeshData_Physics().scales[indexB];
        //return collisionInfo;

        // Only intersect with planes that oppose the velocity sufficiently
        if (glm::dot(glm::vec3(collisionInfo.minimumTranslation), m_meshDataManager->GetMeshData_Physics().velocities[indexA]) > 0.9f) {
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }

        // Now that we have the point of minimum penetration, find all points that are really close to that point, and average their
        // positions for the contact point
        glm::vec3 contactPoint = glm::vec3(0.0f, 0.0f, 0.0f);
        uint32_t numContactPoints = 0;
        //const glm::mat3 rotB_inv = glm::inverse(glm::mat3(obbB.u[0], obbB.u[1], obbB.u[2]));
        for (uint32_t j = 0; j < numOBBPoints; ++j) {
            // Check if the point of OBB A is even inside of OBB B
            if (!ptsInOrOut[j]) { continue; }

            const glm::vec3 point = obbPointsTransformed_A[j] - collisionInfo.point;
            const float proj = glm::dot(point, glm::vec3(collisionInfo.minimumTranslation));

            // If point is on the back side of the plane and is "more positive" than the previous stored min
            if (std::fabsf(proj - collisionInfo.minimumTranslation.w) < 0.065f) {
                contactPoint += obbPointsTransformed_A[j] - obbA.center;
                ++numContactPoints;
            }
        }
        if (numContactPoints == 0) { // Not good enough resolution of the points, pretend we didn't collide
            collisionInfo.minimumTranslation = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
            collisionInfo.point = glm::vec3(0.0f, 0.0f, 0.0f);
            obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
            obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
            return collisionInfo;
        }
        contactPoint /= (float)numContactPoints;
        if (std::isnan(contactPoint.x) || std::isnan(contactPoint.y) || std::isnan(contactPoint.z)) {
            contactPoint += glm::vec3(0.0f);
        }
        collisionInfo.point = contactPoint;

        // Return the collision result otherwise.
        // The collision result contains the minimum penetration information necessary to apply the impulse response
        // TODO: Maybe come back to this if we find that ground collisions are visually shaky.
        obbA.e /= m_meshDataManager->GetMeshData_Physics().scales[indexA];
        obbB.e /= m_meshDataManager->GetMeshData_Physics().scales[indexB];
        return collisionInfo;
    }
}
*/