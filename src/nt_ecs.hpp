#pragma once

#include "nt_components.hpp"

#include <bitset>
#include <cassert>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <memory>
#include <queue>
#include <set>
#include <unordered_map>
#include <array>
#include <typeindex>

namespace nt {

//==============================
// ENTITY
//==============================
// Type alias
using NtEntity = std::uint32_t;

const NtEntity MAX_ENTITIES = 5000;

//==============================
// COMPONENT
//==============================
using NtComponentType = std::uint8_t;

const NtComponentType MAX_COMPONENTS = 32;

//==============================
// SIGNATURE
//==============================
using NtSignature = std::bitset<MAX_COMPONENTS>;

//==============================
// ENTITY MANAGER
//==============================
class NtEntityManager
{
public:
    NtEntityManager() {
        // Initialize the queue with all possible entity IDs
        for (NtEntity entity = 0; entity < MAX_ENTITIES; ++entity) {
            availableEntities.push(entity);
        }
    }

    NtEntity CreateEntity() {
        assert(livingEntityCount < MAX_ENTITIES && "Too many entities in existance");

        // Take an ID from the front of the queue
        NtEntity id = availableEntities.front();
        availableEntities.pop();
        ++livingEntityCount;

        return id;
    }

    void DestroyEntity(NtEntity entity) {
        assert(entity < MAX_ENTITIES && "Entity out of range");

        // Invalidate the destroyed entity's signature
        signatures[entity].reset();

        // Put the destroyed ID at the back of the queue
        availableEntities.push(entity);
        --livingEntityCount;
    }

    void SetSignature(NtEntity entity, NtSignature signature) {
        assert(entity < MAX_ENTITIES && "Entity out of range");

        // Put this entity's signature into the array
        signatures[entity] = signature;
    }

    NtSignature GetSignature(NtEntity entity) {
        assert(entity < MAX_ENTITIES && "Entity out of range");

        // Get this entity's signature from the array
        return signatures[entity];
    }

private:
    // Queue of unused entity IDs
    std::queue<NtEntity> availableEntities{};

    // Array of signatures where the index corresponds to the entity
    std::array<NtSignature, MAX_ENTITIES> signatures{};

    // Total living entities - used to keep limits on how many exist
    uint32_t livingEntityCount{};
};

//==============================
// COMPONENT ARRAY
//==============================
class IComponentArray
{
    public:
    virtual ~IComponentArray() = default;
    virtual void EntityDestroyed(NtEntity entity) = 0;
};

template<typename T>
class NtComponentArray : public IComponentArray
{
public:
    void InsertData(NtEntity entity, T component) {
        assert(entityToIndexMap.find(entity) == entityToIndexMap.end() && "Component added to the same entity more than once");

        // Put new entry at the end and update the maps
        size_t newIndex = size;
        entityToIndexMap[entity] = newIndex;
        indexToEntityMap[newIndex] = entity;
        componentArray[newIndex] = component;
        ++size;
    }

    void RemoveData(NtEntity entity) {
        assert(entityToIndexMap.find(entity) != entityToIndexMap.end() && "Removing non-existent component");

        // Copy element at the end into deleted element's place to maintain density
        size_t indexOfRemovedEntity = entityToIndexMap[entity];
        size_t indexOfLastElement = size - 1;
        componentArray[indexOfRemovedEntity] = componentArray[indexOfLastElement];

        // Update the map to point to the moved spot
        NtEntity entityOfLastElement = indexToEntityMap[indexOfLastElement];
        entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
        indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

        entityToIndexMap.erase(entity);
        indexToEntityMap.erase(indexOfLastElement);

        --size;
    }

    bool HasData(NtEntity entity) const {
            return entityToIndexMap.find(entity) != entityToIndexMap.end();
        }

    T& GetData(NtEntity entity) {
        assert(entityToIndexMap.find(entity) != entityToIndexMap.end() && "Retrieving non-existent component");

        // Return a reference to the entity's component
        return componentArray[entityToIndexMap[entity]];
    }

    void EntityDestroyed(NtEntity entity) override {
        if (entityToIndexMap.find(entity) != entityToIndexMap.end()) {
            // Remove the entity's component if it existed
            RemoveData(entity);
        }
    }

private:
    // The packed array of components (of generic type T)
    std::array<T, MAX_ENTITIES> componentArray;

    // Map from an entity ID to an array index
    std::unordered_map<NtEntity, size_t> entityToIndexMap;

    // Map from an array index to an entity ID
    std::unordered_map<size_t, NtEntity> indexToEntityMap;

    // Total size of valid entries in the array
    size_t size = 0;
};

//==============================
// COMPONENT MANAGER
//==============================
class NtComponentManager
{
public:
    template<typename T>
    void RegisterComponent()
    {
        std::type_index typeName(typeid(T));

        assert(componentTypes.find(typeName) == componentTypes.end() && "Registering component type more than once");

        // Add this component type to the component type map
        componentTypes.insert({typeName, nextComponentType});

        // Create a componentArray pointer and add it to the component arrays map
        componentArrays.insert({typeName, std::make_shared<NtComponentArray<T>>()});

        // Increment the value so that the next component registered will be different
        ++nextComponentType;
    }

    template<typename T>
    NtComponentType GetComponentType()
    {
        std::type_index typeIndex(typeid(T));

        assert(componentTypes.find(typeIndex) != componentTypes.end() && "Component not registered before use");

        // Return this component's type - used for creating signatures
        return componentTypes[typeIndex];
    }

    template<typename T>
    void AddComponent(NtEntity entity, T component)
    {
        // Add a component to the array for an entity
        GetComponentArray<T>()->InsertData(entity, component);
    }

    template<typename T>
    void RemoveComponent(NtEntity entity)
    {
        // Remove a component from the array for an entity
        GetComponentArray<T>()->RemoveData(entity);
    }

    template<typename T>
    bool HasComponent(NtEntity entity) {
        return GetComponentArray<T>()->HasData(entity);
    }

    template<typename T>
    T& GetComponent(NtEntity entity)
    {
        // Get a reference to a component from the array for an entity
        return GetComponentArray<T>()->GetData(entity);
    }

    void EntityDestroyed(NtEntity entity)
    {
        // Notify each component array that an entity has been destroyed
        // If it has a component for that entity, it will remove it
        for (auto const& pair : componentArrays)
        {
            auto const& component = pair.second;

            component->EntityDestroyed(entity);
        }
    }

private:
    // Map from type string pointer to a component type
    std::unordered_map<std::type_index, NtComponentType> componentTypes{};

    // Map from type striong pointer to a component array
    std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>> componentArrays{};

    // The component type to be assigned to the next registered component, starting at 0
    NtComponentType nextComponentType{};

    // Convenience function to get the statically casted pointer to the ComponentArray of type T
    template<typename T>
    std::shared_ptr<NtComponentArray<T>> GetComponentArray() {
        std::type_index typeIndex(typeid(T));

        assert (componentTypes.find(typeIndex) != componentTypes.end() && "Component not registered before use");

        return std::static_pointer_cast<NtComponentArray<T>>(componentArrays[typeIndex]);
    }
};

//==============================
// SYSTEM
//==============================
class NtSystem
{
public:
    std::set<NtEntity> entities;
};

//==============================
// SYSTEM MANAGER
//==============================
class NtSystemManager
{
public:
    template<typename T, typename... Args>
    std::shared_ptr<T> RegisterSystem(Args&&... args)
    {
        std::type_index typeIndex(typeid(T));

        assert(systems.find(typeIndex) == systems.end() && "Registering system more than once");

        // Create a pointer to the system and return it so it can be used externally
        auto system = std::make_shared<T>(std::forward<Args>(args)...);
        systems.insert({typeIndex,  std::static_pointer_cast<NtSystem>(system)});
        return system;
    }

    template<typename T>
    void SetSignature(NtSignature signature)
    {
        std::type_index typeIndex(typeid(T));

        assert(systems.find(typeIndex) != systems.end() && "System used before registered");

        // Set the signature for this system
        signatures.insert({typeIndex, signature});
    }

    void EntityDestroyed(NtEntity entity)
    {
        // Erase a destroyed entity from all system lists
        // entities is a set so no check needed
        for (auto const& pair : systems)
        {
            auto const& system = pair.second;

            system->entities.erase(entity);
        }
    }

    void EntitySignatureChanged(NtEntity entity, NtSignature entitySignature)
    {
        // Notify each system taht an entity's signature changed
        for (auto const& pair : systems)
        {
            auto const& type = pair.first;
            auto const& system = pair.second;
            auto const& systemSignature = signatures[type];

            // Entity signature matches system signature - insert into set
            if ((entitySignature & systemSignature) == systemSignature) {
                system->entities.insert(entity);
            }
            // Entity signature does not match system signature - erase from set
            else {
                system->entities.erase(entity);
            }
        }
    }

private:
    // Map from system type string pointer to a signature
    std::unordered_map<std::type_index, NtSignature> signatures{};

    // Map from system type string pointer to a system pointer
    std::unordered_map<std::type_index, std::shared_ptr<NtSystem>> systems{};
};

// Forward declaration
class NtAstral;
//==============================
// Entity Handle Forward Declaration (for readable access)
//==============================
class NtEntityHandle {
public:
    NtEntityHandle(NtEntity id, NtAstral* astral) : entity(id), astral(astral) {}

    template<typename T>
    NtEntityHandle& AddComponent(T component);

    template<typename T>
    void RemoveComponent();

    template<typename T>
    T& GetComponent();

    NtEntity GetID() const { return entity; }

    operator NtEntity() const { return entity; }

private:
    NtEntity entity;
    NtAstral* astral;
};

//==============================
// Main ECS controller
//==============================
class NtAstral
{
public:
    void Init()
    {
        // Create pointers to each manager
        componentManager = std::make_unique<NtComponentManager>();
        entityManager = std::make_unique<NtEntityManager>();
        systemManager = std::make_unique<NtSystemManager>();
    }

    NtEntityHandle CreateEntity() {
        return NtEntityHandle(entityManager->CreateEntity(), this);
    }

    void DestroyEntity(NtEntity entity)
    {
        entityManager->DestroyEntity(entity);
        componentManager->EntityDestroyed(entity);
        systemManager->EntityDestroyed(entity);
    }

    // Component methods
    template<typename T>
    void RegisterComponent()
    {
        componentManager->RegisterComponent<T>();
    }

    template<typename T>
    void AddComponent(NtEntity entity, T component)
    {
        componentManager->AddComponent<T>(entity, component);

        auto signature = entityManager->GetSignature(entity);
        signature.set(componentManager->GetComponentType<T>(), true);
        entityManager->SetSignature(entity, signature);

        systemManager->EntitySignatureChanged(entity, signature);
    }

    template<typename T>
    void RemoveComponent(NtEntity entity)
    {
        componentManager->RemoveComponent<T>(entity);

        auto signature = entityManager->GetSignature(entity);
        signature.set(componentManager->GetComponentType<T>(), false);
        entityManager->SetSignature(entity, signature);

        systemManager->EntitySignatureChanged(entity, signature);
    }

    template<typename T>
    bool HasComponent(NtEntity entity) {
        return componentManager->HasComponent<T>(entity);
    }

    template<typename T>
    T& GetComponent(NtEntity entity)
    {
        return componentManager->GetComponent<T>(entity);
    }

    template<typename T>
    NtComponentType GetComponentType()
    {
        return componentManager->GetComponentType<T>();
    }

    // System methods
    template<typename T, typename... Args>
    std::shared_ptr<T> RegisterSystem(Args&&... args)
    {
        return systemManager->RegisterSystem<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    void SetSystemSignature(NtSignature signature)
    {
        systemManager->SetSignature<T>(signature);
    }

private:
    std::unique_ptr<NtComponentManager> componentManager;
    std::unique_ptr<NtEntityManager> entityManager;
    std::unique_ptr<NtSystemManager> systemManager;
};

//==============================
// Entity Handle Implementation (for readable access)
//==============================
template<typename T>
NtEntityHandle& NtEntityHandle::AddComponent(T component) {
    astral->AddComponent<T>(entity, component);
    return *this;
}

template<typename T>
void NtEntityHandle::RemoveComponent() {
    astral->RemoveComponent<T>(entity);
}

template<typename T>
T& NtEntityHandle::GetComponent() {
    return astral->GetComponent<T>(entity);
}

}
