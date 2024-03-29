#include "GLBLoader.hpp"

#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>
#include <meshoptimizer.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "vulkancore/Buffer.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/Sampler.hpp"
#include "vulkancore/Texture.hpp"
#include "vulkancore/Utility.hpp"

static int modelId = 0;

namespace {
std::vector<uint8_t> imageData(Microsoft::glTF::GLBResourceReader* resourceReader,
                               Microsoft::glTF::Document& document,
                               const std::string& imageId) {
  auto& image = document.images.Get(imageId);
  auto imageBufferView = document.bufferViews.Get(image.bufferViewId);
  auto imageData = resourceReader->ReadBinaryData<uint8_t>(document, imageBufferView);
  return imageData;
}
}  // namespace

namespace EngineCore {

class StreamReader : public Microsoft::glTF::IStreamReader {
 public:
  StreamReader(std::filesystem::path pathBase) : m_pathBase(std::move(pathBase)) {
    ASSERT(m_pathBase.has_root_path(),
           "StreamReader requires an absolute path. The provided path is relative.");
  }

  std::shared_ptr<std::istream> GetInputStream(
      const std::string& filename) const override {
    auto streamPath = m_pathBase / std::filesystem::u8path(filename);
    auto stream = std::make_shared<std::ifstream>(streamPath, std::ios_base::binary);

    return stream;
  }

 private:
  std::filesystem::path m_pathBase;
};

class InMemoryStream : public Microsoft::glTF::IStreamReader {
 public:
  InMemoryStream(std::shared_ptr<std::stringstream> stream) : m_stream(stream) {}

  std::shared_ptr<std::istream> GetInputStream(const std::string&) const override {
    return m_stream;
  }

 private:
  std::shared_ptr<std::stringstream> m_stream;
};

int imageDataAsync(std::filesystem::path pathCurrent,
                   std::shared_ptr<Microsoft::glTF::Document> document, int index,
                   const std::string& imageId, std::shared_ptr<EngineCore::Model> model,
                   int modelId, std::function<void(int, int)> callback) {
  auto streamReader = std::make_shared<StreamReader>(pathCurrent.parent_path());
  auto stream = streamReader->GetInputStream(pathCurrent.filename().string());

  auto resourceReader =
      Microsoft::glTF::GLBResourceReader(std::move(streamReader), std::move(stream));

  auto& image = document->images.Get(imageId);
  auto imageBufferView = document->bufferViews.Get(image.bufferViewId);
  std::vector<uint8_t> imageData =
      resourceReader.ReadBinaryData<uint8_t>(*document, imageBufferView);

  model->textures[index] =
      std::move(std::make_unique<EngineCore::stbImageData>(imageData));
  callback(index, modelId);
  return index;
}

std::shared_ptr<Model> GLBLoader::load(const std::vector<char>& buffer) {
  std::shared_ptr<Model> outModel = std::make_shared<Model>();
  Model& outputModel = *outModel.get();

  std::shared_ptr<std::stringstream> sstream = std::make_shared<std::stringstream>();
  for (const auto c : buffer) {
    *sstream << c;
  }

  auto streamReader = std::make_shared<InMemoryStream>(sstream);
  auto stream = streamReader->GetInputStream("");
  auto resourceReader = std::make_shared<Microsoft::glTF::GLBResourceReader>(
      std::move(streamReader), std::move(stream));

  auto manifest = resourceReader->GetJson();

  auto document =
      std::make_shared<Microsoft::glTF::Document>(Microsoft::glTF::Deserialize(manifest));

  updateMeshData(resourceReader, document, outputModel);

  outputModel.textures.reserve(document->textures.Size());
  for (int i = 0; i < document->textures.Size(); ++i) {
    outputModel.textures.emplace_back(std::move(std::make_unique<stbImageData>(
        imageData(resourceReader.get(), *document, document->textures[i].imageId))));
  }

  updateMaterials(document, outputModel);

  return outModel;
}

std::shared_ptr<Model> GLBLoader::load(const std::string& filePath, BS::thread_pool& pool,
                                       std::function<void(int, int)> callback) {
  std::shared_ptr<Model> outModel = std::make_shared<Model>();
  Model& outputModel = *outModel.get();

  auto pathCurrent = std::filesystem::current_path();
  pathCurrent /= filePath;
  auto streamReader = std::make_shared<StreamReader>(pathCurrent.parent_path());
  auto stream = streamReader->GetInputStream(pathCurrent.filename().string());

  auto resourceReader =
      std::make_shared<Microsoft::glTF::GLBResourceReader>(streamReader, stream);

  auto manifest = resourceReader->GetJson();

  auto document =
      std::make_shared<Microsoft::glTF::Document>(Microsoft::glTF::Deserialize(manifest));

  updateMeshData(resourceReader, document, outputModel);

  outputModel.textures.resize(document->textures.Size());
  for (int i = 0; i < document->textures.Size(); ++i) {
    results_.emplace_back(pool.submit(imageDataAsync, pathCurrent, document, i,
                                      document->textures[i].imageId, outModel, modelId,
                                      callback));
  }

  updateMaterials(document, outputModel);

  modelId++;

  return outModel;
}

std::shared_ptr<EngineCore::Model> GLBLoader::load(const std::string& filePath) {
  std::shared_ptr<Model> outModel = std::make_shared<Model>();
  Model& outputModel = *outModel.get();

  auto pathCurrent = std::filesystem::current_path();
  pathCurrent /= filePath;
  auto streamReader = std::make_shared<StreamReader>(pathCurrent.parent_path());
  auto stream = streamReader->GetInputStream(pathCurrent.filename().string());

  auto resourceReader =
      std::make_shared<Microsoft::glTF::GLBResourceReader>(streamReader, stream);

  auto manifest = resourceReader->GetJson();

  auto document =
      std::make_shared<Microsoft::glTF::Document>(Microsoft::glTF::Deserialize(manifest));

  updateMeshData(resourceReader, document, outputModel);

  for (int i = 0; i < document->textures.Size(); ++i) {
    outputModel.textures.emplace_back(std::move(std::make_unique<stbImageData>(
        imageData(resourceReader.get(), *document, document->textures[i].imageId))));
  }

  updateMaterials(document, outputModel);

  return outModel;
}

void GLBLoader::updateMeshData(
    std::shared_ptr<Microsoft::glTF::GLBResourceReader> resourceReader,
    std::shared_ptr<Microsoft::glTF::Document> document, Model& outputModel) {
  uint32_t firstIndex = 0;
  uint32_t firstInstance = 0;

  for (size_t i = 0; i < document->nodes.Size(); ++i) {
    if (document->nodes[i].meshId.empty()) {
      continue;
    }
    uint32_t meshId = std::stoul(document->nodes[i].meshId);
    const Microsoft::glTF::Mesh& mesh = document->meshes[meshId];
    Mesh currentMesh;

    glm::mat4 m(1.0);
    if (document->nodes[i].matrix != Microsoft::glTF::Matrix4::IDENTITY) {
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          m[i][j] = document->nodes[i].matrix.values[i * 4 + j];
        }
      }
    } else if (!document->nodes[i].HasIdentityTRS()) {
      glm::mat4 matScale =
          glm::scale(glm::mat4(1.0f),
                     glm::vec3(document->nodes[i].scale.x, document->nodes[i].scale.y,
                               document->nodes[i].scale.z));

      glm::mat4 matRot = glm::mat4_cast(
          glm::quat(document->nodes[i].rotation.w, document->nodes[i].rotation.x,
                    document->nodes[i].rotation.y, document->nodes[i].rotation.z));

      glm::mat4 matTranslate =
          glm::translate(glm::mat4(1.0f), glm::vec3(document->nodes[i].translation.x,
                                                    document->nodes[i].translation.y,
                                                    document->nodes[i].translation.z));

      m = matTranslate * matScale;
    }

    for (auto& primitive : mesh.primitives) {
      std::string positionAccessorID;
      std::string normalAccessorID;
      std::string tangentAccessorID;
      std::string uvAccessorID;
      std::string uvAccessorID2;

      if (primitive.materialId != "") {
        currentMesh.material = document->materials.GetIndex(primitive.materialId);
      }

      if (primitive.TryGetAttributeAccessorId(Microsoft::glTF::ACCESSOR_POSITION,
                                              positionAccessorID) &&
          primitive.TryGetAttributeAccessorId(Microsoft::glTF::ACCESSOR_NORMAL,
                                              normalAccessorID)) {
        bool hasTangent = primitive.TryGetAttributeAccessorId(
            Microsoft::glTF::ACCESSOR_TANGENT, tangentAccessorID);
        bool hasUV = primitive.TryGetAttributeAccessorId(
            Microsoft::glTF::ACCESSOR_TEXCOORD_0, uvAccessorID);

        bool hasUV2 = primitive.TryGetAttributeAccessorId(
            Microsoft::glTF::ACCESSOR_TEXCOORD_1, uvAccessorID2);
        if (document->accessors.Has(primitive.indicesAccessorId) &&
            document->accessors.Has(positionAccessorID) &&
            document->accessors.Has(normalAccessorID)) {
          const Microsoft::glTF::Accessor& indicesAcc =
              document->accessors[primitive.indicesAccessorId];
          const Microsoft::glTF::Accessor& positionAcc =
              document->accessors[positionAccessorID];
          const Microsoft::glTF::Accessor& normalAcc =
              document->accessors[normalAccessorID];

          if (indicesAcc.componentType == Microsoft::glTF::COMPONENT_UNSIGNED_SHORT) {
            std::vector<unsigned short> indices =
                resourceReader->ReadBinaryData<unsigned short>(*document, indicesAcc);
            for (auto& index : indices) {
              currentMesh.indices.push_back(index);
            }
          } else if (indicesAcc.componentType ==
                     Microsoft::glTF::COMPONENT_UNSIGNED_INT) {
            std::vector<unsigned int> indices =
                resourceReader->ReadBinaryData<unsigned int>(*document, indicesAcc);
            for (auto& index : indices) {
              currentMesh.indices.push_back(index);
            }
          }

          if (positionAcc.componentType == Microsoft::glTF::COMPONENT_FLOAT &&
              normalAcc.componentType == Microsoft::glTF::COMPONENT_FLOAT) {
            std::vector<float> positions =
                resourceReader->ReadBinaryData<float>(*document, positionAcc);
            std::vector<float> normals =
                resourceReader->ReadBinaryData<float>(*document, normalAcc);

            auto vertexCount = positionAcc.count;

            std::vector<float> tangents(vertexCount * 4, 0.0);
            std::vector<float> uvs(vertexCount * 2, 0.0);
            std::vector<float> uvs2(vertexCount * 2, 0.0);
            if (hasUV) {
              const Microsoft::glTF::Accessor& uvAcc = document->accessors[uvAccessorID];
              uvs = resourceReader->ReadBinaryData<float>(*document, uvAcc);
            }

            if (hasUV2) {
              const Microsoft::glTF::Accessor& uvAcc = document->accessors[uvAccessorID2];
              uvs2 = resourceReader->ReadBinaryData<float>(*document, uvAcc);
            }

            if (hasTangent) {
              const Microsoft::glTF::Accessor& tangentAcc =
                  document->accessors[tangentAccessorID];
              tangents = resourceReader->ReadBinaryData<float>(*document, tangentAcc);
            }

            for (uint64_t i = 0; i < vertexCount; i++) {
              const std::array<uint64_t, 4> fourElementsProp = {4 * i, 4 * i + 1,
                                                                4 * i + 2, 4 * i + 3};
              const std::array<uint64_t, 3> threeElementsProp = {3 * i, 3 * i + 1,
                                                                 3 * i + 2};
              const std::array<uint64_t, 2> twoElementsProp = {2 * i, 2 * i + 1};

              Vertex vertex{
                  .pos = glm::vec3(positions[threeElementsProp[0]],
                                   positions[threeElementsProp[1]],
                                   positions[threeElementsProp[2]]),
                  .normal = glm::vec3(normals[threeElementsProp[0]],
                                      normals[threeElementsProp[1]],
                                      normals[threeElementsProp[2]]),
                  .tangent = glm::vec4(
                      tangents[fourElementsProp[0]], tangents[fourElementsProp[1]],
                      tangents[fourElementsProp[2]], tangents[fourElementsProp[3]]),
                  .texCoord = glm::vec2(uvs[twoElementsProp[0]], uvs[twoElementsProp[1]]),
                  .texCoord1 =
                      glm::vec2(uvs2[twoElementsProp[0]], uvs2[twoElementsProp[1]]),
                  .material = uint32_t(currentMesh.material),
              };

              vertex.applyTransform(m);

              currentMesh.vertices.emplace_back(vertex);
              currentMesh.vertices16bit.emplace_back(to16bitVertex(vertex));

              if (vertex.pos.x < currentMesh.minAABB.x) {
                currentMesh.minAABB.x = vertex.pos.x;
              }
              if (vertex.pos.y < currentMesh.minAABB.y) {
                currentMesh.minAABB.y = vertex.pos.y;
              }
              if (vertex.pos.z < currentMesh.minAABB.z) {
                currentMesh.minAABB.z = vertex.pos.z;
              }

              if (vertex.pos.x > currentMesh.maxAABB.x) {
                currentMesh.maxAABB.x = vertex.pos.x;
              }
              if (vertex.pos.y > currentMesh.maxAABB.y) {
                currentMesh.maxAABB.y = vertex.pos.y;
              }
              if (vertex.pos.z > currentMesh.maxAABB.z) {
                currentMesh.maxAABB.z = vertex.pos.z;
              }
            }
          }
        }
      }
      if (primitive.materialId != "") {
        currentMesh.material = document->materials.GetIndex(primitive.materialId);
      }
    }
    if (!currentMesh.indices.empty() && !currentMesh.vertices.empty()) {
      IndirectDrawDataAndMeshData indirectDrawData{
          .indexCount = uint32_t(currentMesh.indices.size()),
          .instanceCount = 1,
          .firstIndex = firstIndex,
          .vertexOffset = firstInstance,
          .firstInstance = 0,
          .meshId = uint32_t(outputModel.meshes.size()),
          .materialIndex = static_cast<int>(currentMesh.material),
      };

      firstIndex += currentMesh.indices.size();
      firstInstance += currentMesh.vertices.size();

      currentMesh.extents = (currentMesh.maxAABB - currentMesh.minAABB) * 0.5f;
      currentMesh.center = currentMesh.minAABB + currentMesh.extents;
      outputModel.meshes.emplace_back(std::move(currentMesh));
      outputModel.indirectDrawDataSet.emplace_back(std::move(indirectDrawData));
      outputModel.totalVertexSize +=
          sizeof(Vertex) * outputModel.meshes.back().vertices.size();
      outputModel.totalIndexSize +=
          sizeof(Mesh::Indices) * outputModel.meshes.back().indices.size();
    }
  }
}

void GLBLoader::updateMaterials(std::shared_ptr<Microsoft::glTF::Document> document,
                                Model& outputModel) {
  for (auto& mat : document->materials.Elements()) {
    Material currentMat;

    auto data = {
        std::pair{&mat.metallicRoughness.baseColorTexture.textureId,
                  &currentMat.basecolorTextureId},
        std::pair{&mat.metallicRoughness.metallicRoughnessTexture.textureId,
                  &currentMat.metallicRoughnessTextureId},
        std::pair{&mat.normalTexture.textureId, &currentMat.normalTextureTextureId},
        std::pair{&mat.emissiveTexture.textureId, &currentMat.emissiveTextureId}};

    for (auto& d : data) {
      if (*d.first != "") {
        *(d.second) = std::stoi(*d.first);
      }
    }

    currentMat.basecolorSamplerId = 0;
    currentMat.basecolor = glm::vec4(
        mat.metallicRoughness.baseColorFactor.r, mat.metallicRoughness.baseColorFactor.g,
        mat.metallicRoughness.baseColorFactor.b, mat.metallicRoughness.baseColorFactor.a);

    currentMat.metallicFactor = mat.metallicRoughness.metallicFactor;
    currentMat.roughnessFactor = mat.metallicRoughness.roughnessFactor;

    outputModel.materials.emplace_back(std::move(currentMat));
  }
}

void convertModel2OneMeshPerBuffer(
    const VulkanCore::Context& context, VulkanCore::CommandQueueManager& queueMgr,
    VkCommandBuffer commandBuffer, const Model& model,
    std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
    std::vector<std::shared_ptr<VulkanCore::Texture>>& textures,
    std::vector<std::shared_ptr<VulkanCore::Sampler>>& samplers,
    bool makeBuffersSuitableForAccelStruct) {
  convertModel2OneMeshPerBuffer(context, queueMgr, commandBuffer, model, buffers,
                                samplers, makeBuffersSuitableForAccelStruct);

  for (size_t textureIndex = 0; const auto& texture : model.textures) {
    textures.emplace_back(context.createTexture(
        VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VkExtent3D{
            .width = static_cast<uint32_t>(texture->width),
            .height = static_cast<uint32_t>(texture->height),
            .depth = 1u,
        },
        1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, VK_SAMPLE_COUNT_1_BIT,
        std::to_string(textureIndex)));

    // Upload texture to GPU
    auto textureUploadStagingBuffer = context.createStagingBuffer(
        textures.back()->vkDeviceSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        std::to_string(textureIndex));

    textures.back()->uploadAndGenMips(commandBuffer, textureUploadStagingBuffer.get(),
                                      texture->data);

    queueMgr.disposeWhenSubmitCompletes(std::move(textureUploadStagingBuffer));

    ++textureIndex;
  }
}

void convertModel2OneMeshPerBuffer(
    const VulkanCore::Context& context, VulkanCore::CommandQueueManager& queueMgr,
    VkCommandBuffer commandBuffer, const Model& model,
    std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
    std::vector<std::shared_ptr<VulkanCore::Sampler>>& samplers,
    bool makeBuffersSuitableForAccelStruct) {
  for (size_t meshIndex = 0; const auto& mesh : model.meshes) {
    const auto verticesSize = sizeof(Vertex) * mesh.vertices.size();



    buffers.emplace_back(context.createBuffer(
        verticesSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            (makeBuffersSuitableForAccelStruct ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0) |
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        "Mesh " + std::to_string(meshIndex) + " vertex buffer"));

    // Upload vertices
    context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers.back().get(),
                              reinterpret_cast<const void*>(mesh.vertices.data()),
                              verticesSize);

    const auto indicesSize = sizeof(uint32_t) * mesh.indices.size();
    buffers.emplace_back(context.createBuffer(
        indicesSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                (makeBuffersSuitableForAccelStruct ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0) |
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        "Mesh " + std::to_string(meshIndex) + " index buffer"));

    // Upload indices
    context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers.back().get(),
                              reinterpret_cast<const void*>(mesh.indices.data()),
                              indicesSize);

    ++meshIndex;
  }

  // Material buffer
  const auto totalMaterialSize = sizeof(Material) * model.materials.size();

  buffers.emplace_back(context.createBuffer(
      totalMaterialSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
              (makeBuffersSuitableForAccelStruct
          ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
          : 0)
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
          | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
#endif
      ,
      VMA_MEMORY_USAGE_GPU_ONLY, "materials"));

  context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers.back().get(),
                            reinterpret_cast<const void*>(model.materials.data()),
                            totalMaterialSize);
}

void convertModel2OneBuffer(const VulkanCore::Context& context,
                            VulkanCore::CommandQueueManager& queueMgr,
                            VkCommandBuffer commandBuffer, const Model& model,
                            std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
                            std::vector<std::shared_ptr<VulkanCore::Texture>>& textures,
                            std::vector<std::shared_ptr<VulkanCore::Sampler>>& samplers,
                            bool useHalfFloatVertices,
                            bool makeBuffersSuitableForAccelStruct) {
  convertModel2OneBuffer(context, queueMgr, commandBuffer, model, buffers, samplers,
                         useHalfFloatVertices, makeBuffersSuitableForAccelStruct);

  for (size_t textureIndex = 0; const auto& texture : model.textures) {
    textures.emplace_back(context.createTexture(
        VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VkExtent3D{
            .width = static_cast<uint32_t>(texture->width),
            .height = static_cast<uint32_t>(texture->height),
            .depth = 1u,
        },
        1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, VK_SAMPLE_COUNT_1_BIT,
        std::to_string(textureIndex)));

    // Upload texture to GPU
    auto textureUploadStagingBuffer = context.createStagingBuffer(
        textures.back()->vkDeviceSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        std::to_string(textureIndex));

    textures.back()->uploadAndGenMips(commandBuffer, textureUploadStagingBuffer.get(),
                                      texture->data);

    queueMgr.disposeWhenSubmitCompletes(std::move(textureUploadStagingBuffer));

    ++textureIndex;
  }
}

/// <summary>
/// Adds x buffers:
///   [0] Vertex buffer
///   [1] Index buffer
///   [2] Material buffer
///   [3] Indirect draw commands
/// </summary>
/// <param name="context"></param>
/// <param name="queueMgr"></param>
/// <param name="commandBuffer"></param>
/// <param name="model"></param>
/// <param name="buffers"></param>
/// <param name="samplers"></param>

void convertModel2OneBuffer(const VulkanCore::Context& context,
                            VulkanCore::CommandQueueManager& queueMgr,
                            VkCommandBuffer commandBuffer, const Model& model,
                            std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
                            std::vector<std::shared_ptr<VulkanCore::Sampler>>& samplers,
                            bool useHalfFloatVertices,
                            bool makeBuffersSuitableForAccelStruct) {
  // Vertex buffer
  buffers.emplace_back(context.createBuffer(
      model.totalVertexSize,
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | (makeBuffersSuitableForAccelStruct
          ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
          : 0) |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "vertex"));

  // Index buffer
  buffers.emplace_back(context.createBuffer(
      model.totalIndexSize,
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif

          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT | (makeBuffersSuitableForAccelStruct
          ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
          : 0) |
          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "index"));

  uint32_t currentVertexStartingIndex = 0u;
  uint32_t currentIndicesStartingIndex = 0u;
  uint32_t firstIndex = 0u;
  uint32_t firstInstance = 0u;
  std::vector<IndirectDrawCommandAndMeshData> indirectDrawData;
  indirectDrawData.reserve(model.meshes.size());
  for (size_t meshId = 0; const auto& mesh : model.meshes) {
    size_t vertexTotalSize = 0;
    if (useHalfFloatVertices) {
      vertexTotalSize = sizeof(Vertex16Bit) * mesh.vertices16bit.size();
      context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers[0].get(),
                                reinterpret_cast<const void*>(mesh.vertices16bit.data()),
                                vertexTotalSize, currentVertexStartingIndex);
    } else {
      vertexTotalSize = sizeof(Vertex) * mesh.vertices.size();
      context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers[0].get(),
                                reinterpret_cast<const void*>(mesh.vertices.data()),
                                vertexTotalSize, currentVertexStartingIndex);
    }
    currentVertexStartingIndex += vertexTotalSize;

    const auto indicesTotalSize = sizeof(Mesh::Indices) * mesh.indices.size();

    context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers[1].get(),
                              reinterpret_cast<const void*>(mesh.indices.data()),
                              indicesTotalSize, currentIndicesStartingIndex);

    currentIndicesStartingIndex += indicesTotalSize;

    indirectDrawData.emplace_back(IndirectDrawCommandAndMeshData{
        .command =
            {
                .indexCount = uint32_t(mesh.indices.size()),
                .instanceCount = 1,
                .firstIndex = firstIndex,
                .vertexOffset = static_cast<int>(firstInstance),  // 0,
                .firstInstance = 0,                               // firstInstance,
                                                                  //.vertexOffset = 0,
                                                                  //.firstInstance =
                                                                  // firstInstance,
            },
        .meshId = static_cast<uint32_t>(meshId),
        .materialIndex = static_cast<uint32_t>(mesh.material),
    });

    firstIndex += mesh.indices.size();
    firstInstance += mesh.vertices.size();

    ++meshId;
  }

  // Material buffer
  const auto totalMaterialSize = sizeof(Material) * model.materials.size();

  buffers.emplace_back(context.createBuffer(
      totalMaterialSize,
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif

          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
              (makeBuffersSuitableForAccelStruct
          ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
          : 0),
      VMA_MEMORY_USAGE_GPU_ONLY, "materials"));

  context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers[2].get(),
                            reinterpret_cast<const void*>(model.materials.data()),
                            totalMaterialSize);

  // Indirect draw parameters buffer
  const auto totalIndirectBufferSize =
      sizeof(IndirectDrawCommandAndMeshData) * indirectDrawData.size();

  buffers.emplace_back(context.createBuffer(
      totalIndirectBufferSize,
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
              (makeBuffersSuitableForAccelStruct
          ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
          : 0) |
          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "IndirectDraw"));

  context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers[3].get(),
                            reinterpret_cast<const void*>(indirectDrawData.data()),
                            totalIndirectBufferSize);
}

/// @brief Adds 3 buffers to 'buffers':
///        [0] Vertex buffer (optimized)
///        [1] Index buffer (optimized)
///        [2] Material buffer
void convertModel2OneBufferOptimized(
    const VulkanCore::Context& context, VulkanCore::CommandQueueManager& queueMgr,
    VkCommandBuffer commandBuffer, const Model& model,
    std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
    std::vector<std::shared_ptr<VulkanCore::Texture>>& textures,
    std::vector<std::shared_ptr<VulkanCore::Sampler>>& samplers,
    bool makeBuffersSuitableForAccelStruct) {
  std::vector<std::pair<size_t, size_t>> indexOffsets;
  indexOffsets.reserve(model.meshes.size());
  std::vector<EngineCore::Vertex> vertexData;
  for (auto& mesh : model.meshes) {
    for (auto index : mesh.indices) {
      vertexData.emplace_back(mesh.vertices[index]);
    }
    const size_t newOffset =
        indexOffsets.empty() ? 0 : indexOffsets.back().first + indexOffsets.back().second;
    indexOffsets.push_back(std::make_pair(newOffset, mesh.indices.size()));
  }

  const size_t index_count = vertexData.size();
  std::vector<unsigned int> remap(index_count);
  size_t vertex_count =
      meshopt_generateVertexRemap(remap.data(), nullptr, index_count, vertexData.data(),
                                  index_count, sizeof(EngineCore::Vertex));

  std::vector<EngineCore::Vertex> vertices;
  vertices.resize(vertex_count);
  std::vector<unsigned int> indices;
  indices.resize(index_count);
  meshopt_remapIndexBuffer(indices.data(), nullptr, index_count, &remap[0]);
  meshopt_remapVertexBuffer(vertices.data(), vertexData.data(), index_count,
                            sizeof(EngineCore::Vertex), &remap[0]);

  meshopt_optimizeVertexCache(indices.data(), indices.data(), index_count, vertex_count);

  meshopt_optimizeOverdraw(indices.data(), indices.data(), index_count,
                           &vertices[0].pos.x, vertex_count, sizeof(EngineCore::Vertex),
                           1.05f);

  meshopt_optimizeVertexFetch(vertices.data(), indices.data(), index_count,
                              vertices.data(), vertex_count, sizeof(EngineCore::Vertex));

  // gpuBuffers_.emplace_back(ModelGPUBuffer(context_,
  // bindlessMode_));
  //  gpuBuffers.back().uploadToGPU(model, queueMgr,
  //  commandBuffer);
  // gpuBuffers_.back().uploadToGPU(model, vertices,
  // indices, queueMgr, commandBuffer);
  // gpuBuffers_.back().indexOffsets_ =
  // indexOffsets;

  // Vertex buffer
  const auto verticesSize = sizeof(Vertex) * vertices.size();
  buffers.emplace_back(context.createBuffer(
      verticesSize,
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | (makeBuffersSuitableForAccelStruct
          ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
          : 0) |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "vertex"));

  // Upload vertices
  context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers.back().get(),
                            reinterpret_cast<const void*>(vertices.data()), verticesSize);

  // Index buffer
  const auto indicesSize = sizeof(uint32_t) * indices.size();
  buffers.emplace_back(context.createBuffer(
      indicesSize,
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | (makeBuffersSuitableForAccelStruct
          ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
          : 0) |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "index"));

  // Upload indices
  context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers.back().get(),
                            reinterpret_cast<const void*>(indices.data()), indicesSize);

  // Material buffer
  const auto totalMaterialSize = sizeof(Material) * model.materials.size();
  buffers.emplace_back(context.createBuffer(
      totalMaterialSize,
#if defined(VK_KHR_buffer_device_address) && defined(_WIN32)
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
#endif
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | (makeBuffersSuitableForAccelStruct
          ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
          : 0) |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "materials"));

  context.uploadToGPUBuffer(queueMgr, commandBuffer, buffers.back().get(),
                            reinterpret_cast<const void*>(model.materials.data()),
                            totalMaterialSize);

  for (size_t textureIndex = 0; const auto& texture : model.textures) {
    textures.emplace_back(context.createTexture(
        VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VkExtent3D{
            .width = static_cast<uint32_t>(texture->width),
            .height = static_cast<uint32_t>(texture->height),
            .depth = 1u,
        },
        1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, VK_SAMPLE_COUNT_1_BIT,
        std::to_string(textureIndex)));

    // Upload texture to GPU
    auto textureUploadStagingBuffer = context.createStagingBuffer(
        textures.back()->vkDeviceSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        std::to_string(textureIndex));

    textures.back()->uploadAndGenMips(commandBuffer, textureUploadStagingBuffer.get(),
                                      texture->data);

    queueMgr.disposeWhenSubmitCompletes(std::move(textureUploadStagingBuffer));

    ++textureIndex;
  }
}

}  // namespace EngineCore
