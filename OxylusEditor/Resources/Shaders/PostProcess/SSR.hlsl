#include "../Globals.hlsli"

float3 ViewToNDC(float3 viewPos) {
  float4 clipPos = mul(GetCamera().ProjectionMatrix, float4(viewPos, 1.0));
  return clipPos.xyz / clipPos.w;
}

float3 NDCToView(float3 ndc) {
  float4 viewPos = mul(transpose(GetCamera().ProjectionMatrix), float4(ndc, 1.0));
  return viewPos.xyz / viewPos.w;
}

void CustomBinarySearch(SSRData ssrData,
                        Texture2D<float4> depthTexture,
                        float3 samplePoint,
                        float3 deltaStep,
                        inout float3 projectedSample) {
  // Go back one step at the beginning because we know we are too far
  deltaStep *= 0.5;
  samplePoint -= deltaStep * 0.5;
  for (int i = 1; i < ssrData.BinarySearchSamples; i++) {
    projectedSample = ViewToNDC(samplePoint) * 0.5 + 0.5;
    const float depth = depthTexture.SampleLevel(LINEAR_CLAMPED_SAMPLER, projectedSample.xy, 0).r;
    deltaStep *= 0.5;
    if (projectedSample.z > depth) {
      samplePoint -= deltaStep;
    }
    else {
      samplePoint += deltaStep;
    }
  }
}

float3 SSR(SSRData ssrData, Texture2D<float4> depthTexture, Texture2D<float4> pbrTexture, float3 normal, float3 fragPos, float spec) {
  // Viewpos is origin in view space
  const float3 VIEW_POS = float3(0.0f, 0.0f, 0.0f);
  const float3 reflectDir = reflect(normalize(fragPos - VIEW_POS), normal);
  const float3 maxReflectPoint = fragPos + reflectDir * ssrData.MaxDist;
  const float3 deltaStep = (maxReflectPoint - fragPos) / ssrData.Samples;

  float3 samplePoint = fragPos;
  for (int i = 0; i < ssrData.Samples + 100; i++) {
    samplePoint += deltaStep;

    float3 projectedSample = ViewToNDC(samplePoint) * 0.5 + 0.5;
    if (any(projectedSample.xy >= float2(1.0f, 1.0f))
        || any(projectedSample.xy < float2(0.0f, 0.0f)) || projectedSample.z > 1.0) {
      return float3(0.0f, 0.0f, 0.0f);
    }
    const float depth = depthTexture.SampleLevel(LINEAR_CLAMPED_SAMPLER, projectedSample.xy, 0).r;
    if (projectedSample.z > depth) {
      CustomBinarySearch(ssrData, depthTexture, samplePoint, deltaStep, projectedSample);
      return pbrTexture.SampleLevel(LINEAR_CLAMPED_SAMPLER, projectedSample.xy, 0).rgb;
    }
  }

  return float3(0.0f, 0.0f, 0.0f);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
  const SSRData ssrData = GetSSRData();
  RWTexture2D<float4> ssrTexture = GetSSRRWTexture();
  Texture2D<float4> pbrTexture = GetPBRTexture();
  Texture2D<float4> normalTexture = GetNormalTexture();
  Texture2D<float4> depthTexture = GetDepthTexture();

  const int2 imgCoord = int2(threadID.xy);
  uint width, height;
  ssrTexture.GetDimensions(width, height);
  float2 uv = (threadID + 0.5) / float2(width, height);

  float4 normalSpec = normalTexture.SampleLevel(LINEAR_CLAMPED_SAMPLER, uv, 0);
  float depth = depthTexture.SampleLevel(LINEAR_CLAMPED_SAMPLER, uv, 0).r;
  if (normalSpec.a < EPSILON || depth == 1.0) {
    ssrTexture[imgCoord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  const float3 fragPos = NDCToView(float3(uv, depth) * 2.0 - 1.0);

  // world space to view space conversion for normals
  // mat3 normalToView = mat3(transpose(inverse(u_Ubo.view)));
  // normalSpec.xyz = normalize(normalToView * normalSpec.xyz);

  const float3 color = SSR(ssrData, depthTexture, pbrTexture, normalSpec.xyz, fragPos, normalSpec.a);

  ssrTexture[imgCoord] = float4(color * normalSpec.a, 1.0f);
}
