#version 460

// Define a bias for undistorted distance
#define UDIST_BIAS 0.001

// Define a shader storage buffer object for cells
layout(set = 1, binding = 0) buffer CellBuffer {
  uint cells[];
}
cell_buffer;

// Define a shader storage buffer object for points
layout(set = 2, binding = 0) buffer PointBuffer {
  vec2 points[];
}
point_buffer;

// Define the input variables
layout(location = 0) in vec2 in_glyph_pos;        // Input glyph position
layout(location = 1) flat in uvec4 in_cell_info;  // Input cell information
layout(location = 2) in float in_sharpness;       // Input sharpness
layout(location = 3) in vec2 in_cell_coord;       // Input cell coordinates

// Define the output variables
layout(location = 0) out vec4 out_color;  // Output color

// Function to calculate the parameter t for a line segment
float calc_t(vec2 a, vec2 b, vec2 p) {
  vec2 dir = b - a;                           // Direction of the line segment
  float t = dot(p - a, dir) / dot(dir, dir);  // Parameter t
  return clamp(t, 0.0, 1.0);                  // Clamp t between 0 and 1
}

// Function to calculate the distance from a point to a line
float dist_to_line(vec2 a, vec2 b, vec2 p) {
  vec2 dir = b - a;                    // Direction of the line
  vec2 norm = vec2(-dir.y, dir.x);     // Normal to the line
  return dot(normalize(norm), a - p);  // Dot product of the normalized normal
                                       // and the vector from a to p
}

// Function to calculate the distance from a point to a quadratic Bezier curve
// at a given parameter t
float dist_to_bezier2(vec2 p0, vec2 p1, vec2 p2, float t, vec2 p) {
  vec2 q0 = mix(p0, p1, t);  // Interpolate between p0 and p1
  vec2 q1 = mix(p1, p2, t);  // Interpolate between p1 and p2
  return dist_to_line(
      q0, q1, p);  // Distance from p to the line segment between q0 and q1
}

// Function to process a quadratic Bezier curve
void process_bezier2(vec2 p, uint i, inout float min_udist, inout float v) {
  // Get the control points of the Bezier curve
  vec2 p0 = point_buffer.points[i];
  vec2 p1 = point_buffer.points[i + 1];
  vec2 p2 = point_buffer.points[i + 2];

  // Calculate the parameter t for the point on the Bezier curve closest to p
  float t = calc_t(p0, p2, p);
  // Calculate the undistorted distance from p to the point on the Bezier curve
  // at t
  float udist = distance(mix(p0, p2, t), p);

  // If the undistorted distance is less than or equal to the minimum
  // undistorted distance plus the bias
  if (udist <= min_udist + UDIST_BIAS) {
    // Calculate the distance from p to the Bezier curve at t
    float bez = dist_to_bezier2(p0, p1, p2, t, p);

    // If the undistorted distance is greater than or equal to the minimum
    // undistorted distance minus the bias
    if (udist >= min_udist - UDIST_BIAS) {
      // Get the previous point
      vec2 prevp = point_buffer.points[i - 2];
      // Calculate the distance from the previous point to the line segment
      // between p0 and p2
      float prevd = dist_to_line(p0, p2, prevp);
      // Mix the minimum distance to the Bezier curve and the maximum distance
      // to the Bezier curve based on the step function of prevd and 0.0
      v = mix(min(bez, v), max(bez, v), step(prevd, 0.0));
    } else {
      // Set v to the distance to the Bezier curve
      v = bez;
    }

    // Update the minimum undistorted distance
    min_udist = min(min_udist, udist);
  }
}

// Function to process a loop of quadratic Bezier curves
void process_bezier2_loop(vec2 p,
                          uint begin,
                          uint end,
                          inout float min_udist,
                          inout float v) {
  // For each Bezier curve in the loop
  for (uint i = begin; i < end; i += 2)
    process_bezier2(p, i, min_udist, v);  // Process the Bezier curve
}

// Function to calculate the signed distance from a point to a cell
float cell_signed_dist(uint point_offset, uint cell, vec2 p) {
  // Initialize the minimum undistorted distance and v
  float min_udist = 1.0 / 0.0;
  float v = -1.0 / 0.0;

  // Get the cell information
  uvec3 vcell = uvec3(cell, cell, cell);
  uvec3 len = (vcell >> uvec3(0, 2, 5)) & uvec3(3, 7, 7);
  uvec3 begin = point_offset + ((vcell >> uvec3(8, 16, 24)) & 0xFF) * 2;
  uvec3 end = begin + len * 2;

  // Process the Bezier curve loops
  process_bezier2_loop(p, begin.x, end.x, min_udist, v);
  process_bezier2_loop(p, begin.y, end.y, min_udist, v);
  process_bezier2_loop(p, begin.z, end.z, min_udist, v);

  // Return v
  return v;
}

// Main function of the fragment shader
void main() {
  // Calculate the cell index
  uvec2 c = min(uvec2(in_cell_coord), in_cell_info.zw - 1);
  uint cell_index = in_cell_info.y + in_cell_info.z * c.y + c.x;
  // Get the cell
  uint cell = cell_buffer.cells[cell_index];

  // Calculate the signed distance from the glyph position to the cell
  float v = cell_signed_dist(in_cell_info.x, cell, in_glyph_pos);
  // Calculate the alpha value
  float alpha = clamp(v * in_sharpness + 0.5, 0.0, 1.0);
  out_color = vec4(1.0, 1.0, 1.0, alpha);
}
