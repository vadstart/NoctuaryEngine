#include "nt_physics_system.hpp"
#include "nt_log.hpp"

#include <glm/gtc/quaternion.hpp>

// im3d for debug visualization
#include <im3d/im3d.h>

// Jolt includes - must come before using JPH namespace
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

// Disable warning about unused parameters in Jolt
JPH_SUPPRESS_WARNINGS

using namespace JPH;
using namespace JPH::literals;

namespace nt {

//==============================
// Broad Phase Layer Interface
//==============================
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        mObjectToBroadPhase[PhysicsLayers::STATIC] = BroadPhaseLayer(0);
        mObjectToBroadPhase[PhysicsLayers::CHARACTER] = BroadPhaseLayer(1);
    }

    virtual uint GetNumBroadPhaseLayers() const override
    {
        return PhysicsLayers::NUM_LAYERS;
    }

    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < PhysicsLayers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
    {
        switch ((BroadPhaseLayer::Type)inLayer)
        {
        case 0: return "STATIC";
        case 1: return "CHARACTER";
        default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif

private:
    BroadPhaseLayer mObjectToBroadPhase[PhysicsLayers::NUM_LAYERS];
};

//==============================
// Object vs Broad Phase Layer Filter
//==============================
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
        case PhysicsLayers::STATIC:
            return inLayer2 == BroadPhaseLayer(1); // Static collides with characters
        case PhysicsLayers::CHARACTER:
            return true; // Characters collide with everything
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

//==============================
// Object Layer Pair Filter
//==============================
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
{
public:
    virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
        case PhysicsLayers::STATIC:
            return inObject2 == PhysicsLayers::CHARACTER;
        case PhysicsLayers::CHARACTER:
            return true; // Characters collide with everything
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

//==============================
// Jolt State (pImpl)
//==============================
struct NtPhysicsSystem::JoltState
{
    std::unique_ptr<TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JobSystemSingleThreaded> jobSystem;
    std::unique_ptr<::JPH::PhysicsSystem> physicsSystem;  // Use global namespace

    // Layer interfaces (must outlive PhysicsSystem)
    std::unique_ptr<BPLayerInterfaceImpl> broadPhaseLayerInterface;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> objectVsBroadPhaseFilter;
    std::unique_ptr<ObjectLayerPairFilterImpl> objectLayerPairFilter;

    // Character collision handler
    std::unique_ptr<CharacterVsCharacterCollisionSimple> characterCollisionHandler;

    // Track created characters for cleanup
    std::vector<Ref<CharacterVirtual>> characters;
};

//==============================
// NtPhysicsSystem Implementation
//==============================

NtPhysicsSystem::NtPhysicsSystem(NtNexus* nexus_ptr)
    : nexus(nexus_ptr)
    , jolt(std::make_unique<JoltState>())
{
}

NtPhysicsSystem::~NtPhysicsSystem()
{
    shutdown();
}

void NtPhysicsSystem::initialize()
{
    // Register default allocator
    RegisterDefaultAllocator();

    // Create factory (required for serialization)
    Factory::sInstance = new Factory();

    // Register physics types
    RegisterTypes();

    // Create temp allocator (10 MB)
    jolt->tempAllocator = std::make_unique<TempAllocatorImpl>(10 * 1024 * 1024);

    // Create single-threaded job system
    jolt->jobSystem = std::make_unique<JobSystemSingleThreaded>(cMaxPhysicsJobs);

    // Create layer interfaces
    jolt->broadPhaseLayerInterface = std::make_unique<BPLayerInterfaceImpl>();
    jolt->objectVsBroadPhaseFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    jolt->objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();

    // Create physics system
    jolt->physicsSystem = std::make_unique<::JPH::PhysicsSystem>();

    const uint cMaxBodies = 1024;
    const uint cNumBodyMutexes = 0; // Auto-detect
    const uint cMaxBodyPairs = 1024;
    const uint cMaxContactConstraints = 1024;

    jolt->physicsSystem->Init(
        cMaxBodies,
        cNumBodyMutexes,
        cMaxBodyPairs,
        cMaxContactConstraints,
        *jolt->broadPhaseLayerInterface,
        *jolt->objectVsBroadPhaseFilter,
        *jolt->objectLayerPairFilter
    );

    jolt->physicsSystem->SetGravity(Vec3(gravity.x, gravity.y, gravity.z));

    // Create character collision handler
    jolt->characterCollisionHandler = std::make_unique<CharacterVsCharacterCollisionSimple>();

    NT_LOG_INFO(LogPhysics, "Jolt Physics initialized");
}

void NtPhysicsSystem::shutdown()
{
    if (!jolt->physicsSystem)
        return;

    // Remove all characters from collision handler
    for (auto& character : jolt->characters)
    {
        if (character)
        {
            jolt->characterCollisionHandler->Remove(character.GetPtr());
        }
    }
    jolt->characters.clear();

    // Cleanup in reverse order
    jolt->characterCollisionHandler.reset();
    jolt->physicsSystem.reset();
    jolt->jobSystem.reset();
    jolt->tempAllocator.reset();
    jolt->objectLayerPairFilter.reset();
    jolt->objectVsBroadPhaseFilter.reset();
    jolt->broadPhaseLayerInterface.reset();

    // Cleanup Jolt globals
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;

    NT_LOG_INFO(LogPhysics, "Jolt Physics shutdown");
}

void NtPhysicsSystem::createCharacterController(NtEntity entity)
{
    if (!nexus->HasComponent<cCharacterPhysics>(entity))
    {
        NT_LOG_ERROR(LogPhysics, "Entity {} does not have cCharacterPhysics component", entity);
        return;
    }

    auto& charPhys = nexus->GetComponent<cCharacterPhysics>(entity);
    auto& transform = nexus->GetComponent<cTransform>(entity);

    // Create capsule shape.
    // capsuleHalfHeight = half of total capsule height (feet to top).
    // centerY offsets the capsule center above the shape origin (feet).
    // joltHalfHeight is the cylinder-only half-height that CapsuleShape expects.
    float centerY = charPhys.capsuleHalfHeight;
    float joltHalfHeight = charPhys.capsuleHalfHeight - charPhys.capsuleRadius;

    RefConst<Shape> standingShape = RotatedTranslatedShapeSettings(
        Vec3(0, centerY, 0),
        Quat::sIdentity(),
        new CapsuleShape(joltHalfHeight, charPhys.capsuleRadius)
    ).Create().Get();

    // Create character settings
    Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
    settings->mMaxSlopeAngle = DegreesToRadians(charPhys.maxSlopeAngle);
    settings->mMaxStrength = 100.0f;
    settings->mMass = 70.0f;
    settings->mShape = standingShape;
    settings->mBackFaceMode = EBackFaceMode::CollideWithBackFaces;
    settings->mCharacterPadding = 0.02f;
    settings->mPenetrationRecoverySpeed = 1.0f;
    settings->mPredictiveContactDistance = 0.1f;

    // Accept contacts that touch the lower hemisphere of the capsule
    settings->mSupportingVolume = Plane(Vec3::sAxisY(), -charPhys.capsuleRadius);

    // Create character at current transform position
    RVec3 position(transform.translation.x, transform.translation.y, transform.translation.z);

    Ref<CharacterVirtual> character = new CharacterVirtual(
        settings,
        position,
        Quat::sIdentity(),
        0,
        jolt->physicsSystem.get()
    );

    // Register for character-vs-character collision
    character->SetCharacterVsCharacterCollision(jolt->characterCollisionHandler.get());
    jolt->characterCollisionHandler->Add(character.GetPtr());

    // Store raw pointer in component (Ref keeps it alive in jolt->characters)
    charPhys.character = character.GetPtr();
    jolt->characters.push_back(character);

    NT_LOG_INFO(LogPhysics, "Created character controller for entity {} at ({}, {}, {})",
        entity, transform.translation.x, transform.translation.y, transform.translation.z);
}

void NtPhysicsSystem::destroyCharacterController(NtEntity entity)
{
    if (!nexus->HasComponent<cCharacterPhysics>(entity))
        return;

    auto& charPhys = nexus->GetComponent<cCharacterPhysics>(entity);
    if (!charPhys.character)
        return;

    // Remove from collision handler
    jolt->characterCollisionHandler->Remove(charPhys.character);

    // Find and remove from our tracking vector
    for (auto it = jolt->characters.begin(); it != jolt->characters.end(); ++it)
    {
        if (it->GetPtr() == charPhys.character)
        {
            jolt->characters.erase(it);
            break;
        }
    }

    charPhys.character = nullptr;
}

void NtPhysicsSystem::createStaticBoxCollider(NtEntity entity, const glm::vec3& halfExtents, const glm::vec3& position, const glm::quat& rotation)
{
    BodyInterface& bodyInterface = jolt->physicsSystem->GetBodyInterface();

    BoxShapeSettings boxSettings(Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
    ShapeSettings::ShapeResult shapeResult = boxSettings.Create();

    if (shapeResult.HasError())
    {
        NT_LOG_ERROR(LogPhysics, "Failed to create box shape: {}", shapeResult.GetError().c_str());
        return;
    }

    BodyCreationSettings bodySettings(
        shapeResult.Get(),
        RVec3(position.x, position.y, position.z),
        Quat(rotation.x, rotation.y, rotation.z, rotation.w),
        EMotionType::Static,
        PhysicsLayers::STATIC
    );

    Body* body = bodyInterface.CreateBody(bodySettings);
    if (!body)
    {
        NT_LOG_ERROR(LogPhysics, "Failed to create static body");
        return;
    }

    bodyInterface.AddBody(body->GetID(), EActivation::DontActivate);

    // Track per-body info for debug drawing
    staticBodies.push_back({ body->GetID().GetIndexAndSequenceNumber(), halfExtents, rotation });

    NT_LOG_INFO(LogPhysics, "Created static box collider at ({}, {}, {}) with half-extents ({}, {}, {})",
        position.x, position.y, position.z,
        halfExtents.x, halfExtents.y, halfExtents.z);
}

void NtPhysicsSystem::update(float deltaTime)
{
    // Update all character controllers
    for (auto entity : entities)
    {
        if (!nexus->HasComponent<cCharacterPhysics>(entity))
            continue;

        auto& charPhys = nexus->GetComponent<cCharacterPhysics>(entity);
        if (!charPhys.character)
            continue;

        CharacterVirtual* character = charPhys.character;

        // Update ground velocity (for moving platforms)
        character->UpdateGroundVelocity();

        // Get current state
        CharacterVirtual::EGroundState groundState = character->GetGroundState();
        charPhys.isGrounded = (groundState == CharacterVirtual::EGroundState::OnGround);

        Vec3 currentVelocity = character->GetLinearVelocity();
        Vec3 groundVelocity = character->GetGroundVelocity();

        // Calculate current vertical velocity
        float currentVerticalVelocity = currentVelocity.Dot(Vec3::sAxisY());

        // Build new velocity
        Vec3 newVelocity;
        bool movingTowardsGround = (currentVerticalVelocity - groundVelocity.GetY()) < 0.1f;

        if (charPhys.isGrounded && movingTowardsGround)
        {
            // On ground - use ground velocity as base
            newVelocity = groundVelocity;

            // Handle jump
            if (charPhys.wantsJump)
            {
                newVelocity += Vec3(0, charPhys.jumpSpeed, 0);
                charPhys.wantsJump = false;
            }
        }
        else
        {
            // In air - preserve vertical velocity
            newVelocity = Vec3(0, currentVerticalVelocity, 0);
        }

        // Apply gravity
        newVelocity += jolt->physicsSystem->GetGravity() * deltaTime;

        // Add horizontal input velocity
        newVelocity += Vec3(charPhys.desiredVelocity.x, 0, charPhys.desiredVelocity.z);

        // Set velocity
        character->SetLinearVelocity(newVelocity);

        // Configure stair/floor settings
        CharacterVirtual::ExtendedUpdateSettings updateSettings;
        updateSettings.mStickToFloorStepDown = Vec3(
            charPhys.stickToFloorStepDown.x,
            charPhys.stickToFloorStepDown.y,
            charPhys.stickToFloorStepDown.z
        );
        updateSettings.mWalkStairsStepUp = Vec3(
            charPhys.walkStairsStepUp.x,
            charPhys.walkStairsStepUp.y,
            charPhys.walkStairsStepUp.z
        );
        updateSettings.mWalkStairsMinStepForward = charPhys.walkStairsMinStepForward;
        updateSettings.mWalkStairsStepForwardTest = charPhys.walkStairsStepForwardTest;

        // Update character (handles collision, movement, contacts)
        character->ExtendedUpdate(
            deltaTime,
            jolt->physicsSystem->GetGravity(),
            updateSettings,
            jolt->physicsSystem->GetDefaultBroadPhaseLayerFilter(PhysicsLayers::CHARACTER),
            jolt->physicsSystem->GetDefaultLayerFilter(PhysicsLayers::CHARACTER),
            {},  // Body filter
            {},  // Shape filter
            *jolt->tempAllocator
        );

        // Sync physics position back to transform
        syncPhysicsToTransform(entity);

        // Clear desired velocity for next frame
        charPhys.desiredVelocity = glm::vec3(0.0f);
    }
}

void NtPhysicsSystem::syncPhysicsToTransform(NtEntity entity)
{
    auto& charPhys = nexus->GetComponent<cCharacterPhysics>(entity);
    auto& transform = nexus->GetComponent<cTransform>(entity);

    RVec3 pos = charPhys.character->GetPosition();
    transform.translation = glm::vec3(
        static_cast<float>(pos.GetX()),
        static_cast<float>(pos.GetY()),
        static_cast<float>(pos.GetZ())
    );
}

void NtPhysicsSystem::setCharacterDesiredVelocity(NtEntity entity, const glm::vec3& velocity)
{
    if (nexus->HasComponent<cCharacterPhysics>(entity))
    {
        nexus->GetComponent<cCharacterPhysics>(entity).desiredVelocity = velocity;
    }
}

void NtPhysicsSystem::triggerJump(NtEntity entity)
{
    if (nexus->HasComponent<cCharacterPhysics>(entity))
    {
        auto& charPhys = nexus->GetComponent<cCharacterPhysics>(entity);
        if (charPhys.isGrounded)
        {
            charPhys.wantsJump = true;
        }
    }
}

bool NtPhysicsSystem::isCharacterGrounded(NtEntity entity) const
{
    if (nexus->HasComponent<cCharacterPhysics>(entity))
    {
        return nexus->GetComponent<cCharacterPhysics>(entity).isGrounded;
    }
    return false;
}

void NtPhysicsSystem::setGravity(const glm::vec3& newGravity)
{
    gravity = newGravity;
    if (jolt->physicsSystem)
    {
        jolt->physicsSystem->SetGravity(Vec3(gravity.x, gravity.y, gravity.z));
    }
}

void NtPhysicsSystem::drawDebugColliders()
{
    if (!bDebugDraw)
        return;

    // Draw character capsules (green)
    Im3d::PushColor(Im3d::Color_Green);
    for (NtEntity entity : entities)
    {
        if (!nexus->HasComponent<cCharacterPhysics>(entity))
            continue;

        auto& charPhys = nexus->GetComponent<cCharacterPhysics>(entity);
        if (!charPhys.character)
            continue;

        RVec3 pos = charPhys.character->GetPosition();
        float radius = charPhys.capsuleRadius;
        float halfHeight = charPhys.capsuleHalfHeight; // half total height (feet to top)
        float joltHalfHeight = halfHeight - radius;    // cylinder-only half-height

        // GetPosition() returns the shape origin (feet).
        // Capsule center is at halfHeight above origin; hemispheres are ±joltHalfHeight from center.
        Im3d::Vec3 bottom(
            static_cast<float>(pos.GetX()),
            static_cast<float>(pos.GetY()) + halfHeight - joltHalfHeight,
            static_cast<float>(pos.GetZ())
        );
        Im3d::Vec3 top(
            static_cast<float>(pos.GetX()),
            static_cast<float>(pos.GetY()) + halfHeight + joltHalfHeight,
            static_cast<float>(pos.GetZ())
        );

        Im3d::DrawCapsule(bottom, top, radius);
    }
    Im3d::PopColor();

    // Draw static box colliders (cyan)
    Im3d::PushColor(Im3d::Color_Cyan);
    for (const auto& bodyInfo : staticBodies)
    {
        BodyInterface& bodyInterface = jolt->physicsSystem->GetBodyInterface();
        BodyID bodyId(bodyInfo.bodyIdValue);

        if (!bodyInterface.IsActive(bodyId) && !bodyInterface.IsAdded(bodyId))
            continue;

        RVec3 pos = bodyInterface.GetCenterOfMassPosition(bodyId);

        glm::vec3 halfExtents = bodyInfo.halfExtents;
        glm::mat3 rotMat = glm::mat3_cast(bodyInfo.rotation);

        Im3d::Mat4 transform(
            Im3d::Vec3(static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ())),
            Im3d::Mat3(
                Im3d::Vec3(rotMat[0][0], rotMat[0][1], rotMat[0][2]),
                Im3d::Vec3(rotMat[1][0], rotMat[1][1], rotMat[1][2]),
                Im3d::Vec3(rotMat[2][0], rotMat[2][1], rotMat[2][2])
            ),
            Im3d::Vec3(1.0f, 1.0f, 1.0f)
        );

        Im3d::PushMatrix(transform);
        Im3d::DrawAlignedBox(
            Im3d::Vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z),
            Im3d::Vec3( halfExtents.x,  halfExtents.y,  halfExtents.z)
        );
        Im3d::PopMatrix();
    }
    Im3d::PopColor();
}

} // namespace nt
