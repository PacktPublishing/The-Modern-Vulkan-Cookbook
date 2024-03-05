#include "ShaderModule.hpp"

#ifdef _WIN32
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv_reflect.h>  // comes from spirv-reflect
#endif

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Context.hpp"

#ifdef _WIN32
namespace {
static constexpr uint32_t MAX_DESC_BINDLESS = 1000;
class CustomIncluder final : public glslang::TShader::Includer {
 public:
  explicit CustomIncluder(const std::string& shaderDir) : shaderDirectory(shaderDir) {}
  ~CustomIncluder() = default;

  IncludeResult* includeSystem(const char* headerName, const char* includerName,
                               size_t inclusionDepth) override {
    // You can implement system include paths here if needed.
    return nullptr;
  }

  IncludeResult* includeLocal(const char* headerName, const char* includerName,
                              size_t inclusionDepth) override {
    std::string fullPath = shaderDirectory + "/" + headerName;
    std::ifstream fileStream(fullPath, std::ios::in);
    if (!fileStream.is_open()) {
      std::string errMsg = "Failed to open included file: ";
      errMsg.append(headerName);
      return nullptr;
    }

    std::stringstream fileContent;
    fileContent << fileStream.rdbuf();
    fileStream.close();

    // The Includer owns the content memory and will delete it when it is no
    // longer needed.
    char* content = new char[fileContent.str().length() + 1];
    strncpy(content, fileContent.str().c_str(), fileContent.str().length());
    content[fileContent.str().length()] = '\0';

    return new IncludeResult(headerName, content, fileContent.str().length(), nullptr);
  }

  void releaseInclude(IncludeResult* result) override {
    if (result) {
      delete result;
    }
  }

 private:
  std::string shaderDirectory;
};
}  // namespace
#endif

namespace VulkanCore {

ShaderModule::ShaderModule(const Context* context, const std::string& filePath,
                           const std::string& entryPoint, VkShaderStageFlagBits stages,
                           const std::string& name)
    : context_(context), entryPoint_(entryPoint), vkStageFlags_(stages) {
  createShader(filePath, entryPoint, name);
}

ShaderModule::ShaderModule(const Context* context, const std::vector<char>& data,
                           const std::string& entryPoint, VkShaderStageFlagBits stages,
                           const std::string& name)
    : context_(context), entryPoint_(entryPoint), vkStageFlags_(stages) {
  createShader(data, entryPoint, name);
}

ShaderModule::ShaderModule(const Context* context, const std::string& filePath,
                           VkShaderStageFlagBits stages, const std::string& name)
    : ShaderModule(context, filePath, "main", stages, name) {}

ShaderModule::~ShaderModule() {
  vkDestroyShaderModule(context_->device(), vkShaderModule_, nullptr);
}

VkShaderModule ShaderModule::vkShaderModule() const { return vkShaderModule_; }

VkShaderStageFlagBits ShaderModule::vkShaderStageFlags() const { return vkStageFlags_; }

const std::string& ShaderModule::entryPoint() const { return entryPoint_; }

static constexpr uint32_t MAX_RESOURCES_COUNT = 1000;

#ifdef _WIN32
EShLanguage ShaderModule::shaderStageFromFileName(const char* fileName) {
  if (util::endsWith(fileName, ".vert")) {
    return EShLangVertex;
  } else if (util::endsWith(fileName, ".frag")) {
    return EShLangFragment;
  } else if (util::endsWith(fileName, ".comp")) {
    return EShLangCompute;
  } else if (util::endsWith(fileName, ".rgen")) {
    return EShLangRayGen;
  } else if (util::endsWith(fileName, ".rmiss")) {
    return EShLangMiss;
  } else if (util::endsWith(fileName, ".rchit")) {
    return EShLangClosestHit;
  } else if (util::endsWith(fileName, ".rahit")) {
    return EShLangAnyHit;
  } else {
    ASSERT(false, "Add if/else for GLSL stage");
  }

  return EShLangVertex;
}
#endif

void ShaderModule::printShader(const std::vector<char>& data) {
  uint32_t totalLines = std::count(data.begin(), data.end(), '\n');
  // We support up to 9,999 lines
  const uint32_t maxNumSpaces = static_cast<uint32_t>(std::log10(totalLines)) + 1;

  uint32_t lineNum = 1;
  std::cout << lineNum;
  const auto numSpaces = maxNumSpaces - static_cast<uint32_t>(std::log10(totalLines) - 1);
  for (int i = 0; i < numSpaces; ++i) {
    std::cout << ' ';
  }
  for (char c : data) {
    std::cout << c;
    if (c == '\n') {
      ++lineNum;
      std::cout << lineNum;

      const auto numSpaces =
          maxNumSpaces - static_cast<uint32_t>(std::log10(totalLines) - 1);
      for (int i = 0; i < numSpaces; ++i) {
        std::cout << ' ';
      }
    }
  }
}

std::string removeUnnecessaryLines(const std::string& str) {
  std::istringstream iss(str);
  std::ostringstream oss;
  std::string line;

  while (std::getline(iss, line)) {
    if (line != "#extension GL_GOOGLE_include_directive : require" &&
        line.substr(0, 5) != "#line") {
      oss << line << '\n';
    }
  }
  return oss.str();
}

#ifdef _WIN32
std::vector<char> ShaderModule::glslToSpirv(const std::vector<char>& data,
                                            EShLanguage shaderStage,
                                            const std::string& shaderDir,
                                            const char* entryPoint) {
  static bool glslangInitialized = false;

  if (!glslangInitialized) {
    glslang::InitializeProcess();
    glslangInitialized = true;
  }

  glslang::TShader tshadertemp(shaderStage);
  const char* glslCStr = data.data();
  tshadertemp.setStrings(&glslCStr, 1);

  glslang::EshTargetClientVersion clientVersion = glslang::EShTargetVulkan_1_3;
  glslang::EShTargetLanguageVersion langVersion = glslang::EShTargetSpv_1_0;

  if (shaderStage == EShLangRayGen || shaderStage == EShLangAnyHit ||
      shaderStage == EShLangClosestHit || shaderStage == EShLangMiss) {
    langVersion = glslang::EShTargetSpv_1_4;
  }

  tshadertemp.setEnvInput(glslang::EShSourceGlsl, shaderStage, glslang::EShClientVulkan,
                          460);

  tshadertemp.setEnvClient(glslang::EShClientVulkan, clientVersion);
  tshadertemp.setEnvTarget(glslang::EShTargetSpv, langVersion);

  tshadertemp.setEntryPoint(entryPoint);
  tshadertemp.setSourceEntryPoint(entryPoint);

  glslang::TShader tshader(shaderStage);

  tshader.setEnvInput(glslang::EShSourceGlsl, shaderStage, glslang::EShClientVulkan, 460);

  tshader.setEnvClient(glslang::EShClientVulkan, clientVersion);
  tshader.setEnvTarget(glslang::EShTargetSpv, langVersion);

  tshader.setEntryPoint(entryPoint);
  tshader.setSourceEntryPoint(entryPoint);

  const TBuiltInResource* resources = GetDefaultResources();
  const EShMessages messages = static_cast<EShMessages>(
      EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules | EShMsgDebugInfo);
  CustomIncluder includer(shaderDir);

  std::string preprocessedGLSL;
  if (!tshadertemp.preprocess(resources, 460, ENoProfile, false, false, messages,
                              &preprocessedGLSL, includer)) {
    std::cout << "Preprocessing failed for shader: " << std::endl;
    printShader(data);
    std::cout << std::endl;
    std::cout << tshadertemp.getInfoLog() << std::endl;
    std::cout << tshadertemp.getInfoDebugLog() << std::endl;
    ASSERT(false, "Error occured");
    return std::vector<char>();
  }

  // required since without this renderdoc have difficulty debugging/stepping
  // through shader correctly
  preprocessedGLSL = removeUnnecessaryLines(preprocessedGLSL);

  const char* preprocessedGLSLStr = preprocessedGLSL.c_str();
  tshader.setStrings(&preprocessedGLSLStr, 1);

  if (!tshader.parse(resources, 460, false, messages)) {
    std::cout << "Parsing failed for shader: " << std::endl;
    printShader(data);
    std::cout << std::endl;
    std::cout << tshader.getInfoLog() << std::endl;
    std::cout << tshader.getInfoDebugLog() << std::endl;
    ASSERT(false, "parse failed");
    return std::vector<char>();
  }

  glslang::SpvOptions options;

#ifdef _DEBUG
  tshader.setDebugInfo(true);
  options.generateDebugInfo = true;
  options.disableOptimizer = true;
  options.optimizeSize = false;
  options.stripDebugInfo = false;
#else
  options.disableOptimizer = true;  // this ensure that variables that aren't
  // used in shaders are not removed, without this flag, SPIRV
  // generated will be optimized & unused variables will be removed,
  // this will cause issues in debug vs release if struct on cpu vs
  // gpu are different
  options.optimizeSize = true;
  options.stripDebugInfo = true;
#endif

  glslang::TProgram program;
  program.addShader(&tshader);
  if (!program.link(messages)) {
    std::cout << "Parsing failed for shader " << std::endl;
    std::cout << program.getInfoLog() << std::endl;
    std::cout << program.getInfoDebugLog() << std::endl;
    ASSERT(false, "link failed");
  }

  std::vector<uint32_t> spirvData;
  spv::SpvBuildLogger spvLogger;
  glslang::GlslangToSpv(*program.getIntermediate(shaderStage), spirvData, &spvLogger,
                        &options);

  std::vector<char> byteCode;
  byteCode.resize(spirvData.size() * (sizeof(uint32_t) / sizeof(char)));
  std::memcpy(byteCode.data(), spirvData.data(), byteCode.size());

  return byteCode;
}
#endif

void ShaderModule::createShader(const std::string& filePath,
                                const std::string& entryPoint, const std::string& name) {
  std::vector<char> spirv;
  const bool isBinary = util::endsWith(filePath.c_str(), ".spv");
  std::vector<char> fileData = util::readFile(filePath, isBinary);
  std::filesystem::path file(filePath);
  if (isBinary) {
    spirv = std::move(fileData);
  }
#ifdef _WIN32
  else {
    spirv = glslToSpirv(fileData, shaderStageFromFileName(filePath.c_str()),
                        file.parent_path().string(), entryPoint.c_str());
  }
#endif

  createShader(spirv, entryPoint, name);
}

void ShaderModule::createShader(const std::vector<char>& spirv,
                                const std::string& entryPoint, const std::string& name) {
  const VkShaderModuleCreateInfo shaderModuleInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = spirv.size(),
      .pCode = (const uint32_t*)spirv.data(),
  };
  VK_CHECK(vkCreateShaderModule(context_->device(), &shaderModuleInfo, nullptr,
                                &vkShaderModule_));
  context_->setVkObjectname(vkShaderModule_, VK_OBJECT_TYPE_SHADER_MODULE,
                            "Shader Module: " + name);
}

}  // namespace VulkanCore
