layout(push_constant, std140) uniform Material {
  layout(offset = 64) vec4 Color;
  layout(offset = 80) vec4 Emmisive;
  layout(offset = 96) float Roughness;
  layout(offset = 100) float Metallic;
  layout(offset = 104) float Specular;
  layout(offset = 108) float Normal;
  layout(offset = 112) float AO;
  layout(offset = 116) bool UseAlbedo;
  layout(offset = 120) bool UseRoughness;
  layout(offset = 124) bool UseMetallic;
  layout(offset = 128) bool UseNormal;
  layout(offset = 132) bool UseAO;
  layout(offset = 136) bool UseEmissive;
  layout(offset = 140) bool UseSpecular;
  layout(offset = 144) bool FlipImage;
  layout(offset = 148) float AlphaCutoff;
  layout(offset = 152) bool DoubleSided;
  layout(offset = 156) uint UVScale;
}
u_Material;