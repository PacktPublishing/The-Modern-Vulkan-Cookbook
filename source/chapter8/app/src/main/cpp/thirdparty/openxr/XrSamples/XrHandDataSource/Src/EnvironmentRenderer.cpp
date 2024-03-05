/************************************************************************************************
Filename    :   EnvironmentRenderer.cpp
Content     :   A variant of ModelRenderer suited for rendering gltf scenes with vertex color based fog
Created     :   July 2023
Authors     :   Alexander Borsboom
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.
************************************************************************************************/
#include "EnvironmentRenderer.h"
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
namespace EnvironmentShaders {

/// clang-format off
static const char* VertexShaderSrc = R"glsl(
attribute highp vec4 Position;
attribute highp vec3 Normal;
attribute highp vec2 TexCoord;
attribute lowp vec4 VertexColor;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;
varying lowp vec4 oVertexColor;

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
  oVertexColor = VertexColor;
}
)glsl";

/// This shader uses vertex color.r for a fog, fading to a fog color as vertex color decreases to 0.
/// This gives behaviour consistent with our unity samples.
static const char* FragmentShaderSrc = R"glsl(
precision lowp float;

uniform sampler2D Texture0;
uniform sampler2D Texture1;
uniform lowp vec3 SpecularLightDirection;
uniform lowp vec3 SpecularLightColor;
uniform lowp vec3 AmbientLightColor;
uniform lowp float FogStrength;
uniform lowp vec3 FogColor;

varying lowp vec3 oEye;
varying lowp vec3 oNormal;
varying lowp vec2 oTexCoord;
varying lowp vec4 oVertexColor;

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
  lowp vec4 detail = texture2D( Texture1, oTexCoord * 20.0 );
  lowp vec4 res = 0.5 * (diffuse + detail);
  lowp vec3 ambientValue = res.xyz * AmbientLightColor;

  lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
  lowp vec3 diffuseValue = res.xyz * SpecularLightColor * nDotL;

  lowp float specularPower = 1.0f - res.a;
  specularPower = specularPower * specularPower;

  lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
  lowp float nDotH = max( dot( Normal, H ), 0.0 );
  lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
  lowp vec3 specularValue = specularIntensity * SpecularLightColor;

  lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;

  lowp float fog = FogStrength * (1.0 - oVertexColor.r);
  controllerColor = fog * FogColor + (1.0 - fog) * controllerColor;

  gl_FragColor.w = 1.0;
  gl_FragColor.xyz = controllerColor;
}
)glsl";

/// clang-format on

} // namespace EnvironmentRenderer

bool EnvironmentRenderer::Init(std::string modelPath, OVRFW::ovrFileSys* fileSys) {
    /// Shader
    ovrProgramParm UniformParms[] = {
        {"Texture0", ovrProgramParmType::TEXTURE_SAMPLED},
        {"Texture1", ovrProgramParmType::TEXTURE_SAMPLED}, // An optional detail texture.
        {"SpecularLightDirection", ovrProgramParmType::FLOAT_VECTOR3},
        {"SpecularLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"AmbientLightColor", ovrProgramParmType::FLOAT_VECTOR3},
        {"FogStrength", ovrProgramParmType::FLOAT},
        {"FogColor", ovrProgramParmType::FLOAT_VECTOR3},
    };
    ProgRenderModel = GlProgram::Build(
        "",
        EnvironmentShaders::VertexShaderSrc,
        "",
        EnvironmentShaders::FragmentShaderSrc,
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

    FogStrengths = new OVR::Size<float>[RenderModel->Models.size()];
    int modelIndex = 0;
    for (auto& model : RenderModel->Models) {
        auto& gc = model.surfaces[0].surfaceDef.graphicsCommand;
        gc.UniformData[0].Data = &gc.Textures[0];
        gc.UniformData[1].Data = &gc.Textures[1];
        gc.UniformData[2].Data = &SpecularLightDirection;
        gc.UniformData[3].Data = &SpecularLightColor;
        gc.UniformData[4].Data = &AmbientLightColor;
        FogStrengths[modelIndex] = OVR::Size<float>(gc.GpuState.blendEnable == ovrGpuState::BLEND_ENABLE ? 1.0f : 0.0f);
        gc.UniformData[5].Data = &FogStrengths[modelIndex];
        gc.UniformData[6].Data = &FogColor;
        gc.GpuState.depthEnable = gc.GpuState.depthMaskEnable = true;
        modelIndex++;
    }

    /// Set defaults
    SpecularLightDirection = Vector3f(1.0f, 1.0f, 0.0f);
    SpecularLightColor = Vector3f(1.0f, 0.95f, 0.8f) * 0.75f;
    AmbientLightColor = Vector3f(1.0f, 1.0f, 1.0f) * 0.15f;
    FogColor = Vector3f(0.3372549f, 0.345098f, 0.3686275f);

    /// all good
    Initialized = true;
    return true;
}

void EnvironmentRenderer::Shutdown() {
    OVRFW::GlProgram::Free(ProgRenderModel);
    if (RenderModel != nullptr) {
        delete RenderModel;
        RenderModel = nullptr;
    }
    if(FogStrengths != nullptr) {
        delete FogStrengths;
        FogStrengths = nullptr;
    }
}

void EnvironmentRenderer::Render(std::vector<ovrDrawSurface>& surfaceList) {
    /// toggle alpha override
    if (RenderModel != nullptr) {
        for( int i=0; i < static_cast<int>(RenderModel->Models.size()); i++ ) {
            auto& model = RenderModel->Models[i];
            auto& node = RenderModel->Nodes[i+1];
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
