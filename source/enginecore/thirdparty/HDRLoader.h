#pragma once

// Code copied from
// https://github.com/nvpro-samples/vk_raytrace/blob/master/src/hdr_sampling.cpp

#include <glm/glm.hpp>
#include <iostream>
#include <numeric>
#include <vector>

struct EnvAccel {
  uint32_t alias;
  float q;
  float pdf;
  float aliasPdf;
};

std::vector<EnvAccel> createEnvironmentAccel(const float* pixels, uint32_t rx,
                                             uint32_t ry);
