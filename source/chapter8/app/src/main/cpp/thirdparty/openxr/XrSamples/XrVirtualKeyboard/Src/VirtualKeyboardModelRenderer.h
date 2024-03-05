/************************************************************************************************
Filename    :   VirtualKeyboardModelRenderer.h
Content     :   Helper class for rendering the virtual keyboard model
Created     :   January 2023
Authors     :   Peter Chan
Language    :   C++
Copyright   :   Copyright (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
************************************************************************************************/

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include <OVR_Math.h>

#include <Model/ModelFile.h>
#include <Render/SurfaceRender.h>

class VirtualKeyboardModelRenderer {
   public:
    bool Init(const std::vector<uint8_t>& modelBuffer);
    void Shutdown();

    void Update(const OVR::Posef& pose, const OVR::Vector3f& scale);
    void Render(std::vector<OVRFW::ovrDrawSurface>& surfaceList);

    void UpdateTexture(
        uint64_t textureId,
        const uint8_t* textureData,
        uint32_t textureWidth,
        uint32_t textureHeight);
    void SetAnimationState(int animationIndex, float fraction);
    void UpdateSurfaceGeo();

    bool IsModelLoaded() const;
    bool IsPointNearKeyboard(const OVR::Vector3f& globalPoint) const;
    OVR::Bounds3f GetCollisionBounds() const;

   private:
    std::unique_ptr<OVRFW::ModelFile> keyboardModel_;
    std::unique_ptr<OVRFW::ModelState> keyboardModelState_;

    OVRFW::GlProgram progKeyboard_;
    OVR::Matrix4f transform_;
    const OVRFW::ModelNode* collisionNode_ = nullptr;

    std::map<uint64_t, OVRFW::GlTexture> textureIdMap_;
    std::vector<int> dirtyGeoNodeIndices_;
};
