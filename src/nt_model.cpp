#include "nt_model.hpp"
#include "imgui.h"
#include "nt_descriptors.hpp"
#include "nt_utils.hpp"
#include <memory>
#include <ostream>
#include <stdexcept>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"
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
  } else if (extension == "obj") {
    builder.loadObjModel(filepath);
  } else {
    throw std::runtime_error("Unsupported file format: " + extension + ". Supported formats are: .obj, .gltf, .glb");
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
        std::cerr << "Cannot update bone buffer: buffer doesn't exist!" << std::endl;
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

void NtModel::Builder::loadObjModel(const std::string &filepath) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  // Extract directory path for relative texture paths
  std::string baseDir = filepath.substr(0, filepath.find_last_of('/') + 1);

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str(), baseDir.c_str())) {
    throw std::runtime_error(warn + err);
  }

  l_meshes.clear();

  // Convert tinyobj materials to NtMaterials
  std::cout << "  Loading " << materials.size() << " materials from OBJ" << std::endl;
  for (size_t i = 0; i < materials.size(); ++i) {
    const auto& objMaterial = materials[i];
    NtMaterial::MaterialData materialData;

    materialData.name = objMaterial.name.empty() ? ("Material_" + std::to_string(i)) : objMaterial.name;
    std::cout << "    Loading material: " << materialData.name << std::endl;

    // Basic PBR conversion from OBJ material
    materialData.pbrMetallicRoughness.baseColorFactor = glm::vec4(
      objMaterial.diffuse[0], objMaterial.diffuse[1], objMaterial.diffuse[2], 1.0f);

    // Approximate roughness and metallic from OBJ shininess
    materialData.pbrMetallicRoughness.roughnessFactor = 1.0f - (objMaterial.shininess / 1000.0f);
    materialData.pbrMetallicRoughness.metallicFactor = 0.0f; // OBJ doesn't have metallic

    // Load diffuse texture as base color
    if (!objMaterial.diffuse_texname.empty()) {
      std::string texturePath;
      // Check if path is absolute (starts with /) or relative
      if (objMaterial.diffuse_texname[0] == '/') {
        texturePath = objMaterial.diffuse_texname;
      } else {
        texturePath = baseDir + objMaterial.diffuse_texname;
      }
      std::cout << "      Loading diffuse texture: " << texturePath << std::endl;
      try {
        materialData.pbrMetallicRoughness.baseColorTexture =
          NtImage::createTextureFromFile(ntDevice, texturePath);
      } catch (const std::exception& e) {
        std::cerr << "      Failed to load diffuse texture: " << e.what() << std::endl;
      }
    }

    // Load normal map
    if (!objMaterial.normal_texname.empty()) {
      std::string texturePath;
      // Check if path is absolute (starts with /) or relative
      if (objMaterial.normal_texname[0] == '/') {
        texturePath = objMaterial.normal_texname;
      } else {
        texturePath = baseDir + objMaterial.normal_texname;
      }
      std::cout << "      Loading normal texture: " << texturePath << std::endl;
      try {
        materialData.normalTexture = NtImage::createTextureFromFile(ntDevice, texturePath);
      } catch (const std::exception& e) {
        std::cerr << "      Failed to load normal texture: " << e.what() << std::endl;
      }
    }

    l_materials.push_back(std::make_shared<NtMaterial>(ntDevice, materialData));
  }

  // If no materials found, create a default material
  if (l_materials.empty()) {
    std::cout << "    No materials found, creating default material" << std::endl;
    NtMaterial::MaterialData defaultMaterial;
    defaultMaterial.name = "DefaultMaterial";
    defaultMaterial.pbrMetallicRoughness.baseColorFactor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    l_materials.push_back(std::make_shared<NtMaterial>(ntDevice, defaultMaterial));
  }

  // Create meshes per shape/material combination
  for (size_t s = 0; s < shapes.size(); ++s) {
    const auto &shape = shapes[s];

    // Group faces by material
    std::map<int, std::vector<tinyobj::index_t>> materialGroups;
    for (size_t f = 0; f < shape.mesh.material_ids.size(); ++f) {
      int materialId = shape.mesh.material_ids[f];
      if (materialId < 0) materialId = 0; // Use first material for faces without material

      // Add the 3 indices for this face
      materialGroups[materialId].push_back(shape.mesh.indices[3 * f + 0]);
      materialGroups[materialId].push_back(shape.mesh.indices[3 * f + 1]);
      materialGroups[materialId].push_back(shape.mesh.indices[3 * f + 2]);
    }

    // Create a mesh for each material group
    for (const auto& [materialId, indices] : materialGroups) {
      Mesh mesh;
      mesh.name = shape.name.empty() ? ("Shape_" + std::to_string(s)) : shape.name;
      if (materialGroups.size() > 1) {
        mesh.name += "_Mat" + std::to_string(materialId);
      }
      mesh.materialIndex = std::min(static_cast<uint32_t>(materialId), static_cast<uint32_t>(l_materials.size() - 1));

      std::unordered_map<Vertex, uint32_t> uniqueVertices{};
      for (const auto &index : indices) {
      Vertex vertex{};

      if (index.vertex_index >= 0) {
        vertex.position = {
          attrib.vertices[3 * index.vertex_index + 0],
          attrib.vertices[3 * index.vertex_index + 1],
          attrib.vertices[3 * index.vertex_index + 2],
        };

        vertex.color = {
          attrib.colors[3 * index.vertex_index + 0],
          attrib.colors[3 * index.vertex_index + 1],
          attrib.colors[3 * index.vertex_index + 2],
        };
      }

      if (index.normal_index >= 0) {
        vertex.normal = {
          attrib.normals[3 * index.normal_index + 0],
          attrib.normals[3 * index.normal_index + 1],
          attrib.normals[3 * index.normal_index + 2],
        };
      }

      if (index.texcoord_index >= 0) {
        vertex.uv = {
          attrib.texcoords[2 * index.texcoord_index + 0],
          attrib.texcoords[2 * index.texcoord_index + 1],
        };
      }

      // Default tangent for OBJ files
      vertex.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

        if (uniqueVertices.count(vertex) == 0) {
          uniqueVertices[vertex] = static_cast<uint32_t>(mesh.vertices.size());
          mesh.vertices.push_back(vertex);
        }
        mesh.indices.push_back(uniqueVertices[vertex]);
      }

      // Calculate tangents for OBJ models that don't have them
      if (!mesh.indices.empty()) {
        calculateTangents(mesh.vertices, mesh.indices);
      }

      l_meshes.push_back(mesh);
    }
  }

  std::cout << "  Loaded " << l_meshes.size()-1 << " mesh(es) with " << l_materials.size() << " material(s)" << std::endl;
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
    std::cout << "Warn: " << warn << std::endl;
  }

  if (!err.empty()) {
    std::cerr << "Err: " << err << std::endl;
  }

  if (!ret) {
    throw std::runtime_error("Failed to parse glTF file");
  }

  std::cout << "Successfully loaded glTF file: " << filepath << std::endl;
  std::cout << "  Meshes: " << model.meshes.size() << std::endl;
  std::cout << "  Materials: " << model.materials.size() << std::endl;
  std::cout << "  Textures: " << model.textures.size() << std::endl;
  std::cout << "  Animations: " << model.animations.size() << std::endl;

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

    std::cout << "  Loading material: " << (material.name.empty() ? "<unnamed>" : material.name) << std::endl;

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
        std::cout << "    Loading base color texture: " << texturePath << std::endl;
        try {
          materialData.pbrMetallicRoughness.baseColorTexture =
            NtImage::createTextureFromFile(ntDevice, texturePath);
          materialData.pbrMetallicRoughness.baseColorTexCoord = pbr.baseColorTexture.texCoord;
        } catch (const std::exception& e) {
          std::cerr << "    Failed to load base color texture: " << e.what() << std::endl;
        }
      } else if (!image.image.empty()) {
          // Embedded texture - so let's create texture from memory
          // std::cout << "    Loading embedded base color texture, size: " << image.image.size() << " bytes" << std::endl;
          // std::cout << "    Image format: " << image.mimeType << std::endl;
          // std::cout << "    Texture coord index: " << materialData.pbrMetallicRoughness.baseColorTexCoord << std::endl;
          try {
            materialData.pbrMetallicRoughness.baseColorTexture =
              NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size());
            materialData.pbrMetallicRoughness.baseColorTexCoord = pbr.baseColorTexture.texCoord;
          } catch (const std::exception& e) {
            std::cerr << "    Failed to load embedded base color texture: " << e.what() << std::endl;
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
                    std::cout << "    UV Scale: " << materialData.uvScale.x << ", "
                            << materialData.uvScale.y << std::endl;
                }
            }

            // Parse offset
            if (transform.Has("offset") && transform.Get("offset").IsArray()) {
                auto offsetArray = transform.Get("offset").Get<tinygltf::Value::Array>();
                if (offsetArray.size() >= 2) {
                    materialData.uvOffset.x = offsetArray[0].GetNumberAsDouble();
                    materialData.uvOffset.y = offsetArray[1].GetNumberAsDouble();
                    std::cout << "    UV Offset: " << materialData.uvOffset.x << ", "
                            << materialData.uvOffset.y << std::endl;
                }
            }

            // Parse rotation
            if (transform.Has("rotation")) {
                materialData.uvRotation = transform.Get("rotation").GetNumberAsDouble();
                std::cout << "    UV Rotation: " << materialData.uvRotation << " radians" << std::endl;
            }
        }
    }

    // Load metallic-roughness texture
    if (pbr.metallicRoughnessTexture.index >= 0) {
      const auto &texture = model.textures[pbr.metallicRoughnessTexture.index];
      const auto &image = model.images[texture.source];

      if (!image.uri.empty()) {
        std::string texturePath = baseDir + image.uri;
        std::cout << "    Loading metallic-roughness texture: " << texturePath << std::endl;
        try {
          materialData.pbrMetallicRoughness.metallicRoughnessTexture =
            NtImage::createTextureFromFile(ntDevice, texturePath, true);
          materialData.pbrMetallicRoughness.metallicRoughnessTexCoord = pbr.metallicRoughnessTexture.texCoord;
        } catch (const std::exception& e) {
          std::cerr << "    Failed to load metallic-roughness texture: " << e.what() << std::endl;
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
        std::cout << "    Loading normal texture: " << texturePath << std::endl;
        try {
          materialData.normalTexture = NtImage::createTextureFromFile(ntDevice, texturePath, true);
          materialData.normalScale = material.normalTexture.scale;
          materialData.normalTexCoord = material.normalTexture.texCoord;
        } catch (const std::exception& e) {
          std::cerr << "    Failed to load normal texture: " << e.what() << std::endl;
        }
      } else if (!image.image.empty()) {
        // Embedded texture - so let's create texture from memory
        materialData.normalTexture =
            NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size(), true);
        materialData.normalScale = material.normalTexture.scale;
        materialData.normalTexCoord = material.normalTexture.texCoord;
    }
    }

    // Load occlusion texture
    // if (material.occlusionTexture.index >= 0) {
    //   const auto &texture = model.textures[material.occlusionTexture.index];
    //   const auto &image = model.images[texture.source];

    //   if (!image.uri.empty()) {
    //     std::string texturePath = baseDir + image.uri;
    //     std::cout << "    Loading occlusion texture: " << texturePath << std::endl;
    //     try {
    //       materialData.occlusionTexture = NtImage::createTextureFromFile(ntDevice, texturePath);
    //       materialData.occlusionStrength = material.occlusionTexture.strength;
    //       materialData.occlusionTexCoord = material.occlusionTexture.texCoord;
    //     } catch (const std::exception& e) {
    //       std::cerr << "    Failed to load occlusion texture: " << e.what() << std::endl;
    //     }
    //   } else if (!image.image.empty()) {
    //     // Embedded texture - so let's create texture from memory
    //     materialData.occlusionTexture =
    //         NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size());
    //     materialData.occlusionStrength = material.occlusionTexture.strength;
    //     materialData.occlusionTexCoord = pbr.baseColorTexture.texCoord;
    // }
    // }

    // Load emissive texture
    // if (material.emissiveTexture.index >= 0) {
    //   const auto &texture = model.textures[material.emissiveTexture.index];
    //   const auto &image = model.images[texture.source];

    //   if (!image.uri.empty()) {
    //     std::string texturePath = baseDir + image.uri;
    //     std::cout << "    Loading emissive texture: " << texturePath << std::endl;
    //     try {
    //       materialData.emissiveTexture = NtImage::createTextureFromFile(ntDevice, texturePath);
    //       materialData.emissiveTexCoord = material.emissiveTexture.texCoord;
    //     } catch (const std::exception& e) {
    //       std::cerr << "    Failed to load emissive texture: " << e.what() << std::endl;
    //     }
    //   } else if (!image.image.empty()) {
    //         // Embedded texture - so let's create texture from memory
    //         materialData.emissiveTexture =
    //             NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size());
    //         materialData.emissiveTexCoord = material.emissiveTexture.texCoord;
    //     }
    // }

    // // Emissive factor
    // materialData.emissiveFactor = glm::vec3(
    //   material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2]
    // );

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
    std::cout << "    No materials found, creating default material" << std::endl;
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
        std::cout << "A model should only have a single skin/armature/skeleton. Using skin 0.";

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
            std::cout << "  Loading skeleton: " << l_skeleton->name << std::endl;

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


    std::cout << "    Bones: " << l_skeleton->bones.size() << std::endl;
}

void NtModel::Builder::loadGltfBone(const tinygltf::Model &model, int globalGltfNodeIndex, int parentBone) {
    int currentBone = l_skeleton->nodeIndexToBoneIndex[globalGltfNodeIndex];
    auto& bone = l_skeleton->bones[currentBone];

    bone.parentIndex = parentBone;

    size_t numChildren = model.nodes[globalGltfNodeIndex].children.size();
    if (numChildren > 0) {
        bone.childrenIndices.resize(numChildren);
        for (size_t childIndex = 0; childIndex < numChildren; ++childIndex) {
            uint childGlobalIndex = model.nodes[globalGltfNodeIndex].children[childIndex];
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
    std::cout << "  Animation: " << animation.name << " (" << animation.duration << "s)\n";
}

void NtModel::Skeleton::Traverse()
{
    std::cout << "skeleton: " << name << std::endl;
    uint indent = 0;
    std::string indentStr(indent, ' ');
    auto& joint = bones[0]; // root joint
    Traverse(joint, indent + 1);
}

void NtModel::Skeleton::Traverse(Bone const& bone, uint indent)
{
    std::string indentStr(indent, ' ');
    size_t numberOfChildren = bone.childrenIndices.size();
    std::cout << indentStr <<" Name: " << bone.name << " Parent: " << bone.parentIndex << " Children: " << numberOfChildren << std::endl;

    for (size_t childIndex = 0; childIndex < numberOfChildren; ++childIndex)
    {
        int jointIndex = bone.childrenIndices[childIndex];
        std::cout << indentStr <<" Child: " << childIndex << " Index: " << jointIndex << std::endl;
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
        std::cout << "[DEBUG] No skeleton" << std::endl;
        return;
    }

    if (!boneBuffer) {
        std::cout << "[DEBUG] No bone buffer" << std::endl;
        return;
    }

    skeleton->Update();

    if (skeleton->m_ShaderData.m_FinalJointsMatrices.empty()) {
        std::cout << "[DEBUG] Warning: m_FinalJointsMatrices is empty!" << std::endl;
        return;
    }

    // VALIDATION: Check for invalid matrices
    // for (size_t i = 0; i < 3 && i < skeleton->m_ShaderData.m_FinalJointsMatrices.size(); ++i) {
    //     const auto& mat = skeleton->m_ShaderData.m_FinalJointsMatrices[i];
    //     std::cout << "Bone " << i << " matrix:\n";
    //     std::cout << "  [" << mat[0][0] << ", " << mat[1][0] << ", " << mat[2][0] << ", " << mat[3][0] << "]\n";
    //     std::cout << "  [" << mat[0][1] << ", " << mat[1][1] << ", " << mat[2][1] << ", " << mat[3][1] << "]\n";
    //     std::cout << "  [" << mat[0][2] << ", " << mat[1][2] << ", " << mat[2][2] << ", " << mat[3][2] << "]\n";
    //     std::cout << "  [" << mat[0][3] << ", " << mat[1][3] << ", " << mat[2][3] << ", " << mat[3][3] << "]\n";

    //     // Check for NaN or extreme values
    //     for (int r = 0; r < 4; r++) {
    //         for (int c = 0; c < 4; c++) {
    //             if (std::isnan(mat[c][r]) || std::abs(mat[c][r]) > 1000.0f) {
    //                 std::cerr << "WARNING: Bone " << i << " has invalid value at [" << r << "][" << c << "]: " << mat[c][r] << std::endl;
    //             }
    //         }
    //     }
    // }

    boneBuffer->writeToBuffer((void*)skeleton->m_ShaderData.m_FinalJointsMatrices.data());
    boneBuffer->flush();
}

uint32_t NtModel::getMaterialIndex(uint32_t meshIndex) const {
  if (meshIndex >= meshes.size()) {
    return 0; // Default to first material if mesh index is out of range
  }

  return meshes[meshIndex].materialIndex;
}

// void NtModel::updateBoneMatrices(const std::vector<glm::mat4>& matrices) {
//     if (boneBuffer && matrices.size() == skeleton->bones.size()) {
//         boneBuffer->writeToBuffer((void*)matrices.data());
//         boneBuffer->flush();
//     }
// }

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

}
