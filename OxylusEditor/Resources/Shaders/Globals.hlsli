#include "Common.hlsli"

cbuffer CameraBuffer : register(b0) {
  CameraData Camera;
}

cbuffer SceneBuffer : register(b1) {
  SceneData Scene;
}
