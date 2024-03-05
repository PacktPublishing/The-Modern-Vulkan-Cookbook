// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

/************************************************************************************

Filename    :   ModelAnimationUtils.cpp
Content     :   Utility helpers for model animations.
Created     :   November 2, 2022
Authors     :   Peter Chan

*************************************************************************************/

#include "ModelAnimationUtils.h"
#include "ModelFile.h"

#include "Misc/Log.h"

using OVR::OVRMath_Lerp;
using OVR::Quatf;
using OVR::Vector3f;

namespace OVRFW {

static Vector3f AnimationInterpolateVector3f(
    float* buffer,
    int frame,
    float fraction,
    ModelAnimationInterpolation interpolationType) {
    Vector3f firstElement;
    firstElement.x = buffer[frame * 3 + 0];
    firstElement.y = buffer[frame * 3 + 1];
    firstElement.z = buffer[frame * 3 + 2];
    Vector3f secondElement;
    secondElement.x = buffer[frame * 3 + 3];
    secondElement.y = buffer[frame * 3 + 4];
    secondElement.z = buffer[frame * 3 + 5];

    if (interpolationType == MODEL_ANIMATION_INTERPOLATION_LINEAR) {
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_STEP) {
        if (fraction >= 1.0f) {
            return secondElement;
        } else {
            return firstElement;
        }
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE) {
        // #TODO implement MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE
        ALOGW("MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE not implemented");
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE) {
        // #TODO implement MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE
        ALOGW("MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE not implemented");
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else {
        ALOGW("invalid interpolation type on animation");
        return firstElement;
    }
}

static Quatf AnimationInterpolateQuatf(
    float* buffer,
    int frame,
    float fraction,
    ModelAnimationInterpolation interpolationType) {
    Quatf firstElement;
    firstElement.x = buffer[frame * 4 + 0];
    firstElement.y = buffer[frame * 4 + 1];
    firstElement.z = buffer[frame * 4 + 2];
    firstElement.w = buffer[frame * 4 + 3];
    Quatf secondElement;
    secondElement.x = buffer[frame * 4 + 4];
    secondElement.y = buffer[frame * 4 + 5];
    secondElement.z = buffer[frame * 4 + 6];
    secondElement.w = buffer[frame * 4 + 7];

    if (interpolationType == MODEL_ANIMATION_INTERPOLATION_LINEAR) {
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_STEP) {
        if (fraction >= 1.0f) {
            return secondElement;
        } else {
            return firstElement;
        }
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE) {
        ALOGW(
            "MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE does not make sense for quaternions.");
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE) {
        ALOGW("MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE does not make sense for quaternions.");
        firstElement = firstElement.Lerp(secondElement, fraction);
        return firstElement;
    } else {
        ALOGW("invalid interpolation type on animation");
        return firstElement;
    }
}

static std::vector<float> AnimationInterpolateWeights(
    const float* buffer,
    int numWeightsPerFrame,
    int frame,
    float fraction,
    ModelAnimationInterpolation interpolationType) {
    const int firstElementIndex = frame * numWeightsPerFrame;
    const int secondElementIndex = firstElementIndex + numWeightsPerFrame;
    const float* firstElement = buffer + firstElementIndex;
    const float* secondElement = buffer + secondElementIndex;

    std::vector<float> result(numWeightsPerFrame, 0.0f);
    if (interpolationType == MODEL_ANIMATION_INTERPOLATION_LINEAR) {
        for (int i = 0; i < numWeightsPerFrame; ++i) {
            result[i] = OVRMath_Lerp(firstElement[i], secondElement[i], fraction);
        }
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_STEP) {
        if (fraction >= 1.0f) {
            for (int i = 0; i < numWeightsPerFrame; ++i) {
                result[i] = secondElement[i];
            }
        } else {
            for (int i = 0; i < numWeightsPerFrame; ++i) {
                result[i] = firstElement[i];
            }
        }
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE) {
        ALOGW("MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE not implemented, treating as linear");
        for (int i = 0; i < numWeightsPerFrame; ++i) {
            result[i] = OVRMath_Lerp(firstElement[i], secondElement[i], fraction);
        }
    } else if (interpolationType == MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE) {
        ALOGW("MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE not implemented, treating as linear");
        for (int i = 0; i < numWeightsPerFrame; ++i) {
            result[i] = OVRMath_Lerp(firstElement[i], secondElement[i], fraction);
        }
    } else {
        ALOGW("invalid interpolation type on animation");
    }
    return result;
}

void ApplyAnimation(ModelState& modelState, int animationIndex) {
    const ModelAnimation& animation = modelState.mf->Animations[animationIndex];
    for (const ModelAnimationChannel& channel : animation.channels) {
        ModelNodeState& nodeState = modelState.nodeStates[channel.nodeIndex];
        const ModelAnimationTimeLineState& timeLineState =
            modelState.animationTimelineStates[channel.sampler->timeLineIndex];

        float* bufferData = (float*)(channel.sampler->output->BufferData());
        if (channel.path == MODEL_ANIMATION_PATH_TRANSLATION) {
            Vector3f translation = AnimationInterpolateVector3f(
                bufferData,
                timeLineState.frame,
                timeLineState.fraction,
                channel.sampler->interpolation);
            nodeState.translation = translation;
        } else if (channel.path == MODEL_ANIMATION_PATH_SCALE) {
            Vector3f scale = AnimationInterpolateVector3f(
                bufferData,
                timeLineState.frame,
                timeLineState.fraction,
                channel.sampler->interpolation);
            nodeState.scale = scale;
        } else if (channel.path == MODEL_ANIMATION_PATH_ROTATION) {
            Quatf rotation = AnimationInterpolateQuatf(
                bufferData,
                timeLineState.frame,
                timeLineState.fraction,
                channel.sampler->interpolation);
            nodeState.rotation = rotation;
        } else if (channel.path == MODEL_ANIMATION_PATH_WEIGHTS) {
            const int numWeightsPerFrame =
                channel.sampler->output->count / channel.sampler->input->count;
            std::vector<float> weights = AnimationInterpolateWeights(
                bufferData,
                numWeightsPerFrame,
                timeLineState.frame,
                timeLineState.fraction,
                channel.sampler->interpolation);
            if (nodeState.weights.size() != weights.size()) {
                ALOGE(
                    "Mismatch animation weights count, node:%zu, animation:%zu, channel:%d, '%s'",
                    nodeState.weights.size(),
                    weights.size(),
                    channel.nodeIndex,
                    animation.name.c_str());
                continue;
            }
            if (channel.additiveWeightIndex >= 0) {
                nodeState.weights[channel.additiveWeightIndex] +=
                    weights[channel.additiveWeightIndex];
            } else {
                nodeState.weights = weights;
            }
        } else {
            ALOGW("Bad animation path on channel '%s'", animation.name.c_str());
        }

        nodeState.CalculateLocalTransform();
    }
}

} // namespace OVRFW
