/************************************************************************************************
Filename    :   ModelRenderer.h
Content     :   A one stop for models from the render model extension
Created     :   April 2021
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.
************************************************************************************************/
#include "ModelRenderer.h"
#include "Model/ModelFile.h"
#include "Model/ModelFileLoading.h"
#include "XrApp.h"
#include <string.h>

using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {
namespace ModelRender {

/// clang-format off
static const char* VertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec3 Normal;
attribute highp vec2 TexCoord;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;

vec3 multiply( mat4 m, vec3 v )
{
  return vec3(
  m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
  m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
  m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
}

vec3 transposeMultiply( mat4 m, vec3 v )
{
  return vec3(
  m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
  m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
  m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
}

void main()
{
  gl_Position = TransformVertex( Position );
  vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
  oEye = eye - vec3( ModelMatrix * Position );
  vec3 iNormal = Normal * 100.0f;
  oNormal = multiply( ModelMatrix, iNormal );
  oTexCoord = TexCoord;
}
)glsl";

static const char* FragmentShaderSrc = R"glsl(
precision lowp float;

uniform sampler2D Texture0;
uniform lowp vec3 SpecularLightDirection;
uniform lowp vec3 SpecularLightColor;
uniform lowp vec3 AmbientLightColor;
uniform float Opacity;
uniform float AlphaBlend;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;

lowp vec3 multiply( lowp mat3 m, lowp vec3 v )
{
  return vec3(
  m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
  m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
  m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
}

void main()
{
  lowp vec3 eyeDir = normalize( oEye.xyz );
  lowp vec3 Normal = normalize( oNormal );

  lowp vec3 reflectionDir = dot( eyeDir, Normal ) * 2.0 * Normal - eyeDir;
  lowp vec4 diffuse = texture2D( Texture0, oTexCoord );
  lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

  lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
  lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

  lowp float specularPower = 1.0f - diffuse.a;
  specularPower = specularPower * specularPower;

  lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
  lowp float nDotH = max( dot( Normal, H ), 0.0 );
  lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
  lowp vec3 specularValue = specularIntensity * SpecularLightColor;

  lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;

  float alphaBlendFactor = max(diffuse.w, AlphaBlend) * Opacity;

  // apply alpha
  gl_FragColor.w = alphaBlendFactor;
  // premult
  gl_FragColor.xyz = controllerColor * gl_FragColor.w;
}
)glsl";

/// clang-format on

} // namespace ModelRender

bool ModelRenderer::Init(std::vector<uint8_t>& modelBuffer) {
    /// Shader
    ovrProgramParm UniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"Opacity", ovrProgramParmType::FLOAT},
        {"AlphaBlend", ovrProgramParmType::FLOAT},
    };
    ProgRenderModel = GlProgram::Build(
        "",
        ModelRender::VertexShaderSrc,
        "",
        ModelRender::FragmentShaderSrc,
        UniformParms,
        sizeof(UniformParms) / sizeof(ovrProgramParm));

    MaterialParms materials = {};
    ModelGlPrograms programs = {};
    programs.ProgSingleTexture = &ProgRenderModel;
    programs.ProgBaseColorPBR = &ProgRenderModel;
    programs.ProgSkinnedBaseColorPBR = &ProgRenderModel;
    programs.ProgLightMapped = &ProgRenderModel;
    programs.ProgBaseColorEmissivePBR = &ProgRenderModel;
    programs.ProgSkinnedBaseColorEmissivePBR = &ProgRenderModel;
    programs.ProgSimplePBR = &ProgRenderModel;
    programs.ProgSkinnedSimplePBR = &ProgRenderModel;

    RenderModel = LoadModelFile_glB(
        "modelrenderer", (const char*)modelBuffer.data(), modelBuffer.size(), programs, materials);

    if (RenderModel == nullptr || static_cast<int>(RenderModel->Models.size()) < 1) {
        ALOGE("Couldn't load modelrenderer model!");
        return false;
    }

    for (auto& model : RenderModel->Models) {
        auto& gc = model.surfaces[0].surfaceDef.graphicsCommand;
        gc.UniformData[0].Data = &gc.Textures[0];
        gc.UniformData[1].Data = &SpecularLightDirection;
        gc.UniformData[2].Data = &SpecularLightColor;
        gc.UniformData[3].Data = &AmbientLightColor;
        gc.UniformData[4].Data = &Opacity;
        gc.UniformData[5].Data = &AlphaBlendFactor;
        gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
        gc.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
        gc.GpuState.blendMode = GL_FUNC_ADD;
        gc.GpuState.blendSrc = GL_ONE;
        gc.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
    }

    for (auto& node : RenderModel->Nodes) {
        if (node.name.find("grip") != std::string::npos) {
            GripPose = OVR::Posef(node.rotation, node.translation);
        }
    }

    /// Set defaults
    SpecularLightDirection = Vector3f(1.0f, 1.0f, 0.0f);
    SpecularLightColor = Vector3f(1.0f, 0.95f, 0.8f) * 0.75f;
    AmbientLightColor = Vector3f(1.0f, 1.0f, 1.0f) * 0.15f;

    /// all good
    Initialized = true;
    return true;
}

void ModelRenderer::Shutdown() {
    OVRFW::GlProgram::Free(ProgRenderModel);
    if (RenderModel != nullptr) {
        delete RenderModel;
        RenderModel = nullptr;
    }
}

void ModelRenderer::Update(
    const OVR::Posef& pose,
    const UpdateOffset updateOffset) {
    /// Compute transform for the root

    OVR::Posef offsetPose = pose;

    if (updateOffset == UpdateOffset_Grip) {
        offsetPose = offsetPose * GripPose;
    }

    Transform = Matrix4f(offsetPose);
}

void ModelRenderer::Render(std::vector<ovrDrawSurface>& surfaceList) {
    /// toggle alpha override
    AlphaBlendFactor = UseSolidTexture ? 1.0f : 0.0f;
    if (RenderModel != nullptr) {
        for (auto& model : RenderModel->Models) {
            ovrDrawSurface controllerSurface;
            controllerSurface.surface = &(model.surfaces[0].surfaceDef);
            controllerSurface.modelMatrix = Transform;
            surfaceList.push_back(controllerSurface);
        }
    }
}

} // namespace OVRFW
