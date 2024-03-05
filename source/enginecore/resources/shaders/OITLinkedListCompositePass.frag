#version 460
#extension GL_GOOGLE_include_directive : require

layout(set = 0, binding = 0, r32ui) uniform coherent uimage2D headPointers;

struct Node {
  vec4 color;
  uint previousIndex;
  float depth;
  uint padding1;  // add 4 byte padding for alignment
  uint padding2;  // add 4 byte padding for alignment
};

layout(set = 0, binding = 1) buffer LinkedList {
  Node transparencyList[];
}
transparencyLinkedList;

layout(location = 0) out vec4 outputColor;

void main() {
  outputColor = vec4(0.0);

  // Get the head of the linked list for the current pixel
  uint nodeIndex = imageLoad(headPointers, ivec2(gl_FragCoord.xy)).x;

  // Create a temporary array to store the nodes for sorting
  Node nodes[20];  // Assuming a maximum of 20 overlapping fragments

  int numNodes = 0;

  // Iterate over the linked list
  while (nodeIndex != 0 && numNodes < 20) {
    nodes[numNodes] = transparencyLinkedList.transparencyList[nodeIndex];
    nodeIndex = nodes[numNodes].previousIndex;
    numNodes++;
  }

  // Sort the nodes array based on depth (simple bubble sort used here)
  for (int i = 0; i < numNodes; i++) {
    for (int j = i + 1; j < numNodes; j++) {
      if (nodes[j].depth > nodes[i].depth) {
        Node temp = nodes[i];
        nodes[i] = nodes[j];
        nodes[j] = temp;
      }
    }
  }

  // Blend the colors from back to front
  for (int i = 0; i < numNodes; i++) {
    outputColor = mix(outputColor, nodes[i].color, nodes[i].color.a);
  }
}
