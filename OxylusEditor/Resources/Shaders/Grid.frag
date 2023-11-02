#version 450
#pragma shader_stage(fragment)

layout(location = 0) out vec4 out_Colour;

layout(set = 0, binding = 1) uniform UniformBuffer {
  vec4 u_CameraPos;
  float u_Near;
  float u_Far;
  float u_MaxDistance;
};

layout(location = 0) in vec3 in_FragPosition;
layout(location = 1) in vec2 in_FragTexCoord;

layout(location = 2) in vec3 in_NearPoint;
layout(location = 3) in vec3 in_FarPoint;
layout(location = 4) in mat4 in_FragView;
layout(location = 8) in mat4 in_FragProj;

const float step = 100.0f;
const float subdivisions = 10.0f;

vec4 grid(vec3 fragPos3D, float scale, bool drawAxis) {
  vec2 coord = fragPos3D.xz * scale;

  vec2 derivative = fwidth(coord);
  vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
  float line = min(grid.x, grid.y);
  float minimumz = min(derivative.y, 1);
  float minimumx = min(derivative.x, 1);
  vec4 colour = vec4(0.35, 0.35, 0.35, 1.0 - min(line, 1.0));
  // z axis
  if (fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx) colour = vec4(0.0, 0.0, 1.0, colour.w);
  // x axis
  if (fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz) colour = vec4(1.0, 0.0, 0.0, colour.w);
  return colour;
}

float compute_depth(vec3 pos) {
  vec4 clip_space_pos = in_FragProj * in_FragView * vec4(pos.xyz, 1.0);
  return (clip_space_pos.z / clip_space_pos.w);
}

float compute_linear_depth(vec3 pos) {
  vec4 clip_space_pos = in_FragProj * in_FragView * vec4(pos.xyz, 1.0);
  float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0;                           // put back between -1 and 1
  float linearDepth = (2.0 * u_Near * u_Far) / (u_Far + u_Near - clip_space_depth * (u_Far - u_Near));  // get linear value between 0.01 and 100
  return linearDepth / u_Far;                                                                           // normalize
}

int round_to_power_of_ten(float n) { return int(pow(10.0, floor((1 / log(10)) * log(n)))); }

void main() {
  float t = -in_NearPoint.y / (in_FarPoint.y - in_NearPoint.y);
  if (t < 0.) discard;

  vec3 fragPos3D = in_NearPoint + t * (in_FarPoint - in_NearPoint);

  gl_FragDepth = compute_depth(fragPos3D);

  float linearDepth = compute_linear_depth(fragPos3D);
  float fading = max(0, (0.5 - linearDepth));

  float decreaseDistance = u_Far * 1.5;
  vec3 pseudoViewPos = vec3(u_CameraPos.x, fragPos3D.y, u_CameraPos.z);

  float dist, angleFade;
  if (in_FragProj[3][3] == 0.0) {
    vec3 viewvec = u_CameraPos.xyz - fragPos3D;
    dist = length(viewvec);
    viewvec /= dist;

    float angle;
    angle = viewvec.y;
    angle = 1.0 - abs(angle);
    angle *= angle;
    angleFade = 1.0 - angle * angle;
    angleFade *= 1.0 - smoothstep(0.0, u_MaxDistance, dist - u_MaxDistance);
  } else {
    dist = gl_FragCoord.z * 2.0 - 1.0;
    /* Avoid fading in +Z direction in camera view (see T70193). */
    dist = clamp(dist, 0.0, 1.0);  // abs(dist);
    angleFade = 1.0 - smoothstep(0.0, 0.5, dist - 0.5);
    dist = 1.0; /* avoid branch after */
  }

  float distanceToCamera = abs(u_CameraPos.y - fragPos3D.y);
  int powerOfTen = round_to_power_of_ten(distanceToCamera);
  powerOfTen = max(1, powerOfTen);
  float divs = 1.0 / float(powerOfTen);
  float secondFade = smoothstep(subdivisions / divs, 1 / divs, distanceToCamera);

  vec4 grid1 = grid(fragPos3D, divs / subdivisions, true);
  vec4 grid2 = grid(fragPos3D, divs, true);

  // secondFade*= smoothstep(u_Far / 0.5, u_Far, distanceToCamera);
  grid2.a *= secondFade;
  grid1.a *= (1 - secondFade);

  out_Colour = grid1 + grid2;  // adding multiple resolution for the grid
  out_Colour.a = max(grid1.a, grid2.a);

  out_Colour *= float(t > 0);
  out_Colour.a *= fading;
  out_Colour.a *= angleFade;
}