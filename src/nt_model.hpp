#pragma once

#include "nt_animation.hpp"
#include "nt_device.hpp"
#include "nt_buffer.hpp"
#include "nt_material.hpp"

#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>



// Forward declarations
namespace tinygltf {
  class Model;
  class Skin;
  class Animation;
  class Node;
}

namespace nt {

class NtModel {
    public:

        struct Vertex {
          glm::vec3 position{};
          glm::vec3 color{};
          glm::vec3 normal{};
          glm::vec2 uv{};
          glm::vec4 tangent{};  // w component stores handedness

          glm::ivec4 boneIndices; // Up to 4 bones per vertex
          glm::vec4 boneWeights; // Must sum to 1.0

          static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
          static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

          bool operator==(const Vertex &other) const {
            return position == other.position && color == other.color && normal == other.normal &&
                   uv == other.uv && tangent == other.tangent;
          }
        };

        struct Mesh {
          std::vector<Vertex> vertices{};
          std::vector<uint32_t> indices{};
          uint32_t materialIndex{0};
          std::string name{};
        };

        struct ShaderData
        {
            std::vector<glm::mat4> m_FinalJointsMatrices;
        };

        struct Bone {
            int globalGltfNodeIndex; // node index from the gltf nodes std::vector
            std::string name;

            // INITIAL
            glm::mat4 initialNodeMatrix{1.0f}; // Transform for world coordinate system
            glm::mat4 inverseBindMatrix; // Bones coordinate system

            // ANIMATED
            glm::vec3 animatedNodeTranslation{0.0f};                            // T
            glm::quat animatedNodeRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // R
            glm::vec3 animatedNodeScale{1.0f};                                  // S

            glm::mat4 getAnimatedBindMatrix() const {
                return glm::translate(glm::mat4(1.0f), animatedNodeTranslation) * // T
                       glm::mat4(animatedNodeRotation) *                          // R
                       glm::scale(glm::mat4(1.0f), animatedNodeScale) *           // S
                       initialNodeMatrix;
            }

            // TREE HIERARCHY
            int parentIndex; // -1 for root
            std::vector<int> childrenIndices;
        };

        struct Skeleton {
            bool isRoot = false;
            bool isAnimated = true;
            std::string name;
            std::vector<Bone> bones;
            std::unordered_map<int, int> nodeIndexToBoneIndex; // Map node index -> bone index
            ShaderData m_ShaderData;

            void Traverse();
            void Traverse(Bone const& bone, uint32_t indent = 0);
            void Update();
            void UpdateBone(int16_t boneIndex);
        };

        struct Builder {
          // CPU-side attributes, only needed for loading
          std::vector<Mesh> l_meshes{};
          std::vector<MaterialData> l_materialData{};
          std::optional<Skeleton> l_skeleton{};
          std::vector<NtAnimation> l_animations{};

          explicit Builder(NtDevice &device) : ntDevice{device} {}

          ~Builder() {}

          void loadGltfModel(const std::string &filepath);

        private:
          void loadGltfMaterials(const tinygltf::Model &model, const std::string &filepath);
          void loadGltfMeshes(const tinygltf::Model &model);
          void loadGltfSkeleton(const tinygltf::Model &model);
          void loadGltfBone(const tinygltf::Model &model, int globalGltfNodeIndex, int parentBone);
          void loadGltfAnimation(const tinygltf::Model &model, const tinygltf::Animation& anim);

         void calculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

          // Reference to device for material creation
          NtDevice &ntDevice;
        };

        NtModel(NtDevice &device, NtModel::Builder &builder);
        ~NtModel();

        NtModel(const NtModel &) = delete;
        NtModel& operator=(const NtModel &) = delete;

        static std::unique_ptr<NtModel> createModelFromFile(NtDevice &device, const std::string &filepath, MaterialType matType,
            VkDescriptorSetLayout materialLayout,
            VkDescriptorPool materialPool,
            VkDescriptorSetLayout boneLayout = VK_NULL_HANDLE,
            VkDescriptorPool bonePool = VK_NULL_HANDLE);
        uint32_t getMeshCount() const { return static_cast<uint32_t>(meshes.size()); }
        uint32_t getMaterialIndex(uint32_t meshIndex) const;
        const std::optional<Skeleton>& getSkeleton() const { return skeleton; }
        uint32_t getBonesCount() const { return skeleton.has_value() ? static_cast<uint32_t>(skeleton->bones.size()) : 0; }
        const std::vector<NtAnimation>& getAnimations() const { return animations; }

        MaterialType getMaterialType() const { return materialType; }
        void setMaterialType(MaterialType type) { materialType = type; }

        void createMaterialDescriptorSets(VkDescriptorSetLayout layout, VkDescriptorPool pool);
        VkDescriptorSet getMaterialDescriptorSet(uint32_t meshIndex) const;
        const MaterialData& getMaterialData(uint32_t meshIndex) const;
        const std::vector<MaterialData>& getMaterialDataList() const { return materialDataList; }

        const VkDescriptorSet& getBoneDescriptorSet() const { return boneDescriptorSet; }
        bool hasBoneDescriptor() const { return boneDescriptorSet != VK_NULL_HANDLE; }

        void bind (VkCommandBuffer commandBuffer, uint32_t meshIndex = 0);
        void draw (VkCommandBuffer commandBuffer, uint32_t meshIndex = 0);
        void drawAll (VkCommandBuffer commandBuffer);
        void updateSkeleton();

        bool hasSkeleton() const { return skeleton.has_value(); }

        static std::unique_ptr<NtModel> createPlane(NtDevice &device, float size, const std::string &texturePath,
            MaterialType matType,
            VkDescriptorSetLayout materialLayout,
            VkDescriptorPool materialPool);
    private:
        struct MeshBuffers {
          std::unique_ptr<NtBuffer> vertexBuffer;
          std::unique_ptr<NtBuffer> indexBuffer;
          uint32_t vertexCount;
          uint32_t indexCount;
          bool hasIndexBuffer = false;
          uint32_t materialIndex = 0;
        };

        void createMeshBuffers(const std::vector<Mesh> &meshes);
        void createVertexBuffer(const std::vector<Vertex> &vertices, MeshBuffers &meshBuffers);
        void createIndexBuffer(const std::vector<uint32_t> &indices, MeshBuffers &meshBuffers);
        void createBoneBuffer();
        void updateBoneBuffer(VkDescriptorSetLayout boneLayout, VkDescriptorPool bonePool);

        std::unique_ptr<NtBuffer> boneBuffer;
        VkDescriptorSet boneDescriptorSet = VK_NULL_HANDLE;

        NtDevice &ntDevice;
        std::vector<MeshBuffers> meshes;
        std::optional<Skeleton> skeleton;
        std::vector<NtAnimation> animations;

        std::vector<MaterialData> materialDataList;
        std::vector<VkDescriptorSet> materialDescriptorSets;
        MaterialType materialType = MaterialType::PBR;
};

}
