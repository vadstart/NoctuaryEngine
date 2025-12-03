#include "nt_model.hpp"
#include "imgui.h"
#include "nt_descriptors.hpp"
#include "nt_log.hpp"
#include "nt_utils.hpp"
#include <memory>
#include <ostream>
#include <stdexcept>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tinygltf/tiny_gltf.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <cctype>

namespace std {
template<>
struct hash<nt::NtModel::Vertex> {
  size_t operator()(nt::NtModel::Vertex const &vertex) const {
    size_t seed = 0;
    nt::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv, vertex.tangent);
    return seed;
  }
};

}

namespace nt {

NtModel::NtModel(NtDevice &device, NtModel::Builder &builder) : ntDevice{device},
        materials{std::move(builder.l_materials)},
        skeleton{std::move(builder.l_skeleton)},
        animations{std::move(builder.l_animations)}
{
  createMeshBuffers(builder.l_meshes);
  builder.l_meshes.clear();
  builder.l_meshes.shrink_to_fit();

  if (skeleton.has_value() && skeleton->bones.size() > 0) {
      createBoneBuffer();
  }
}

NtModel::~NtModel() {
}

std::unique_ptr<NtModel> NtModel::createModelFromFile(NtDevice &device, const std::string &filepath,
    VkDescriptorSetLayout materialLayout,
    VkDescriptorPool materialPool,
    VkDescriptorSetLayout boneLayout,
    VkDescriptorPool bonePool) {
  Builder builder{device};

  // Determine file type by extension
  std::string extension = filepath.substr(filepath.find_last_of('.') + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  if (extension == "gltf" || extension == "glb") {
    builder.loadGltfModel(filepath);
  } else {
    NT_LOG_ERROR(LogAssets, "Unsupported file format: {} - Supported formats are: .gltf, .glb", extension);
    throw std::runtime_error("Unsupported file format: " + extension + ". Supported formats are: .gltf, .glb");
  }

  // Initialize material descriptor sets for all loaded materials
  for (const auto& material : builder.l_materials) {
    material->updateDescriptorSet(materialLayout, materialPool);
  }

  // Create the model first so we can call its member function
  auto model = std::make_unique<NtModel>(device, builder);

  // Initialize bone descriptor sets if skeleton exists
  if (boneLayout != VK_NULL_HANDLE && bonePool != VK_NULL_HANDLE && model->boneBuffer) {
    model->updateBoneBuffer(boneLayout, bonePool);
  }

  return model;
}


void NtModel::createMeshBuffers(const std::vector<Mesh> &meshData) {
  meshes.resize(meshData.size());

  for (size_t i = 0; i < meshData.size(); ++i) {
    const auto &mesh = meshData[i];
    createVertexBuffer(mesh.vertices, meshes[i]);
    createIndexBuffer(mesh.indices, meshes[i]);
    meshes[i].materialIndex = mesh.materialIndex;
  }
}

void NtModel::createVertexBuffer(const std::vector<Vertex> &vertices, MeshBuffers &meshBuffers) {
  meshBuffers.vertexCount = static_cast<uint32_t>(vertices.size());
  assert(meshBuffers.vertexCount >= 3 && "Vertex count must be at least 3");

  // Create buffer on Device(GPU) side
  VkDeviceSize bufferSize = sizeof(vertices[0]) * meshBuffers.vertexCount;
  uint32_t vertexSize = sizeof(vertices[0]);

  NtBuffer stagingBuffer {
    ntDevice,
    vertexSize,
    meshBuffers.vertexCount,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
  };

  stagingBuffer.map();
  stagingBuffer.writeToBuffer((void *)vertices.data());

  meshBuffers.vertexBuffer = std::make_unique<NtBuffer>(
    ntDevice,
    vertexSize,
    meshBuffers.vertexCount,
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  );

  ntDevice.copyBuffer(stagingBuffer.getBuffer(), meshBuffers.vertexBuffer->getBuffer(), bufferSize);
}

void NtModel::createIndexBuffer(const std::vector<uint32_t> &indices, MeshBuffers &meshBuffers) {
  meshBuffers.indexCount = static_cast<uint32_t>(indices.size());
  meshBuffers.hasIndexBuffer = meshBuffers.indexCount > 0;

  if (!meshBuffers.hasIndexBuffer)
    return;

  VkDeviceSize bufferSize = sizeof(indices[0]) * meshBuffers.indexCount;
  uint32_t indexSize = sizeof(indices[0]);

  NtBuffer stagingBuffer {
    ntDevice,
    indexSize,
    meshBuffers.indexCount,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
  };

  stagingBuffer.map();
  stagingBuffer.writeToBuffer((void *)indices.data());

  meshBuffers.indexBuffer = std::make_unique<NtBuffer>(
    ntDevice,
    indexSize,
    meshBuffers.indexCount,
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  ntDevice.copyBuffer(stagingBuffer.getBuffer(), meshBuffers.indexBuffer->getBuffer(), bufferSize);
}

void NtModel::createBoneBuffer() {
    size_t boneCount = getBonesCount();

    // Buffer for bone matrices
    boneBuffer = std::make_unique<NtBuffer>(
        ntDevice,
        sizeof(glm::mat4),
        boneCount,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    boneBuffer->map();

    // Initialize with identity matrices
    std::vector<glm::mat4> identityMatrices(boneCount, glm::mat4(1.0f));

    boneBuffer->writeToBuffer(identityMatrices.data());
    boneBuffer->flush();
}

void NtModel::updateBoneBuffer(VkDescriptorSetLayout boneLayout, VkDescriptorPool bonePool) {
    if (!boneBuffer) {
        NT_LOG_ERROR(LogAssets, "Cannot update bone buffer: buffer doesn't exist!");
        return;
    }

    auto bufferInfo = boneBuffer->descriptorInfo();

    // Use raw pool allocate method, not NtDescriptorWriter
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = bonePool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &boneLayout;

    if (vkAllocateDescriptorSets(ntDevice.device(), &allocInfo, &boneDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate bone descriptor set!");
    }

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = boneDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(ntDevice.device(), 1, &descriptorWrite, 0, nullptr);
}

void NtModel::bind (VkCommandBuffer commandBuffer, uint32_t meshIndex) {
  assert(meshIndex < meshes.size() && "Mesh index out of range");

  const auto &mesh = meshes[meshIndex];
  VkBuffer buffers[] = {mesh.vertexBuffer->getBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

  if (mesh.hasIndexBuffer) {
    vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
  }
}

void NtModel::draw (VkCommandBuffer commandBuffer, uint32_t meshIndex) {
  assert(meshIndex < meshes.size() && "Mesh index out of range");

  const auto &mesh = meshes[meshIndex];
  if (mesh.hasIndexBuffer) {
    vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
  } else {
    vkCmdDraw(commandBuffer, mesh.vertexCount, 1, 0, 0);
  }
}

void NtModel::drawAll (VkCommandBuffer commandBuffer) {
  for (uint32_t i = 0; i < meshes.size(); ++i) {
    bind(commandBuffer, i);
    draw(commandBuffer, i);
  }
}

std::vector<VkVertexInputBindingDescription> NtModel::Vertex::getBindingDescriptions() {
  std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);

  bindingDescriptions[0].binding = 0;
  bindingDescriptions[0].stride = sizeof(Vertex);
  bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> NtModel::Vertex::getAttributeDescriptions() {
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

  attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
  attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
  attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
  attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});
  attributeDescriptions.push_back({4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)});
  attributeDescriptions.push_back({5, 0, VK_FORMAT_R32G32B32A32_SINT, offsetof(Vertex, boneIndices)});
  attributeDescriptions.push_back({6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, boneWeights)});

  return attributeDescriptions;
}

void NtModel::Builder::loadGltfModel(const std::string &filepath) {
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = false;
  std::string extension = filepath.substr(filepath.find_last_of('.') + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  if (extension == "glb") {
    ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
  } else {
    ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
  }

  if (!warn.empty()) {
    NT_LOG_WARN(LogAssets, "glTF loader warning: {}", warn);
  }

  if (!err.empty()) {
    NT_LOG_ERROR(LogAssets, "glTF loader error: {}", err);
  }

  if (!ret) {
    NT_LOG_ERROR(LogAssets, "Failed to parse glTF file {}", filepath);
    throw std::runtime_error("Failed to parse glTF file");
  }

  NT_LOG_VERBOSE(LogAssets, "Successfully loaded glTF file: {}\n   Meshes: {}\n   Materials: {}\n   Textures: {}\n   Animations:{}"
     ,filepath, model.meshes.size(), model.materials.size(), model.textures.size(), model.animations.size());

  // Load materials first
  loadGltfMaterials(model, filepath);

  // Load meshes
  loadGltfMeshes(model);

  // Load skeleton
  if (!model.skins.empty()) {
      loadGltfSkeleton(model);
  }

  // Load animations
  for (const auto& anim : model.animations) {
      loadGltfAnimation(model, anim);
  }
}

void NtModel::Builder::loadGltfMaterials(const tinygltf::Model &model, const std::string &filepath) {
  std::string baseDir = filepath.substr(0, filepath.find_last_of('/') + 1);

  for (const auto &material : model.materials) {
    NtMaterial::MaterialData materialData;
    materialData.name = material.name;

    NT_LOG_VERBOSE(LogAssets, "Loading material: {}", (material.name.empty() ? "<unnamed>" : material.name));

    // PBR Metallic Roughness
    const auto &pbr = material.pbrMetallicRoughness;
    materialData.pbrMetallicRoughness.baseColorFactor = glm::vec4(
      pbr.baseColorFactor[0], pbr.baseColorFactor[1],
      pbr.baseColorFactor[2], pbr.baseColorFactor[3]
    );
    materialData.pbrMetallicRoughness.metallicFactor = pbr.metallicFactor;
    materialData.pbrMetallicRoughness.roughnessFactor = pbr.roughnessFactor;

    // Load base color texture
    if (pbr.baseColorTexture.index >= 0) {
      const auto &texture = model.textures[pbr.baseColorTexture.index];
      const auto &image = model.images[texture.source];

      if (!image.uri.empty()) {
        std::string texturePath = baseDir + image.uri;
        NT_LOG_VERBOSE(LogAssets, "Loading base color texture: {}", texturePath);
        try {
          materialData.pbrMetallicRoughness.baseColorTexture =
            NtImage::createTextureFromFile(ntDevice, texturePath);
          materialData.pbrMetallicRoughness.baseColorTexCoord = pbr.baseColorTexture.texCoord;
        } catch (const std::exception& e) {
            NT_LOG_ERROR(LogAssets, "Failed to load base color texture: {}", e.what());
        }
      } else if (!image.image.empty()) {
          // Embedded texture - so let's create texture from memory
          try {
            materialData.pbrMetallicRoughness.baseColorTexture =
              NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size());
            materialData.pbrMetallicRoughness.baseColorTexCoord = pbr.baseColorTexture.texCoord;
          } catch (const std::exception& e) {
              NT_LOG_ERROR(LogAssets, "Failed to load base embedded color texture: {}", e.what());
          }
      }

      // Parse UV transform from GLTF (access pbr.baseColorTexture, not materialData)
      if (pbr.baseColorTexture.extensions.count("KHR_texture_transform") > 0) {
            const auto& transform = pbr.baseColorTexture.extensions.at("KHR_texture_transform");

            // Parse scale
            if (transform.Has("scale") && transform.Get("scale").IsArray()) {
                auto scaleArray = transform.Get("scale").Get<tinygltf::Value::Array>();
                if (scaleArray.size() >= 2) {
                    materialData.uvScale.x = scaleArray[0].GetNumberAsDouble();
                    materialData.uvScale.y = scaleArray[1].GetNumberAsDouble();
                    NT_LOG_VERBOSE(LogAssets, "UV Scale: {} {}", materialData.uvScale.x, materialData.uvScale.y);
                }
            }

            // Parse offset
            if (transform.Has("offset") && transform.Get("offset").IsArray()) {
                auto offsetArray = transform.Get("offset").Get<tinygltf::Value::Array>();
                if (offsetArray.size() >= 2) {
                    materialData.uvOffset.x = offsetArray[0].GetNumberAsDouble();
                    materialData.uvOffset.y = offsetArray[1].GetNumberAsDouble();
                    NT_LOG_VERBOSE(LogAssets, "UV Offset: {} {}", materialData.uvOffset.x, materialData.uvOffset.y);
                }
            }

            // Parse rotation
            if (transform.Has("rotation")) {
                materialData.uvRotation = transform.Get("rotation").GetNumberAsDouble();
                NT_LOG_VERBOSE(LogAssets, "UV Rotation: {} radians", materialData.uvRotation);
            }
        }
    }

    // Load metallic-roughness texture
    if (pbr.metallicRoughnessTexture.index >= 0) {
      const auto &texture = model.textures[pbr.metallicRoughnessTexture.index];
      const auto &image = model.images[texture.source];

      if (!image.uri.empty()) {
        std::string texturePath = baseDir + image.uri;
        NT_LOG_VERBOSE(LogAssets, "Loading metallic-roughness texture: {}", texturePath);
        try {
          materialData.pbrMetallicRoughness.metallicRoughnessTexture =
            NtImage::createTextureFromFile(ntDevice, texturePath, true);
          materialData.pbrMetallicRoughness.metallicRoughnessTexCoord = pbr.metallicRoughnessTexture.texCoord;
        } catch (const std::exception& e) {
            NT_LOG_ERROR(LogAssets, "Failed to load metallic-roughness texture: {}", e.what());
        }
      } else if (!image.image.empty()) {
        // Embedded texture - so let's create texture from memory
        materialData.pbrMetallicRoughness.metallicRoughnessTexture =
            NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size(), true);
        materialData.pbrMetallicRoughness.metallicRoughnessTexCoord = pbr.metallicRoughnessTexture.texCoord;
        }
    }

    // Load normal texture
    if (material.normalTexture.index >= 0) {
      const auto &texture = model.textures[material.normalTexture.index];
      const auto &image = model.images[texture.source];

      if (!image.uri.empty()) {
        std::string texturePath = baseDir + image.uri;
        NT_LOG_VERBOSE(LogAssets, "Loading normal texture: {}", texturePath);
        try {
          materialData.normalTexture = NtImage::createTextureFromFile(ntDevice, texturePath, true);
          materialData.normalScale = material.normalTexture.scale;
          materialData.normalTexCoord = material.normalTexture.texCoord;
        } catch (const std::exception& e) {
            NT_LOG_ERROR(LogAssets, "Failed to load normal texture: {}", e.what());
        }
      } else if (!image.image.empty()) {
        // Embedded texture - so let's create texture from memory
        materialData.normalTexture =
            NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size(), true);
        materialData.normalScale = material.normalTexture.scale;
        materialData.normalTexCoord = material.normalTexture.texCoord;
    }
    }

    // Alpha mode
    if (material.alphaMode == "OPAQUE") {
      materialData.alphaMode = NtMaterial::AlphaMode::Opaque;
    } else if (material.alphaMode == "MASK") {
      materialData.alphaMode = NtMaterial::AlphaMode::Mask;
    } else if (material.alphaMode == "BLEND") {
      materialData.alphaMode = NtMaterial::AlphaMode::Blend;
    }

    materialData.alphaCutoff = material.alphaCutoff;
    materialData.doubleSided = material.doubleSided;

    l_materials.push_back(std::make_shared<NtMaterial>(ntDevice, materialData));
  }

  // Create a default material if none exist
  if (l_materials.empty()) {
    NT_LOG_WARN(LogAssets, "No materials found, creating default material");
    NtMaterial::MaterialData defaultMaterial;
    defaultMaterial.name = "Default";
    l_materials.push_back(std::make_shared<NtMaterial>(ntDevice, defaultMaterial));
  }
}

void NtModel::Builder::loadGltfMeshes(const tinygltf::Model &model) {
  for (const auto &gltfMesh : model.meshes) {
    for (const auto &primitive : gltfMesh.primitives) {
      Mesh mesh;
      mesh.name = gltfMesh.name;
      mesh.materialIndex = primitive.material >= 0 ? primitive.material : 0;

      // Get vertex attributes
      const auto &posAccessor = model.accessors[primitive.attributes.at("POSITION")];
      const auto &posBufferView = model.bufferViews[posAccessor.bufferView];
      const auto &posBuffer = model.buffers[posBufferView.buffer];

      // Normal attribute
      const auto &normalAccessor = model.accessors[primitive.attributes.at("NORMAL")];
      const auto &normalBufferView = model.bufferViews[normalAccessor.bufferView];
      const auto &normalBuffer = model.buffers[normalBufferView.buffer];

      // UV attribute (optional)
      const float *uvData = nullptr;
      if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
        const auto &uvAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
        const auto &uvBufferView = model.bufferViews[uvAccessor.bufferView];
        const auto &uvBuffer = model.buffers[uvBufferView.buffer];
        uvData = reinterpret_cast<const float*>(&uvBuffer.data[uvBufferView.byteOffset + uvAccessor.byteOffset]);
      }

      // Tangent attribute (optional)
      const float *tangentData = nullptr;
      if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
        const auto &tangentAccessor = model.accessors[primitive.attributes.at("TANGENT")];
        const auto &tangentBufferView = model.bufferViews[tangentAccessor.bufferView];
        const auto &tangentBuffer = model.buffers[tangentBufferView.buffer];
        tangentData = reinterpret_cast<const float*>(&tangentBuffer.data[tangentBufferView.byteOffset + tangentAccessor.byteOffset]);
      }

      // Bone joints (indices)
      const void *jointsData = nullptr;
      int jointsComponentType = 0;
      if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {
        const auto &jointsAccessor = model.accessors[primitive.attributes.at("JOINTS_0")];
        const auto &jointsBufferView = model.bufferViews[jointsAccessor.bufferView];
        const auto &jointsBuffer = model.buffers[jointsBufferView.buffer];
        jointsData = &jointsBuffer.data[jointsBufferView.byteOffset + jointsAccessor.byteOffset];
        jointsComponentType = jointsAccessor.componentType;
      }

      // Bone weights
      const float *weightsData = nullptr;
      if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()) {
        const auto &weightsAccessor = model.accessors[primitive.attributes.at("WEIGHTS_0")];
        const auto &weightsBufferView = model.bufferViews[weightsAccessor.bufferView];
        const auto &weightsBuffer = model.buffers[weightsBufferView.buffer];
        weightsData = reinterpret_cast<const float*>(&weightsBuffer.data[weightsBufferView.byteOffset + weightsAccessor.byteOffset]);
      }

      // Extract vertex data
      const float *posData = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);
      const float *normalData = reinterpret_cast<const float*>(&normalBuffer.data[normalBufferView.byteOffset + normalAccessor.byteOffset]);

      mesh.vertices.resize(posAccessor.count);
      for (size_t i = 0; i < posAccessor.count; ++i) {
        Vertex &vertex = mesh.vertices[i];

        // Position
        vertex.position = glm::vec3(posData[i * 3], posData[i * 3 + 1], posData[i * 3 + 2]);

        // Normal
        vertex.normal = glm::vec3(normalData[i * 3], normalData[i * 3 + 1], normalData[i * 3 + 2]);

        // UV
        if (uvData) {
          vertex.uv = glm::vec2(uvData[i * 2], uvData[i * 2 + 1]);
        } else {
          vertex.uv = glm::vec2(0.0f, 0.0f);
        }

        // Tangent
        if (tangentData) {
          vertex.tangent = glm::vec4(tangentData[i * 4], tangentData[i * 4 + 1],
                                    tangentData[i * 4 + 2], tangentData[i * 4 + 3]);
        } else {
          vertex.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        }

        // Bone indices
        if (jointsData) {
        if (jointsComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const uint16_t *joints = reinterpret_cast<const uint16_t*>(jointsData);
            vertex.boneIndices = glm::ivec4(joints[i * 4], joints[i * 4 + 1],
                                            joints[i * 4 + 2], joints[i * 4 + 3]);
        } else if (jointsComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            const uint8_t *joints = reinterpret_cast<const uint8_t*>(jointsData);
            vertex.boneIndices = glm::ivec4(joints[i * 4], joints[i * 4 + 1],
                                            joints[i * 4 + 2], joints[i * 4 + 3]);
        }
        }

        // Bone weights
        if (weightsData) {
        vertex.boneWeights = glm::vec4(weightsData[i * 4], weightsData[i * 4 + 1],
                                        weightsData[i * 4 + 2], weightsData[i * 4 + 3]);

        // Normalize weights to sum to 1.0
        float sum = vertex.boneWeights.x + vertex.boneWeights.y +
                    vertex.boneWeights.z + vertex.boneWeights.w;
        if (sum > 0.0001f) {
            vertex.boneWeights /= sum;
        }
        }

        // Default color (glTF doesn't typically have per-vertex colors)
        vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
      }

      // Extract indices
      if (primitive.indices >= 0) {
        const auto &indexAccessor = model.accessors[primitive.indices];
        const auto &indexBufferView = model.bufferViews[indexAccessor.bufferView];
        const auto &indexBuffer = model.buffers[indexBufferView.buffer];

        mesh.indices.resize(indexAccessor.count);

        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
          const uint16_t *indexData = reinterpret_cast<const uint16_t*>(&indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
          for (size_t i = 0; i < indexAccessor.count; ++i) {
            mesh.indices[i] = static_cast<uint32_t>(indexData[i]);
          }
        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
          const uint32_t *indexData = reinterpret_cast<const uint32_t*>(&indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
          for (size_t i = 0; i < indexAccessor.count; ++i) {
            mesh.indices[i] = indexData[i];
          }
        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
          const uint8_t *indexData = reinterpret_cast<const uint8_t*>(&indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
          for (size_t i = 0; i < indexAccessor.count; ++i) {
            mesh.indices[i] = static_cast<uint32_t>(indexData[i]);
          }
        }
      }

      // Calculate tangents if not provided by the model
    if (!tangentData && !mesh.indices.empty()) {
        calculateTangents(mesh.vertices, mesh.indices);
    }

      l_meshes.push_back(mesh);
    }
  }
}

void NtModel::Builder::loadGltfSkeleton(const tinygltf::Model &model) {
    size_t numSkeletons = model.skins.size();
    if (!numSkeletons)
        return;

    if (numSkeletons > 1)
        NT_LOG_VERBOSE(LogAssets, "A model should only have a single skin/armature/skeleton. Using skin 0.");

    l_skeleton = Skeleton();

    // Use skeleton 0 from GLTF model to fill the skeleton
    // {
        const tinygltf::Skin& skin = model.skins[0];

        // Does it have information about bones?
        if (skin.inverseBindMatrices != -1) {
            auto& bones = l_skeleton->bones;

            size_t numBones = skin.joints.size();
            bones.resize(numBones);
            // TODO: resize shaderdata_finalbonematrices

            l_skeleton->name = skin.name;
            NT_LOG_VERBOSE(LogAssets, "Loading skeleton: {}", l_skeleton->name);

            // Get array of inverse bind matrices for all bones
            // First retrieve raw data
            const glm::mat4* inverseBindMatrices;
            {
                const void *jointsData = nullptr;
                int inverseBindComponentType = 0;
                const auto &inverseBindAccessor = model.accessors[skin.inverseBindMatrices];
                const auto &inverseBindBufferView = model.bufferViews[inverseBindAccessor.bufferView];
                const auto &inverseBindBuffer = model.buffers[inverseBindBufferView.buffer];
                // TODO: sounds sketchy:
                inverseBindMatrices = reinterpret_cast<const glm::mat4*>(&inverseBindBuffer.data[inverseBindBufferView.byteOffset + inverseBindAccessor.byteOffset]);
                inverseBindComponentType = inverseBindAccessor.componentType;

            // loop over all joints from gltf model and fill our skeleton with bones
            for (size_t jointIndex = 0; jointIndex < numBones; ++jointIndex) {
                int globalGltfNodeIndex = skin.joints[jointIndex];
                auto& bone = bones[jointIndex];

                bone.globalGltfNodeIndex = globalGltfNodeIndex;
                bone.inverseBindMatrix = inverseBindMatrices[jointIndex];
                bone.name = model.nodes[globalGltfNodeIndex].name;

                // set up node transform
                auto& gltfNode = model.nodes[globalGltfNodeIndex];

                if (gltfNode.translation.size() == 3) {
                    bone.animatedNodeTranslation = glm::make_vec3(gltfNode.translation.data());
                }
                if (gltfNode.rotation.size() == 4) {
                    glm::quat q = glm::make_quat(gltfNode.rotation.data());
                    bone.animatedNodeRotation = q;
                }
                if (gltfNode.scale.size() == 3) {
                    bone.animatedNodeScale = glm::make_vec3(gltfNode.scale.data());
                }
                if (gltfNode.matrix.size() == 16) {
                    bone.initialNodeMatrix = glm::make_mat4x4(gltfNode.matrix.data());
                }
                else {
                    bone.initialNodeMatrix = glm::mat4(1.0f);
                }

                // set up the "global node" to "bone index" mapping
                l_skeleton->nodeIndexToBoneIndex[globalGltfNodeIndex] = jointIndex;
            }

            int rootJoint = skin.joints[0];
            loadGltfBone(model, rootJoint, -1);
        }
     }

    // Initialize the shader data vector
    l_skeleton->m_ShaderData.m_FinalJointsMatrices.resize(l_skeleton->bones.size(), glm::mat4(1.0f));

    NT_LOG_VERBOSE(LogAssets, "Bones: {}", l_skeleton->bones.size());
}

void NtModel::Builder::loadGltfBone(const tinygltf::Model &model, int globalGltfNodeIndex, int parentBone) {
    int currentBone = l_skeleton->nodeIndexToBoneIndex[globalGltfNodeIndex];
    auto& bone = l_skeleton->bones[currentBone];

    bone.parentIndex = parentBone;

    size_t numChildren = model.nodes[globalGltfNodeIndex].children.size();
    if (numChildren > 0) {
        bone.childrenIndices.resize(numChildren);
        for (size_t childIndex = 0; childIndex < numChildren; ++childIndex) {
            uint32_t childGlobalIndex = model.nodes[globalGltfNodeIndex].children[childIndex];
            bone.childrenIndices[childIndex] = l_skeleton->nodeIndexToBoneIndex[childGlobalIndex];
            loadGltfBone(model, childGlobalIndex, currentBone);
        }
    }
}

void NtModel::Builder::loadGltfAnimation(const tinygltf::Model &model, const tinygltf::Animation& anim) {
    NtAnimation animation;
    animation.name = anim.name.empty() ? "Unnamed" : anim.name;
    animation.duration = 0.0f;

    // Load samplers (keyframe data)
    for (const auto& sampler : anim.samplers) {
        NtAnimationSampler animSampler;

        // Input = time values
        {
            const tinygltf::Accessor& accessor = model.accessors[sampler.input];
            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            const float* times = reinterpret_cast<const float*>(
                &buffer.data[bufferView.byteOffset + accessor.byteOffset]
            );

            animSampler.inputTimestamps.resize(accessor.count);
            std::memcpy(animSampler.inputTimestamps.data(), times, accessor.count * sizeof(float));

            // Track longest animation
            animation.duration = std::max(animation.duration, times[accessor.count - 1]);
        }

        // Output = transform values (vec3 for translation/scale, vec4 for rotation)
        {
            const tinygltf::Accessor& accessor = model.accessors[sampler.output];
            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
            const void* dataPtr = &buffer.data[bufferView.byteOffset + accessor.byteOffset];

            animSampler.outputValues.resize(accessor.count);

            if (accessor.type == TINYGLTF_TYPE_VEC3) {
                const glm::vec3* data = reinterpret_cast<const glm::vec3*>(dataPtr);
                for (size_t i = 0; i < accessor.count; ++i) {
                    animSampler.outputValues[i] = glm::vec4(data[i], 0.0f);
                }
            } else if (accessor.type == TINYGLTF_TYPE_VEC4) {
                std::memcpy(animSampler.outputValues.data(), dataPtr, accessor.count * sizeof(glm::vec4));
            }
        }

        // Interpolation
        if (sampler.interpolation == "LINEAR") {
            animSampler.interpolation = NtAnimationSampler::LINEAR;
        } else if (sampler.interpolation == "STEP") {
            animSampler.interpolation = NtAnimationSampler::STEP;
        } else {
            animSampler.interpolation = NtAnimationSampler::CUBICSPLINE;
        }

        animation.samplers.push_back(animSampler);
    }

    // Load channels (which bone each sampler affects)
    for (const auto& channel : anim.channels) {
        NtAnimationChannel animChannel;
        animChannel.samplerIndex = channel.sampler;

        // Map GLTF node to bone index
        int nodeIndex = channel.target_node;

        // Find which bone this node corresponds to
        if (l_skeleton.has_value() && l_skeleton->nodeIndexToBoneIndex.count(nodeIndex) > 0) {
            animChannel.targetNode = l_skeleton->nodeIndexToBoneIndex[nodeIndex];
        } else {
            // Skip channels that don't target bones in the skeleton
            continue;
        }

        if (channel.target_path == "translation") {
            animChannel.path = NtAnimationChannel::TRANSLATION;
        } else if (channel.target_path == "rotation") {
            animChannel.path = NtAnimationChannel::ROTATION;
        } else if (channel.target_path == "scale") {
            animChannel.path = NtAnimationChannel::SCALE;
        }

        animation.channels.push_back(animChannel);
    }

    l_animations.push_back(animation);
    NT_LOG_VERBOSE(LogAssets, "Animation: {} ({}s)", animation.name, animation.duration);
}

void NtModel::Skeleton::Traverse()
{
    NT_LOG_VERBOSE(LogAssets, "Skeleton: {}", name);
    uint32_t indent = 0;
    std::string indentStr(indent, ' ');
    auto& joint = bones[0]; // root joint
    Traverse(joint, indent + 1);
}

void NtModel::Skeleton::Traverse(Bone const& bone, uint32_t indent)
{
    std::string indentStr(indent, ' ');
    size_t numberOfChildren = bone.childrenIndices.size();
    NT_LOG_VERBOSE(LogAssets, "Bone: {} Parent: {}  Children: {})", bone.name, bone.parentIndex, numberOfChildren);

    for (size_t childIndex = 0; childIndex < numberOfChildren; ++childIndex)
    {
        int jointIndex = bone.childrenIndices[childIndex];

        NT_LOG_VERBOSE(LogAssets, "Child: {} Index: {}", childIndex, jointIndex);
    }

    for (size_t childIndex = 0; childIndex < numberOfChildren; ++childIndex)
    {
        int jointIndex = bone.childrenIndices[childIndex];
        Traverse(bones[jointIndex], indent + 1);
    }
}

void NtModel::Skeleton::Update()
{
    // update the final global transform of all joints
    int16_t numberOfBones = static_cast<int16_t>(bones.size());

    if (!isAnimated) // used for debugging to check if the model renders w/o deformation
    {
        for (int16_t boneIndex = 0; boneIndex < numberOfBones; ++boneIndex)
        {
            m_ShaderData.m_FinalJointsMatrices[boneIndex] = glm::mat4(1.0f);
        }
    }
    else
    {
        // STEP 1: apply animation results
        for (int16_t boneIndex = 0; boneIndex < numberOfBones; ++boneIndex)
        {
            m_ShaderData.m_FinalJointsMatrices[boneIndex] = bones[boneIndex].getAnimatedBindMatrix();
        }

        // STEP 2: recursively update final joint matrices
        UpdateBone(0);

        // STEP 3: bring back into model space
        for (int16_t boneIndex = 0; boneIndex < numberOfBones; ++boneIndex)
        {
            m_ShaderData.m_FinalJointsMatrices[boneIndex] =
                m_ShaderData.m_FinalJointsMatrices[boneIndex] * bones[boneIndex].inverseBindMatrix;
        }
    }
}

// Update the final joint matrices of all joints
// traverses entire skeleton from top (a.k.a root a.k.a hip bone)
// This way, it is guaranteed that the global parent transform is already updated
void NtModel::Skeleton::UpdateBone(int16_t boneIndex)
{
    auto& currentBone = bones[boneIndex]; // just a reference for easier code

    int16_t parentBone = currentBone.parentIndex;
    if (parentBone != -1)
    {
        m_ShaderData.m_FinalJointsMatrices[boneIndex] =
             m_ShaderData.m_FinalJointsMatrices[parentBone] * m_ShaderData.m_FinalJointsMatrices[boneIndex];
    }

    // update children
    size_t numberOfChildren = currentBone.childrenIndices.size();
    for (size_t childIndex = 0; childIndex < numberOfChildren; ++childIndex)
    {
        int childJoint = currentBone.childrenIndices[childIndex];
        UpdateBone(childJoint);
    }
}

void NtModel::updateSkeleton() {
    if (!skeleton.has_value()) {
        NT_LOG_WARN(LogAssets, "No skeleton");
        return;
    }

    if (!boneBuffer) {
        NT_LOG_WARN(LogAssets, "No bone buffer");
        return;
    }

    skeleton->Update();

    if (skeleton->m_ShaderData.m_FinalJointsMatrices.empty()) {
        NT_LOG_WARN(LogAssets, "m_FinalJointsMatrices is empty!");
        return;
    }

    boneBuffer->writeToBuffer((void*)skeleton->m_ShaderData.m_FinalJointsMatrices.data());
    boneBuffer->flush();
}

uint32_t NtModel::getMaterialIndex(uint32_t meshIndex) const {
  if (meshIndex >= meshes.size()) {
    return 0; // Default to first material if mesh index is out of range
  }

  return meshes[meshIndex].materialIndex;
}

void NtModel::Builder::calculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
  // Calculate tangents using the method described in "Mathematics for 3D Game Programming and Computer Graphics"
  for (size_t i = 0; i < indices.size(); i += 3) {
    uint32_t i0 = indices[i];
    uint32_t i1 = indices[i + 1];
    uint32_t i2 = indices[i + 2];

    Vertex& v0 = vertices[i0];
    Vertex& v1 = vertices[i1];
    Vertex& v2 = vertices[i2];

    glm::vec3 edge1 = v1.position - v0.position;
    glm::vec3 edge2 = v2.position - v0.position;

    glm::vec2 deltaUV1 = v1.uv - v0.uv;
    glm::vec2 deltaUV2 = v2.uv - v0.uv;

    float denominator = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;

    if (abs(denominator) < 1e-6f) {
      // Degenerate triangle, set default tangent
      v0.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
      v1.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
      v2.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
      continue;
    }

    float f = 1.0f / denominator;

    glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
    glm::vec3 bitangent = f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);

    // Normalize tangent
    tangent = glm::normalize(tangent);

    // Calculate handedness
    glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
    float handedness = (glm::dot(glm::cross(tangent, bitangent), normal) < 0.0f) ? -1.0f : 1.0f;

    // Store tangent with handedness
    glm::vec4 tangentWithHandedness = glm::vec4(tangent, handedness);

    // Add to vertices (will be averaged later for shared vertices)
    v0.tangent += tangentWithHandedness;
    v1.tangent += tangentWithHandedness;
    v2.tangent += tangentWithHandedness;
  }

  // Normalize all tangents
  for (auto& vertex : vertices) {
    if (glm::length(glm::vec3(vertex.tangent)) > 0.0f) {
      float handedness = vertex.tangent.w;
      glm::vec3 tangent = glm::normalize(glm::vec3(vertex.tangent));
      vertex.tangent = glm::vec4(tangent, handedness > 0.0f ? 1.0f : -1.0f);
    } else {
      vertex.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    }
  }
}

// Helper functions
/*std::unique_ptr<NtModel> NtModel::createGOPlane(float size) {
  NtModel::Builder modelData{ntDevice};
  modelData.l_meshes.resize(1);

  // Quad vertices (using a plane in the XZ plane)
  modelData.l_meshes[0].vertices = {
    {{-size, 0.0f, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size, 0.0f, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size, 0.0f,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size, 0.0f,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}
  };

  modelData.l_meshes[0].indices = {0, 1, 2, 2, 3, 0};

  return std::make_unique<NtModel>(ntDevice, modelData);
}

std::unique_ptr<NtModel> NtModel::createGOCube(float size) {
  NtModel::Builder modelData{ntDevice};
  modelData.l_meshes.resize(1);

  // Cube vertices
  modelData.l_meshes[0].vertices = {
    // Front face
    {{-size, -size, size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size, -size, size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size,  size, size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size,  size, size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

    // Back face
    {{ size, -size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size, -size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size,  size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size,  size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},

    // Left face
    {{-size, -size, -size}, {1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-size, -size,  size}, {1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-size,  size,  size}, {1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-size,  size, -size}, {1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},

    // Right face
    {{ size, -size,  size}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
    {{ size, -size, -size}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
    {{ size,  size, -size}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
    {{ size,  size,  size}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},

    // Top face
    {{-size,  size,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size,  size,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size,  size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size,  size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

    // Bottom face
    {{-size, -size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size, -size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size, -size,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size, -size,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}}
  };

  modelData.l_meshes[0].indices = {
    // Front face
    0, 1, 2, 2, 3, 0,
    // Back face
    4, 5, 6, 6, 7, 4,
    // Left face
    8, 9, 10, 10, 11, 8,
    // Right face
    12, 13, 14, 14, 15, 12,
    // Top face
    16, 17, 18, 18, 19, 16,
    // Bottom face
    20, 21, 22, 22, 23, 20
  };

  return std::make_unique<NtModel>(ntDevice, modelData);
  }

std::unique_ptr<NtModel> NtModel::createBillboardQuad(float size) {
  NtModel::Builder modelData{ntDevice};
  modelData.l_meshes.resize(1);

  // Billboard quad vertices (facing forward, centered at origin)
  modelData.l_meshes[0].vertices = {
    {{-size, -size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Bottom-left
    {{ size, -size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Bottom-right
    {{ size,  size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Top-right
    {{-size,  size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}   // Top-left
  };

  modelData.l_meshes[0].indices = {0, 1, 2, 2, 3, 0};

  modelData.l_materials.resize(1);
  NtMaterial::MaterialData materialData;
  materialData.name = "BillboardMaterial";
  materialData.pbrMetallicRoughness.baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
  modelData.l_materials[0] = std::make_shared<NtMaterial>(ntDevice, materialData);

  // Update the material with descriptor set
  modelData.l_materials[0]->updateDescriptorSet(modelSetLayout->getDescriptorSetLayout(), modelPool->getDescriptorPool());

  return std::make_unique<NtModel>(ntDevice, modelData);
}

std::unique_ptr<NtModel> NtModel::createBillboardQuadWithTexture(float size, std::shared_ptr<NtImage> texture) {
  NtModel::Builder modelData{ntDevice};
  modelData.l_meshes.resize(1);

  // Billboard quad vertices (facing forward, centered at origin)
  modelData.l_meshes[0].vertices = {
    {{-size, -size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Bottom-left
    {{ size, -size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Bottom-right
    {{ size,  size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Top-right
    {{-size,  size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}   // Top-left
  };

  modelData.l_meshes[0].indices = {0, 1, 2, 2, 3, 0};

  modelData.l_materials.resize(1);
  NtMaterial::MaterialData materialData;
  materialData.name = "BillboardMaterial";
  materialData.pbrMetallicRoughness.baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
  materialData.pbrMetallicRoughness.baseColorTexture = texture;
  modelData.l_materials[0] = std::make_shared<NtMaterial>(ntDevice, materialData);

  // Update the material with descriptor set
  modelData.l_materials[0]->updateDescriptorSet(modelSetLayout->getDescriptorSetLayout(), modelPool->getDescriptorPool());

  return std::make_unique<NtModel>(ntDevice, modelData);
  }*/

}
