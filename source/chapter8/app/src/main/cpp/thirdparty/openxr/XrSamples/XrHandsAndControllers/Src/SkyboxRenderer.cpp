/************************************************************************************************
Filename    :   SkyboxRenderer.cpp
Content     :   A renderer suited for gradient skyboxes.
Created     :   July 2023
Authors     :   Alex Borsboom
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.
************************************************************************************************/
#include "SkyboxRenderer.h"
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
namespace SkyboxShaders {

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
  oTexCoord = TexCoord;
}
)glsl";

static const char* FragmentShaderSrc = R"glsl(
precision lowp float;

uniform lowp vec3 TopColor;
uniform lowp vec3 MiddleColor;
uniform lowp vec3 BottomColor;

varying lowp vec2 oTexCoord;

lowp vec3 multiply( lowp mat3 m, lowp vec3 v )
{
  return vec3(
  m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
  m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
  m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
}

lowp float saturate(lowp float v) {
    return clamp(v, 0.0f, 1.0f);
}

void main()
{
  lowp float val = oTexCoord.y;
  lowp float topVal = saturate(-3.0 + ( 4.0 * val ));
  lowp float middleVal = saturate( 1.0 - 4.0 * abs(0.75 - val ));
  lowp float bottomVal = saturate( 4.0 * ( 0.75 - val));

  lowp vec3 finalColor = BottomColor.rgb * bottomVal + MiddleColor.rgb * middleVal + TopColor.rgb * topVal;

  gl_FragColor.w = 1.0f;
  gl_FragColor.xyz = finalColor;
}
)glsl";

/// clang-format on

} // namespace SkyboxShaders

bool SkyboxRenderer::Init(std::string modelPath, OVRFW::ovrFileSys* fileSys) {
    /// Shader
    ovrProgramParm UniformParms[] = {
        {"TopColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"MiddleColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"BottomColor", ovrProgramParmType::FLOAT_VECTOR3},
    };
    ProgRenderModel = GlProgram::Build(
        "",
        SkyboxShaders::VertexShaderSrc,
        "",
        SkyboxShaders::FragmentShaderSrc,
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

    if( fileSys ) {
        OVRFW::ovrFileSys& fs = *fileSys;
        RenderModel = LoadModelFile(fs, modelPath.c_str(), programs, materials);
    } else {
        ALOGE("Couldn't load model, we didn't get a valid filesystem");
        return false;
    }

    if (RenderModel == nullptr || static_cast<int>(RenderModel->Models.size()) < 1) {
        ALOGE("Couldn't load modelrenderer model!");
        return false;
    }

    TopColor = OVR::Vector3f(0.937f, 0.9236477f, 0.883591f);
    MiddleColor = OVR::Vector3f(0.6705883f, 0.6909091f, 0.7450981f);
    BottomColor = OVR::Vector3f(0.3372549f, 0.345098f, 0.3686275f);

    for (auto& model : RenderModel->Models) {
        auto& gc = model.surfaces[0].surfaceDef.graphicsCommand;
        gc.UniformData[0].Data = &TopColor;
        gc.UniformData[1].Data = &MiddleColor;
        gc.UniformData[2].Data = &BottomColor;
        gc.GpuState.depthMaskEnable = false;
        gc.GpuState.depthEnable = false;
        gc.GpuState.blendEnable = ovrGpuState::BLEND_DISABLE;
    }

    /// all good
    Initialized = true;
    return true;
}

void SkyboxRenderer::Shutdown() {
    OVRFW::GlProgram::Free(ProgRenderModel);
    if (RenderModel != nullptr) {
        delete RenderModel;
        RenderModel = nullptr;
    }
}

void SkyboxRenderer::Render(std::vector<ovrDrawSurface>& surfaceList) {
    if (RenderModel != nullptr) {
        for( int i=0; i < static_cast<int>(RenderModel->Models.size()); i++ ) {
            auto& model = RenderModel->Models[i];
            auto& node = RenderModel->Nodes[i];
            ovrDrawSurface controllerSurface;
            for( int j=0; j < static_cast<int>(model.surfaces.size()); j++ ) {
                controllerSurface.surface = &(model.surfaces[j].surfaceDef);
                controllerSurface.modelMatrix = node.GetGlobalTransform();
                surfaceList.push_back(controllerSurface);
            }
        }
    }
}

} // namespace OVRFW
