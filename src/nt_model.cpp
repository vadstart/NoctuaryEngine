#include "nt_model.hpp"
#include "nt_utils.hpp"
#include <memory>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tinygltf/tiny_gltf.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

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

NtModel::NtModel(NtDevice &device, NtModel::Data &data) : ntDevice{device} {
  createMeshBuffers(data.meshes);
  materials = data.materials;
}

NtModel::~NtModel() {}

std::unique_ptr<NtModel> NtModel::createModelFromFile(NtDevice &device, const std::string &filepath, VkDescriptorSetLayout materialLayout, VkDescriptorPool materialPool) {
  Data data{device};

  // Determine file type by extension
  std::string extension = filepath.substr(filepath.find_last_of('.') + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  if (extension == "gltf" || extension == "glb") {
    data.loadGltfModel(filepath);
  } else if (extension == "obj") {
    data.loadObjModel(filepath);
  } else {
    throw std::runtime_error("Unsupported file format: " + extension + ". Supported formats are: .obj, .gltf, .glb");
  }

  // Initialize material descriptor sets for all loaded materials
  for (const auto& material : data.materials) {
    material->updateDescriptorSet(materialLayout, materialPool);
  }

  return std::make_unique<NtModel>(device, data);
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

  return attributeDescriptions;
}

void NtModel::Data::loadObjModel(const std::string &filepath) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  // Extract directory path for relative texture paths
  std::string baseDir = filepath.substr(0, filepath.find_last_of('/') + 1);

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str(), baseDir.c_str())) {
    throw std::runtime_error(warn + err);
  }

  meshes.clear();
  this->materials.clear();

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

    this->materials.push_back(std::make_shared<NtMaterial>(ntDevice, materialData));
  }

  // If no materials found, create a default material
  if (this->materials.empty()) {
    std::cout << "    No materials found, creating default material" << std::endl;
    NtMaterial::MaterialData defaultMaterial;
    defaultMaterial.name = "DefaultMaterial";
    defaultMaterial.pbrMetallicRoughness.baseColorFactor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    this->materials.push_back(std::make_shared<NtMaterial>(ntDevice, defaultMaterial));
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
      mesh.materialIndex = std::min(static_cast<uint32_t>(materialId), static_cast<uint32_t>(this->materials.size() - 1));

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

      // Default tangent for OBJ files (will be calculated later if needed)
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

      meshes.push_back(mesh);
    }
  }

  std::cout << "  Loaded " << meshes.size() << " mesh(es) with " << this->materials.size() << " material(s)" << std::endl;
}

void NtModel::Data::loadGltfModel(const std::string &filepath) {
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
  std::cout << "  Materials: " << model.materials.size() << std::endl;
  std::cout << "  Meshes: " << model.meshes.size() << std::endl;
  std::cout << "  Textures: " << model.textures.size() << std::endl;

  // Load materials first
  loadGltfMaterials(model, filepath);

  // Load meshes
  loadGltfMeshes(model);

  // Apply coordinate system transformation from glTF (Y-up) to engine coordinate system
  // glTF uses Y-up, but when exported from Blender with Z-up, we need to rotate around X-axis
  glm::mat4 transform = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
  glm::mat3 normalTransform = glm::transpose(glm::inverse(glm::mat3(transform)));

  for (auto& mesh : meshes) {
    for (auto& vertex : mesh.vertices) {
      // Transform position
      glm::vec4 pos = transform * glm::vec4(vertex.position, 1.0f);
      vertex.position = glm::vec3(pos);

      // Transform normal
      vertex.normal = normalTransform * vertex.normal;

      // Transform tangent
      glm::vec4 tangent = transform * glm::vec4(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z, 0.0f);
      vertex.tangent = glm::vec4(tangent.x, tangent.y, tangent.z, vertex.tangent.w);
    }
  }

  std::cout << "  Loaded " << meshes.size() << " mesh(es) with " << materials.size() << " material(s)" << std::endl;
}

void NtModel::Data::loadGltfMaterials(const tinygltf::Model &model, const std::string &filepath) {
  std::string baseDir = filepath.substr(0, filepath.find_last_of('/') + 1);

  for (const auto &material : model.materials) {
    NtMaterial::MaterialData materialData;
    materialData.name = material.name;

    // std::cout << "  Loading material: " << (material.name.empty() ? "<unnamed>" : material.name) << std::endl;

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
            NtImage::createTextureFromFile(ntDevice, texturePath);
          materialData.pbrMetallicRoughness.metallicRoughnessTexCoord = pbr.metallicRoughnessTexture.texCoord;
        } catch (const std::exception& e) {
          std::cerr << "    Failed to load metallic-roughness texture: " << e.what() << std::endl;
        }
      } else if (!image.image.empty()) {
        // Embedded texture - so let's create texture from memory
        materialData.pbrMetallicRoughness.metallicRoughnessTexture =
            NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size());
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
          materialData.normalTexture = NtImage::createTextureFromFile(ntDevice, texturePath);
          materialData.normalScale = material.normalTexture.scale;
          materialData.normalTexCoord = material.normalTexture.texCoord;
        } catch (const std::exception& e) {
          std::cerr << "    Failed to load normal texture: " << e.what() << std::endl;
        }
      } else if (!image.image.empty()) {
        // Embedded texture - so let's create texture from memory
        materialData.normalTexture =
            NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size());
        materialData.normalScale = material.normalTexture.scale;
        materialData.normalTexCoord = material.normalTexture.texCoord;
    }
    }

    // Load occlusion texture
    if (material.occlusionTexture.index >= 0) {
      const auto &texture = model.textures[material.occlusionTexture.index];
      const auto &image = model.images[texture.source];

      if (!image.uri.empty()) {
        std::string texturePath = baseDir + image.uri;
        std::cout << "    Loading occlusion texture: " << texturePath << std::endl;
        try {
          materialData.occlusionTexture = NtImage::createTextureFromFile(ntDevice, texturePath);
          materialData.occlusionStrength = material.occlusionTexture.strength;
          materialData.occlusionTexCoord = material.occlusionTexture.texCoord;
        } catch (const std::exception& e) {
          std::cerr << "    Failed to load occlusion texture: " << e.what() << std::endl;
        }
      } else if (!image.image.empty()) {
        // Embedded texture - so let's create texture from memory
        materialData.occlusionTexture =
            NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size());
        materialData.occlusionStrength = material.occlusionTexture.strength;
        materialData.occlusionTexCoord = pbr.baseColorTexture.texCoord;
    }
    }

    // Load emissive texture
    if (material.emissiveTexture.index >= 0) {
      const auto &texture = model.textures[material.emissiveTexture.index];
      const auto &image = model.images[texture.source];

      if (!image.uri.empty()) {
        std::string texturePath = baseDir + image.uri;
        std::cout << "    Loading emissive texture: " << texturePath << std::endl;
        try {
          materialData.emissiveTexture = NtImage::createTextureFromFile(ntDevice, texturePath);
          materialData.emissiveTexCoord = material.emissiveTexture.texCoord;
        } catch (const std::exception& e) {
          std::cerr << "    Failed to load emissive texture: " << e.what() << std::endl;
        }
      } else if (!image.image.empty()) {
            // Embedded texture - so let's create texture from memory
            materialData.emissiveTexture =
                NtImage::createTextureFromMemory(ntDevice, image.image.data(), image.image.size());
            materialData.emissiveTexCoord = material.emissiveTexture.texCoord;
        }
    }

    // Emissive factor
    materialData.emissiveFactor = glm::vec3(
      material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2]
    );

    // Alpha mode
    if (material.alphaMode == "OPAQUE") {
      materialData.alphaMode = NtMaterial::AlphaMode::OPAQUE;
    } else if (material.alphaMode == "MASK") {
      materialData.alphaMode = NtMaterial::AlphaMode::MASK;
    } else if (material.alphaMode == "BLEND") {
      materialData.alphaMode = NtMaterial::AlphaMode::BLEND;
    }

    materialData.alphaCutoff = material.alphaCutoff;
    materialData.doubleSided = material.doubleSided;

    materials.push_back(std::make_shared<NtMaterial>(ntDevice, materialData));
  }

  // Create a default material if none exist
  if (materials.empty()) {
    NtMaterial::MaterialData defaultMaterial;
    defaultMaterial.name = "Default";
    materials.push_back(std::make_shared<NtMaterial>(ntDevice, defaultMaterial));
  }
}

void NtModel::Data::loadGltfMeshes(const tinygltf::Model &model) {
  for (const auto &gltfMesh : model.meshes) {
    for (const auto &primitive : gltfMesh.primitives) {
      Mesh mesh;
      mesh.name = gltfMesh.name;
      mesh.materialIndex = primitive.material >= 0 ? primitive.material : 0;

      // std::cout << "  Loading mesh: " << (gltfMesh.name.empty() ? "<unnamed>" : gltfMesh.name)
      //           << " with material index: " << mesh.materialIndex << std::endl;

      // Debug vertex attributes - can be enabled for troubleshooting
      // if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
      //   std::cout << "    Has TEXCOORD_0" << std::endl;
      // }
      // if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
      //   std::cout << "    Has TANGENT attribute" << std::endl;
      // } else {
      //   std::cout << "    No TANGENT attribute found" << std::endl;
      // }

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
          // Debug first few UV coordinates
          // if (i < 5) {
          //   std::cout << "    Vertex " << i << " UV: (" << vertex.uv.x << ", " << vertex.uv.y << ")" << std::endl;
          // }
        } else {
          vertex.uv = glm::vec2(0.0f, 0.0f);
        }

        // Tangent
        if (tangentData) {
          vertex.tangent = glm::vec4(tangentData[i * 4], tangentData[i * 4 + 1],
                                    tangentData[i * 4 + 2], tangentData[i * 4 + 3]);
          // Debug first few tangent vectors - can be enabled for troubleshooting
          // if (i < 3) {
          //   std::cout << "    Vertex " << i << " Tangent: (" << vertex.tangent.x << ", " << vertex.tangent.y << ", " << vertex.tangent.z << ", " << vertex.tangent.w << ")" << std::endl;
          // }
        } else {
          vertex.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
          // Debug default tangent - can be enabled for troubleshooting
          // if (i < 3) {
          //   std::cout << "    Vertex " << i << " Tangent: DEFAULT (no tangent data)" << std::endl;
          // }
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

      // std::cout << "    Vertices: " << mesh.vertices.size() << ", Indices: " << mesh.indices.size() << std::endl;
      meshes.push_back(mesh);
    }
  }
}

uint32_t NtModel::getMaterialIndex(uint32_t meshIndex) const {
  if (meshIndex >= meshes.size()) {
    return 0; // Default to first material if mesh index is out of range
  }

  return meshes[meshIndex].materialIndex;
}

void NtModel::Data::calculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
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
