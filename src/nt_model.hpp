#pragma once

#include "nt_device.hpp"
#include "nt_buffer.hpp"
#include "nt_material.hpp"

#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <string>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <vector>

// Forward declarations
namespace tinygltf {
  class Model;
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

        struct Data {
          std::vector<Mesh> meshes{};
          std::vector<std::shared_ptr<NtMaterial>> materials{};

          explicit Data(NtDevice &device) : ntDevice{device} {}

          void loadObjModel(const std::string &filepath);
          void loadGltfModel(const std::string &filepath);

        private:
          void loadGltfMaterials(const tinygltf::Model &model, const std::string &filepath);
          void loadGltfMeshes(const tinygltf::Model &model);
          void calculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

          // Reference to device for material creation
          NtDevice &ntDevice;
        };

        NtModel(NtDevice &device, NtModel::Data &data);
        ~NtModel();

        NtModel(const NtModel &) = delete;
        NtModel& operator=(const NtModel &) = delete;

        static std::unique_ptr<NtModel> createModelFromFile(NtDevice &device, const std::string &filepath, VkDescriptorSetLayout materialLayout, VkDescriptorPool materialPool);
        uint32_t getMeshCount() const { return static_cast<uint32_t>(meshes.size()); }
        const std::vector<std::shared_ptr<NtMaterial>>& getMaterials() const { return materials; }
        uint32_t getMaterialIndex(uint32_t meshIndex) const;

        void bind (VkCommandBuffer commandBuffer, uint32_t meshIndex = 0);
        void draw (VkCommandBuffer commandBuffer, uint32_t meshIndex = 0);
        void drawAll (VkCommandBuffer commandBuffer);
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

        NtDevice &ntDevice;
        std::vector<MeshBuffers> meshes;
        std::vector<std::shared_ptr<NtMaterial>> materials;
};

}
