// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

/************************************************************************************

Filename    :   SimpleCollisionRenderer.h
Content     :   Based on SimpleBeamRenderer, this renderer takes a subset of beam ids
                for which it only draws particles and can treat differently in other
                ways--coloring the particles individually, etc.
Created     :   October 2022
Authors     :   Isaac Greenbride

************************************************************************************/

#pragma once

#include "OVR_Math.h"
#include "BeamRenderer.h"
#include "ParticleSystem.h"
#include "Input/TinyUI.h"
#include <random>

namespace OVRFW {

static float get_random_to_one_float() {
    static std::default_random_engine e;
    static std::uniform_real_distribution<> dis(0, 1.0f); // range 0 - 1.0f
    return dis(e);
}
static int get_random_to_zero_two() {
    static std::default_random_engine e;
    static std::uniform_real_distribution<> dis(0, 2); // range 0 - 3
    return dis(e);
}

class SimpleCollisionRenderer {
   public:
    SimpleCollisionRenderer() = default;
    ~SimpleCollisionRenderer() {
        delete spriteAtlas_;
    }

    void Init(
        OVRFW::ovrFileSys* fileSys,
        const char* particleTexture,
        OVR::Vector4f particleColor,
        float scale = 1.0f,
        std::vector<int> noBeamIds = {},
        bool randomizeNoBeamParticleColor = false,
        bool showNoBeamParticles = true) {
        PointerParticleColor = particleColor;
        Scale = scale;
        RandomizeNoBeamParticleColor = randomizeNoBeamParticleColor;
        ShowNoBeamParticles = showNoBeamParticles;
        beamRenderer_.Init(256, true);
        UpdateNoBeamIds(noBeamIds);

        if (particleTexture != nullptr) {
            spriteAtlas_ = new OVRFW::ovrTextureAtlas();
            spriteAtlas_->Init(*fileSys, particleTexture);
            spriteAtlas_->BuildSpritesFromGrid(4, 2, 8);
            particleSystem_.Init(
                1024, spriteAtlas_, ovrParticleSystem::GetDefaultGpuState(), false);
        } else {
            particleSystem_.Init(1024, nullptr, ovrParticleSystem::GetDefaultGpuState(), false);
        }
    }

    void Shutdown() {
        beamRenderer_.Shutdown();
        particleSystem_.Shutdown();
    }

    void ShowRandomParticleColor(bool makeRandom) {
        RandomizeNoBeamParticleColor = makeRandom;
    }

    void ShowParticlesForSpecifiedIds(bool show) {
        ShowNoBeamParticles = show;
    }

    void UpdateNoBeamIds(const std::vector<int>& noBeamIds) {
        for (int id : noBeamIds) {
            std::vector<float> rgb = {0.0f, 0.0f, 0.0f};
            rgb[get_random_to_zero_two()] =
                get_random_to_one_float(); // just changing one channel at a time
            beamlessDeviceIds_.insert({id, OVR::Vector4f(rgb[0], rgb[1], rgb[2], 1.0f)});
        }
    }

    void Update(
        const OVRFW::ovrApplFrameIn& in,
        const std::vector<OVRFW::TinyUI::HitTestDevice>& hitTestDevices) {
        // Clear old beams and particles
        for (auto h : beams_) {
            beamRenderer_.RemoveBeam(h);
        }
        for (auto h : particles_) {
            particleSystem_.RemoveParticle(h);
        }

        // Add UI pointers to render
        for (auto& device : hitTestDevices) {
            bool showBeam = beamlessDeviceIds_.count(device.deviceNum) == 0;
            if (showBeam) {
                constexpr float beamLength = 0.5f; // 0.5 meter beam length
                const OVR::Vector3f beamDir =
                    ((device.pointerEnd - device.pointerStart) * 0.5f).Normalized();
                const OVR::Vector3f beamEnd = device.pointerStart + beamDir * beamLength;
                const auto& beam =
                    beamRenderer_.AddBeam(in, 0.015f, device.pointerStart, beamEnd, BeamColor);
                beams_.push_back(beam);
            }
            if (device.hitObject && (showBeam || ShowNoBeamParticles)) {
                OVR::Vector4f currentPointerParticleColor =
                    showBeam || !RandomizeNoBeamParticleColor
                    ? PointerParticleColor
                    : beamlessDeviceIds_[device.deviceNum];
                const auto& particle = particleSystem_.AddParticle(
                    in,
                    device.pointerEnd,
                    0.0f,
                    OVR::Vector3f(0.0f),
                    OVR::Vector3f(0.0f),
                    currentPointerParticleColor,
                    ovrEaseFunc::NONE,
                    0.0f,
                    0.05f * Scale,
                    0.1f,
                    0);
                particles_.push_back(particle);
            }
        }
    }
    void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) {
        /// Render beams
        particleSystem_.Frame(in, spriteAtlas_, out.FrameMatrices.CenterView);
        particleSystem_.RenderEyeView(
            out.FrameMatrices.CenterView, out.FrameMatrices.EyeProjection[0], out.Surfaces);
        beamRenderer_.Frame(in, out.FrameMatrices.CenterView);
        beamRenderer_.Render(out.Surfaces);
    }

   public:
    OVR::Vector4f PointerParticleColor = {0.5f, 0.8f, 1.0f, 1.0f};
    OVR::Vector4f BeamColor = {0.5f, 0.8f, 1.0f, 1.0f};

   private:
    OVRFW::ovrBeamRenderer beamRenderer_;
    OVRFW::ovrParticleSystem particleSystem_;
    OVRFW::ovrTextureAtlas* spriteAtlas_ = nullptr;
    std::unordered_map<int, OVR::Vector4f> beamlessDeviceIds_;
    std::vector<OVRFW::ovrBeamRenderer::handle_t> beams_;
    std::vector<OVRFW::ovrParticleSystem::handle_t> particles_;
    float Scale;
    bool RandomizeNoBeamParticleColor;
    bool ShowNoBeamParticles;
};

} // namespace OVRFW
