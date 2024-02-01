#include "../Globals.hlsli"

struct VSOutput {
  float4 Position : SV_Position;
  float2 UV : TEXCOORD;
  float3 NearPoint : NEAR_POINT;
  float3 FarPoint : FAR_POINT;
};

float3 UnprojectPoint(float x, float y, float z, float4x4 view, float4x4 projection) {
  float4 unprojectedPoint = mul(mul(view, projection), float4(x, y, z, 1.0));
  return unprojectedPoint.xyz / unprojectedPoint.w;
}

VSOutput main(Vertex inVertex) {
  VSOutput output;
  output.UV = inVertex.UV;

  float3 position = inVertex.Position;
  output.NearPoint = UnprojectPoint(position.x, position.y, 0.0, GetCamera().InvViewMatrix, GetCamera().InvProjectionMatrix).xyz;
  output.FarPoint = UnprojectPoint(position.x, position.y, 1.0, GetCamera().InvViewMatrix, GetCamera().InvProjectionMatrix).xyz;
  output.Position = float4(position.xyz, 1.0);

  return output;
}

struct PSOut {
  float4 Color : SV_Target;
  float Depth : SV_Depth;
};

static const float step = 100.0f;
static const float subdivisions = 10.0f;

float4 Grid(float3 fragPos3D, float scale, bool drawAxis) {
  float2 coord = fragPos3D.xz * scale;

  float2 derivative = fwidth(coord);
  float2 grid = abs(frac(coord - 0.5) - 0.5) / derivative;
  float _line = min(grid.x, grid.y);
  float minimumz = min(derivative.y, 1);
  float minimumx = min(derivative.x, 1);
  float4 colour = float4(0.35, 0.35, 0.35, 1.0 - min(_line, 1.0));
  // z axis
  if (fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
    colour = float4(0.0, 0.0, 1.0, colour.w);
  // x axis
  if (fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
    colour = float4(1.0, 0.0, 0.0, colour.w);
  return colour;
}

float ComputeDepth(float3 pos) {
  float4 clipSpacePos = mul(mul(GetCamera().ProjectionMatrix, GetCamera().ViewMatrix), float4(pos.xyz, 1.0));
  return (clipSpacePos.z / clipSpacePos.w);
}

float ComputeLinearDepth(float3 pos) {
  float4 clipSpacePos = mul(mul(GetCamera().ProjectionMatrix, GetCamera().ViewMatrix), float4(pos.xyz, 1.0));
  float clipSpaceDepth = (clipSpacePos.z / clipSpacePos.w) * 2.0 - 1.0; // put back between -1 and 1
  float linearDepth = (2.0 * GetCamera().NearClip * GetCamera().FarClip)
                      / (GetCamera().FarClip + GetCamera().NearClip - clipSpaceDepth * (GetCamera().FarClip - GetCamera().NearClip)); // get linear value between 0.01 and 100
  return linearDepth / GetCamera().FarClip;                                                                                           // normalize
}

int RoundToPowerOfTen(float n) { return int(pow(10.0, floor((1 / log(10)) * log(n)))); }

PSOut PSmain(VSOutput input, float4 pixelPosition : SV_Position) {
  PSOut psOut;

  const float t = -input.NearPoint.y / (input.FarPoint.y - input.NearPoint.y);
  if (t < 0.)
    discard;

  float3 fragPos3D = input.NearPoint + t * (input.FarPoint - input.NearPoint);
  fragPos3D.y -= 0.1; // depth fighting with zero plane fix

  psOut.Depth = ComputeDepth(fragPos3D);

  float linearDepth = ComputeLinearDepth(fragPos3D);
  float fading = max(0, (0.5 - linearDepth));

  float decreaseDistance = GetCamera().FarClip * 1.5;
  float3 pseudoViewPos = float3(GetCamera().Position.x, fragPos3D.y, GetCamera().Position.z);

  float dist, angleFade;
  if (GetCamera().ProjectionMatrix[3][3] == 0.0) {
    float3 viewvec = GetCamera().Position.xyz - fragPos3D;
    dist = length(viewvec);
    viewvec /= dist;

    float angle = viewvec.y;
    angle = 1.0 - abs(angle);
    angle *= angle;
    angleFade = 1.0 - angle * angle;
    angleFade *= 1.0 - smoothstep(0.0, GetScene().GridMaxDistance, dist - GetScene().GridMaxDistance);
  }
  else {
    dist = pixelPosition.z * 2.0 - 1.0;
    /* Avoid fading in +Z direction in camera view (see T70193). */
    dist = clamp(dist, 0.0, 1.0); // abs(dist);
    angleFade = 1.0 - smoothstep(0.0, 0.5, dist - 0.5);
    dist = 1.0; /* avoid branch after */
  }

  const float distanceToCamera = abs(GetCamera().Position.y - fragPos3D.y);
  const int powerOfTen = max(1, RoundToPowerOfTen(distanceToCamera));
  const float divs = 1.0f / float(powerOfTen);
  const float secondFade = smoothstep(subdivisions / divs, 1 / divs, distanceToCamera);

  float4 grid1 = Grid(fragPos3D, divs / subdivisions, true);
  float4 grid2 = Grid(fragPos3D, divs, true);

  // secondFade*= smoothstep(GetCamera().FarClip / 0.5, GetCamera().FarClip, distanceToCamera);
  grid2.a *= secondFade;
  grid1.a *= (1 - secondFade);

  psOut.Color = grid1 + grid2; // adding multiple resolution for the grid
  psOut.Color.a = max(grid1.a, grid2.a);

  psOut.Color *= float(t > 0);
  psOut.Color.a *= fading;
  psOut.Color.a *= angleFade;

  return psOut;
}
