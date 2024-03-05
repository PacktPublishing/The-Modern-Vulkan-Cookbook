/************************************************************************************************
Filename    :   VirtualKeyboardModelRenderer.cpp
Content     :   Helper class for rendering the virtual keyboard model
Created     :   January 2023
Authors     :   Peter Chan
Language    :   C++
Copyright   :   Copyright (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
************************************************************************************************/

#include "VirtualKeyboardModelRenderer.h"

#include <Misc/Log.h>
#include <Model/ModelAnimationUtils.h>
#include <Model/ModelFileLoading.h>
#include <Model/ModelDef.h>
#include <OVR_Math.h>

namespace {

/// clang-format off
const char* kVertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec2 TexCoord;

varying lowp vec2 oTexCoord;

void main()
{
  gl_Position = TransformVertex( Position );
  oTexCoord = TexCoord;
}
)glsl";

const char* kFragmentShaderSrc = R"glsl(
precision lowp float;

uniform sampler2D Texture0;
uniform lowp vec4 BaseColorFactor;

varying lowp vec2 oTexCoord;

void main()
{
  lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
  lowp vec3 finalColor = diffuse.xyz * BaseColorFactor.xyz;

  // apply alpha
  gl_FragColor.w = diffuse.w;
  // premult + gamma correction
  gl_FragColor.xyz = pow(finalColor.rgb, vec3(2.2)) * gl_FragColor.w;
}
)glsl";
/// clang-format on

bool ParseImageUri(
    const std::string& uri,
    uint64_t& textureId,
    uint32_t& pixelWidth,
    uint32_t& pixelHeight) {
    // URI format:
    // metaVirtualKeyboard://texture/{textureID}?w={width}&h={height}&fmt=RGBA32

    auto getToken = [&uri](size_t startIdx, char delimiter, std::string& token) {
        const size_t endIdx = uri.find_first_of(delimiter, startIdx);
        if (endIdx == std::string::npos) {
            return false;
        }
        token = uri.substr(startIdx, endIdx - startIdx);
        return true;
    };

    // Validate scheme
    std::string token;
    size_t index = 0;
    if (!getToken(index, ':', token) || token != "metaVirtualKeyboard") {
        return false;
    }

    // Validate resource type
    index += token.size() + 3; // skip "://"
    if (!getToken(index, '/', token) || token != "texture") {
        return false;
    }

    // Get texture id
    index += token.size() + 1; // skip "/"
    if (!getToken(index, '?', token)) {
        return false;
    }
    textureId = std::stoull(token);

    // Get pixel width
    index += token.size() + 3; // skip "?w="
    if (!getToken(index, '&', token)) {
        return false;
    }
    pixelWidth = std::stoul(token);

    // Get pixel height
    index += token.size() + 3; // skip "&h="
    if (!getToken(index, '&', token)) {
        return false;
    }
    pixelHeight = std::stoul(token);

    // Validate format
    index += token.size();
    if (uri.substr(index) != "&fmt=RGBA32") {
        return false;
    }

    return true;
}

OVRFW::GlTexture CreateGlTexture(uint32_t pixelWidth, uint32_t pixelHeight) {
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, pixelWidth, pixelHeight);
    std::vector<uint8_t> blankBytes((size_t)pixelWidth * pixelHeight * 4);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        pixelWidth,
        pixelHeight,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        blankBytes.data());
    return OVRFW::GlTexture(texId, GL_TEXTURE_2D, pixelWidth, pixelHeight);
}

void UpdateGlTexture(OVRFW::GlTexture texture, const uint8_t* textureData) {
    glBindTexture(GL_TEXTURE_2D, texture.texture);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        texture.Width,
        texture.Height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        textureData);
}

const OVRFW::ModelNode* FindCollisionNode(const OVRFW::ModelFile& model) {
    auto iter =
        std::find_if(model.Nodes.begin(), model.Nodes.end(), [](const auto& node) {
        return node.name == "collision";
    });
    if (iter == model.Nodes.end()) {
        return nullptr;
    }
    const OVRFW::ModelNode* collisionNode = &(*iter);
    return collisionNode;
}

} // namespace

bool VirtualKeyboardModelRenderer::Init(const std::vector<uint8_t>& modelBuffer) {
    if (modelBuffer.empty()) {
        return false;
    }

    OVRFW::ovrProgramParm uniformParms[] = {
        {"Texture0", OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        {"BaseColorFactor", OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
    };
    progKeyboard_ = OVRFW::GlProgram::Build(
        "",
        kVertexShaderSrc,
        "",
        kFragmentShaderSrc,
        uniformParms,
        sizeof(uniformParms) / sizeof(OVRFW::ovrProgramParm));

    OVRFW::MaterialParms materials = {};
    materials.ImageUriHandler = [this](OVRFW::ModelFile& modelFile, const std::string& uri) {
        uint64_t textureId;
        uint32_t pixelWidth;
        uint32_t pixelHeight;
        if (!ParseImageUri(uri, textureId, pixelWidth, pixelHeight)) {
            return false;
        }

        // Add texture to our model
        OVRFW::ModelTexture tex;
        tex.name = std::to_string(textureId);
        tex.texid = CreateGlTexture(pixelWidth, pixelHeight);
        modelFile.Textures.push_back(tex);

        // Register texture
        textureIdMap_[textureId] = tex.texid;
        ALOG("Registered texture %d, %ux%u", (int)textureId, pixelWidth, pixelHeight);
        return true;
    };
    OVRFW::ModelGlPrograms programs = {};
    programs.ProgSingleTexture = &progKeyboard_;
    programs.ProgBaseColorPBR = &progKeyboard_;
    programs.ProgSkinnedBaseColorPBR = &progKeyboard_;
    programs.ProgLightMapped = &progKeyboard_;
    programs.ProgBaseColorEmissivePBR = &progKeyboard_;
    programs.ProgSkinnedBaseColorEmissivePBR = &progKeyboard_;
    programs.ProgSimplePBR = &progKeyboard_;
    programs.ProgSkinnedSimplePBR = &progKeyboard_;

    keyboardModel_ = std::unique_ptr<OVRFW::ModelFile>(LoadModelFile_glB(
        "keyboard", (const char*)modelBuffer.data(), modelBuffer.size(), programs, materials));
    if (keyboardModel_ == nullptr) {
        return false;
    }

    collisionNode_ = FindCollisionNode(*keyboardModel_);

    keyboardModelState_ = std::make_unique<OVRFW::ModelState>();
    keyboardModelState_->GenerateStateFromModelFile(keyboardModel_.get());

    for (const auto& nodeState : keyboardModelState_->nodeStates) {
        OVRFW::Model* model = nodeState.node->model;
        if (model != nullptr) {
            auto& gc = model->surfaces[0].surfaceDef.graphicsCommand;
            gc.UniformData[0].Data = &gc.Textures[0];
            gc.UniformData[1].Data = (void*)&model->surfaces[0].material->baseColorFactor;
            gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
            gc.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_ENABLE;
            gc.GpuState.blendMode = GL_FUNC_ADD;
            gc.GpuState.blendSrc = GL_ONE;
            gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
        }
    }

    return true;
}

void VirtualKeyboardModelRenderer::Shutdown() {
    OVRFW::GlProgram::Free(progKeyboard_);
    keyboardModelState_.reset();
    keyboardModel_.reset();
    textureIdMap_.clear();
    collisionNode_ = nullptr;
}

void VirtualKeyboardModelRenderer::Update(const OVR::Posef& pose, const OVR::Vector3f& scale) {
    transform_ = OVR::Matrix4f(pose) * OVR::Matrix4f::Scaling(scale);
}

void VirtualKeyboardModelRenderer::Render(std::vector<OVRFW::ovrDrawSurface>& surfaceList) {
    if (keyboardModel_ != nullptr) {
        for (const auto& nodeState : keyboardModelState_->nodeStates) {
            const OVRFW::Model* model = nodeState.node->model;
            if (model != nullptr) {
                OVRFW::ovrDrawSurface surface;
                surface.surface = &(model->surfaces[0].surfaceDef);
                surface.modelMatrix = transform_ * nodeState.GetGlobalTransform();
                surfaceList.push_back(surface);
            }
        }
    }
}

void VirtualKeyboardModelRenderer::UpdateTexture(
    uint64_t textureId,
    const uint8_t* textureData,
    uint32_t textureWidth,
    uint32_t textureHeight) {
    auto iter = textureIdMap_.find(textureId);
    if (iter == textureIdMap_.end()) {
        ALOGE("Failed to update render model texture. Texture id %d not found.", (int)textureId);
        return;
    }
    auto& modelTexture = iter->second;
    if (modelTexture.Width != static_cast<int>(textureWidth) ||
        modelTexture.Height != static_cast<int>(textureHeight)) {
        ALOGE("Invalid render model texture dimensions for id %d.", (int)textureId);
        return;
    }

    UpdateGlTexture(modelTexture, textureData);
}

void VirtualKeyboardModelRenderer::SetAnimationState(int animationIndex, float fraction) {
    if (animationIndex < 0 ||
        animationIndex >= static_cast<int>(keyboardModel_->Animations.size())) {
        ALOGE(
            "Invalid animation index %i, count = %zu",
            animationIndex,
            keyboardModel_->Animations.size());
        return;
    }

    const float timeInSeconds =
        (keyboardModel_->animationEndTime - keyboardModel_->animationStartTime) *
        std::clamp(fraction, 0.0f, 1.0f);

    const OVRFW::ModelAnimation& animation = keyboardModel_->Animations[animationIndex];
    for (const OVRFW::ModelAnimationChannel& channel : animation.channels) {
        OVRFW::ModelAnimationTimeLineState& timeLineState =
            keyboardModelState_->animationTimelineStates[channel.sampler->timeLineIndex];
        timeLineState.CalculateFrameAndFraction(timeInSeconds);
    }

    ApplyAnimation(*keyboardModelState_, animationIndex);

    for (const OVRFW::ModelAnimationChannel& channel : animation.channels) {
        OVRFW::ModelNodeState& nodeState = keyboardModelState_->nodeStates[channel.nodeIndex];
        nodeState.RecalculateMatrix();

        // If animation controls weights, cache the node index so we can update the surface geo once
        // all the weights are applied
        if (channel.path == OVRFW::MODEL_ANIMATION_PATH_WEIGHTS) {
            dirtyGeoNodeIndices_.push_back(channel.nodeIndex);
        }
    }
}

void VirtualKeyboardModelRenderer::UpdateSurfaceGeo() {
    for (auto nodeIndex : dirtyGeoNodeIndices_) {
        OVRFW::ModelNodeState& nodeState = keyboardModelState_->nodeStates[nodeIndex];
        std::vector<OVRFW::ModelSurface>& surfaces = nodeState.node->model->surfaces;
        for (auto& surface : surfaces) {
            OVRFW::VertexAttribs attribs = surface.attribs; // copy base values
            for (int w = 0; w < static_cast<int>(nodeState.weights.size()); ++w) {
                const auto& targetAttribs = surface.targets[w];
                if (!targetAttribs.position.empty()) {
                    float* original = (float*)attribs.position.data();
                    float* target = (float*)targetAttribs.position.data();
                    int posIndex = (w % 2) + (w / 2 * 3); // skip z
                    original[posIndex] += target[posIndex] * nodeState.weights[w];
                }
                if (!targetAttribs.uv0.empty()) {
                    float* original = (float*)attribs.uv0.data();
                    float* target = (float*)targetAttribs.uv0.data();
                    int uvIndex = w - 8; // 4 pairs of (x,y) for positions
                    original[uvIndex] += target[uvIndex] * nodeState.weights[w];
                }
            }
            surface.surfaceDef.geo.Update(attribs, false);
        }
    }
    dirtyGeoNodeIndices_.clear();
}

bool VirtualKeyboardModelRenderer::IsModelLoaded() const {
    return keyboardModel_ != nullptr;
}

bool VirtualKeyboardModelRenderer::IsPointNearKeyboard(const OVR::Vector3f& globalPoint) const {
    if (!IsModelLoaded() || collisionNode_ == nullptr) {
        return false;
    }
    auto localPoint =
        (transform_ * collisionNode_->GetGlobalTransform()).Inverted().Transform(globalPoint);
    auto bounds = GetCollisionBounds();
    // Expand in front and behind the keyboard
    bounds.AddPoint(bounds.GetCenter() + OVR::Vector3f(0.0f, 0.0f, -0.25f));
    bounds.AddPoint(bounds.GetCenter() + OVR::Vector3f(0.0f, 0.0f, 0.4f));
    return bounds.Contains(localPoint);
}

OVR::Bounds3f VirtualKeyboardModelRenderer::GetCollisionBounds() const {
    OVR::Bounds3f bounds;
    if (collisionNode_ != nullptr) {
        bounds = OVR::Bounds3f(collisionNode_->model->surfaces[0].surfaceDef.geo.localBounds);
    }
    return bounds;
}
