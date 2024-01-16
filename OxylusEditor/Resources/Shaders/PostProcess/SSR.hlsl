#define EPSILON 0.001
[[vk::binding(0, 0)]] RWTexture2D<float4> SSRTexture;
[[vk::binding(1, 0)]] Texture2D<float4> SrcTexture;
[[vk::binding(2, 0)]] Texture2D<float4> DepthTexture;
[[vk::binding(3, 0)]] Texture2D<float4> NormalTexture;

[[vk::binding(5, 0)]] cbuffer CameraBuffer {
  float4x4 projection;
  float4x4 view;
  float3 camPos;
}

[[vk::binding(6, 0)]] cbuffer CameraBuffer {
  int samples;               // default: 30
  int binary_search_samples; // default: 8
  float max_dist;            // default: 50
}

[[vk::binding(7, 0)]] SamplerState LinearSampler;

float3 ViewToNDC(float3 viewPos) {
  float4 clipPos = mul(projection, float4(viewPos, 1.0));
  return clipPos.xyz / clipPos.w;
}

float3 NDCToView(float3 ndc) {
  float4 viewPos = mul(transpose(projection), float4(ndc, 1.0));
  return viewPos.xyz / viewPos.w;
}


void CustomBinarySearch(float3 samplePoint,
                        float3 deltaStep,
                        inout float3 projectedSample) {
  // Go back one step at the beginning because we know we are too far
  deltaStep *= 0.5;
  samplePoint -= deltaStep * 0.5;
  for (int i = 1; i < binary_search_samples; i++) {
    projectedSample = ViewToNDC(samplePoint) * 0.5 + 0.5;
    const float depth = DepthTexture.SampleLevel(LinearSampler, projectedSample.xy, 0).r;
    deltaStep *= 0.5;
    if (projectedSample.z > depth) {
      samplePoint -= deltaStep;
    }
    else {
      samplePoint += deltaStep;
    }
  }
}

float3 SSR(float3 normal, float3 fragPos, float spec) {
  // Viewpos is origin in view space
  const float3 VIEW_POS = float3(0.0f, 0.0f, 0.0f);
  const float3 reflectDir = reflect(normalize(fragPos - VIEW_POS), normal);
  const float3 maxReflectPoint = fragPos + reflectDir * max_dist;
  const float3 deltaStep = (maxReflectPoint - fragPos) / samples;

  float3 samplePoint = fragPos;
  for (int i = 0; i < samples + 100; i++) {
    samplePoint += deltaStep;

    float3 projectedSample = ViewToNDC(samplePoint) * 0.5 + 0.5;
    if (any(projectedSample.xy >= float2(1.0f, 1.0f))
        || any(projectedSample.xy < float2(0.0f, 0.0f)) || projectedSample.z > 1.0) {
      return float3(0.0f, 0.0f, 0.0f);
    }
    const float depth = DepthTexture.SampleLevel(LinearSampler, projectedSample.xy, 0).r;
    if (projectedSample.z > depth) {
      CustomBinarySearch(samplePoint, deltaStep, projectedSample);
      return SrcTexture.SampleLevel(LinearSampler, projectedSample.xy, 0).rgb;
    }
  }

  return float3(0.0f, 0.0f, 0.0f);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
  const int2 imgCoord = int2(threadID.xy);
  uint width, height;
  SSRTexture.GetDimensions(width, height);
  float2 uv = (threadID + 0.5) / float2(width, height);

  float4 normalSpec = NormalTexture.SampleLevel(LinearSampler, uv, 0);
  float depth = DepthTexture.SampleLevel(LinearSampler, uv, 0).r;
  if (normalSpec.a < EPSILON || depth == 1.0) {
    SSRTexture[imgCoord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  float3 fragPos = NDCToView(float3(uv, depth) * 2.0 - 1.0);

  // world space to view space conversion for normals
  // mat3 normalToView = mat3(transpose(inverse(u_Ubo.view)));
  // normalSpec.xyz = normalize(normalToView * normalSpec.xyz);

  float3 color = SSR(normalSpec.xyz, fragPos, normalSpec.a);

  SSRTexture[imgCoord] = float4(color * normalSpec.a, 1.0f);
}
