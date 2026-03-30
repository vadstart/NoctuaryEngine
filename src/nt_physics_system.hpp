#pragma once

#include "nt_ecs.hpp"
#include "nt_components.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>

// Forward declarations for Jolt types
namespace JPH {
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystemSingleThreaded;
    class CharacterVirtual;
    class CharacterVsCharacterCollisionSimple;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
}

namespace nt {

// Collision layers
namespace PhysicsLayers {
    static constexpr uint8_t STATIC = 0;
    static constexpr uint8_t CHARACTER = 1;
    static constexpr uint8_t NUM_LAYERS = 2;
}

class NtPhysicsSystem : public NtSystem
{
public:
    NtPhysicsSystem(NtNexus* nexus_ptr);
    ~NtPhysicsSystem();

    // Initialize Jolt physics - call once at startup
    void initialize();

    // Update all character controllers - call each frame
    void update(float deltaTime);

    // Cleanup - call before shutdown
    void shutdown();

    // Character controller creation
    void createCharacterController(NtEntity entity);
    void destroyCharacterController(NtEntity entity);

    // Static collider creation
    void createStaticBoxCollider(NtEntity entity, const glm::vec3& halfExtents, const glm::vec3& position,
                                 const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    // Input interface for character movement
    void setCharacterDesiredVelocity(NtEntity entity, const glm::vec3& velocity);
    void triggerJump(NtEntity entity);

    // Query character state
    bool isCharacterGrounded(NtEntity entity) const;

    // Debug visualization
    void setDebugDrawEnabled(bool enabled) { bDebugDraw = enabled; }
    bool isDebugDrawEnabled() const { return bDebugDraw; }
    void drawDebugColliders();

    // Gravity
    void setGravity(const glm::vec3& gravity);
    glm::vec3 getGravity() const { return gravity; }

private:
    NtNexus* nexus;

    // Jolt infrastructure (use pImpl to hide Jolt headers)
    struct JoltState;
    std::unique_ptr<JoltState> jolt;

    // Per-body info for debug drawing
    struct StaticBodyInfo {
        uint32_t bodyIdValue;
        glm::vec3 halfExtents;
        glm::quat rotation;
    };
    std::vector<StaticBodyInfo> staticBodies;

    // Settings
    glm::vec3 gravity{0.0f, -27.0f, 0.0f};
    bool bDebugDraw = false;

    // Internal helpers
    void syncPhysicsToTransform(NtEntity entity);
};

} // namespace nt
