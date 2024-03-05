#version 460
#extension GL_GOOGLE_include_directive : require

struct Node {
  vec4 color;
  uint previousIndex;
  float depth;
  uint padding1;  // add 4 byte padding for alignment
  uint padding2;  // add 4 byte padding for alignment
};

layout(set = 1, binding = 0) uniform ObjectProperties {
  vec4 color;
  mat4 model;
}
objectProperties;

layout(set = 2, binding = 0) buffer AtomicCounter {
  uint counter;
};

layout(set = 2, binding = 1) buffer LinkedList {
  Node transparencyList[];
}
transparencyLinkedList;

layout(set = 2, binding = 2, r32ui) uniform coherent uimage2D headPointers;

layout(location = 0) out vec4 outputColor;

void main() {
  // Set the output color to transparent
  outputColor = vec4(0.0);

  // Atomic operation to get unique index for each fragment, don't return 0
  // since that will be used as ll terminator
  uint newNodeIndex = atomicAdd(counter, 1) + 1;

  ivec2 size = imageSize(headPointers);

  // max size of linked list * width * height
  if (newNodeIndex > (10 * size.x * size.y) - 1) {
    return;
  }

  // Atomic operation to insert the new node at the beginning of the linked list
  uint oldHeadIndex =
      imageAtomicExchange(headPointers, ivec2(gl_FragCoord.xy), newNodeIndex);

  transparencyLinkedList.transparencyList[newNodeIndex].previousIndex =
      oldHeadIndex;
  transparencyLinkedList.transparencyList[newNodeIndex].color =
      objectProperties.color;
  transparencyLinkedList.transparencyList[newNodeIndex].depth = gl_FragCoord.z;
  transparencyLinkedList.transparencyList[newNodeIndex].padding1 = 0;
  transparencyLinkedList.transparencyList[newNodeIndex].padding2 = 0;
}
