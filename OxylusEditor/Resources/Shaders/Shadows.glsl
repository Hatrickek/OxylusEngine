float HardenedKernel(float x) {
    // this is basically a stronger smoothstep()
    x = 2.0 * x - 1.0;
    float s = sign(x);
    x = 1.0 - s * x;
    x = x * x * x;
    x = s - x * s;
    return 0.5 * x + 0.5;
}

// Poisson disk generated with 'poisson-disk-generator' tool from
// https://github.com/corporateshark/poisson-disk-generator by Sergey Kosarevsky
/*const*/ mediump vec2 PoissonDisk[64] = vec2[](
vec2(0.511749, 0.547686), vec2(0.58929, 0.257224), vec2(0.165018, 0.57663), vec2(0.407692, 0.742285),
vec2(0.707012, 0.646523), vec2(0.31463, 0.466825), vec2(0.801257, 0.485186), vec2(0.418136, 0.146517),
vec2(0.579889, 0.0368284), vec2(0.79801, 0.140114), vec2(-0.0413185, 0.371455), vec2(-0.0529108, 0.627352),
vec2(0.0821375, 0.882071), vec2(0.17308, 0.301207), vec2(-0.120452, 0.867216), vec2(0.371096, 0.916454),
vec2(-0.178381, 0.146101), vec2(-0.276489, 0.550525), vec2(0.12542, 0.126643), vec2(-0.296654, 0.286879),
vec2(0.261744, -0.00604975), vec2(-0.213417, 0.715776), vec2(0.425684, -0.153211), vec2(-0.480054, 0.321357),
vec2(-0.0717878, -0.0250567), vec2(-0.328775, -0.169666), vec2(-0.394923, 0.130802), vec2(-0.553681, -0.176777),
vec2(-0.722615, 0.120616), vec2(-0.693065, 0.309017), vec2(0.603193, 0.791471), vec2(-0.0754941, -0.297988),
vec2(0.109303, -0.156472), vec2(0.260605, -0.280111), vec2(0.129731, -0.487954), vec2(-0.537315, 0.520494),
vec2(-0.42758, 0.800607), vec2(0.77309, -0.0728102), vec2(0.908777, 0.328356), vec2(0.985341, 0.0759158),
vec2(0.947536, -0.11837), vec2(-0.103315, -0.610747), vec2(0.337171, -0.584), vec2(0.210919, -0.720055),
vec2(0.41894, -0.36769), vec2(-0.254228, -0.49368), vec2(-0.428562, -0.404037), vec2(-0.831732, -0.189615),
vec2(-0.922642, 0.0888026), vec2(-0.865914, 0.427795), vec2(0.706117, -0.311662), vec2(0.545465, -0.520942),
vec2(-0.695738, 0.664492), vec2(0.389421, -0.899007), vec2(0.48842, -0.708054), vec2(0.760298, -0.62735),
vec2(-0.390788, -0.707388), vec2(-0.591046, -0.686721), vec2(-0.769903, -0.413775), vec2(-0.604457, -0.502571),
vec2(-0.557234, 0.00451362), vec2(0.147572, -0.924353), vec2(-0.0662488, -0.892081), vec2(0.863832, -0.407206)
);

float GetPenumbraRatio(const bool DIRECTIONAL, const int index, float z_receiver, float z_blocker) {
    // z_receiver/z_blocker are not linear depths (i.e. they're not distances)
    // Penumbra ratio for PCSS is given by:  pr = (d_receiver - d_blocker) / d_blocker
    float penumbraRatio;
    if (DIRECTIONAL) {
        // TODO: take lispsm into account
        // For directional lights, the depths are linear but depend on the position (because of LiSPSM).
        // With:        z_linear = f + z * (n - f)
        // We get:      (r-b)/b ==> (f/(n-f) + r_linear) / (f/(n-f) + b_linear) - 1
        // Assuming f>>n and ignoring LISPSM, we get:
        penumbraRatio = (z_blocker - z_receiver) / (1.0 - z_blocker);
    } else {
        // For spotlights, the depths are congruent to 1/z, specifically:
        //      z_linear = (n * f) / (n + z * (f - n))
        // replacing in (r - b) / b gives:
        float nearOverFarMinusNear = 0.01 / (1000 - 0.01);// shadowUniforms.shadows[index].nearOverFarMinusNear; TODO: parameterize
        penumbraRatio = (nearOverFarMinusNear + z_blocker) / (nearOverFarMinusNear + z_receiver) - 1.0;
    }
    return penumbraRatio;
}

void BlockerSearchAndFilter(out float occludedCount,
out float z_occSum,
const mediump sampler2DArray map,
const highp vec4 scissorNormalized,
const highp vec2 uv,
const float z_rec,
const uint layer,
const highp vec2 filterRadii,
const mat2 R,
const float shadowBias,
const uint tapCount) {
    occludedCount = 0.0;
    z_occSum = 0.0;
    
    for (uint i = 0u; i < tapCount; i++) {
        highp vec2 duv = R * (PoissonDisk[i] * filterRadii);
        highp vec2 tc = clamp(uv + duv, 0, 1);

        float z_occ = texture(map, vec3(uv + duv, layer)).r;

        // note: z_occ and z_rec are not necessarily linear here, comparing them is always okay for
        // the regular PCF, but the "distance" is meaningless unless they are actually linear
        // (e.g.: for the directional light).
        // Either way, if we assume that all the samples are close to each other we can take their
        // average regardless, and the average depth value of the occluders
        // becomes: z_occSum / occludedCount.

        // receiver plane depth bias
        //float dz = z_occ - z_rec;// dz>0 when blocker is between receiver and light
        float occluded = step(z_rec - shadowBias, z_occ);
        occludedCount += occluded;
        z_occSum += z_occ * occluded;
    }
}

mat2 GetRandomRotationMatrix(highp vec2 fragCoord) {
    // rotate the poisson disk randomly
    float randomAngle = interleavedGradientNoise(fragCoord) * (2.0 * PI);
    vec2 randomBase = vec2(cos(randomAngle), sin(randomAngle));
    mat2 R = mat2(randomBase.x, randomBase.y, -randomBase.y, randomBase.x);
    return R;
}

float GetPenumbraLs(const bool DIRECTIONAL, const int index, const highp float zLight) {
    float penumbra;
    // This conditional is resolved at compile time
    if (DIRECTIONAL) {
        penumbra = 3.0;
        //penumbra = shadowUniforms.shadows[index].bulbRadiusLs; TODO:
    } else {
        penumbra = 3.0;
        // the penumbra radius depends on the light-space z for spotlights
        //penumbra = shadowUniforms.shadows[index].bulbRadiusLs / zLight; TODO:
    }
    return penumbra;
}

const uint DPCF_SHADOW_TAP_COUNT = 12u;

/*
 * DPCF, PCF with contact hardenning simulation.
 * see "Shadow of Cold War", A scalable approach to shadowing -- by Kevin Myers
 */
float ShadowSample_DPCF(const bool DIRECTIONAL,
const highp sampler2DArray map,
const highp vec4 scissorNormalized,
const uint layer,
const int index,
const highp vec4 shadowPosition,
const float shadowBias) {
    highp vec2 texelSize = vec2(1.0) / vec2(textureSize(map, 0));
    highp vec3 position = shadowPosition.xyz * (1.0 / shadowPosition.w);

    float penumbra = GetPenumbraLs(DIRECTIONAL, index, 0.0);

    mat2 R = GetRandomRotationMatrix(gl_FragCoord.xy);

    float occludedCount = 0.0;
    float z_occSum = 0.0;

    BlockerSearchAndFilter(occludedCount,
    z_occSum,
    map,
    scissorNormalized,
    position.xy,
    position.z,
    layer,
    texelSize * penumbra,
    R,
    shadowBias,
    DPCF_SHADOW_TAP_COUNT);

    // early exit if there is no occluders at all, also avoids a divide-by-zero below.
    if (z_occSum == 0.0) {
        return 1.0;
    }

    float penumbraRatio = GetPenumbraRatio(DIRECTIONAL, index, position.z, z_occSum / occludedCount);
    penumbraRatio = saturate(penumbraRatio);

    float percentageOccluded = occludedCount * (1.0 / float(DPCF_SHADOW_TAP_COUNT));

    percentageOccluded = mix(HardenedKernel(percentageOccluded), percentageOccluded, penumbraRatio);
    return 1.0 - percentageOccluded;
}

int GetCascadeIndex(vec4 cascadeSplits, vec3 viewPosition, int cascadeCount) {
    int cascadeIndex = 0;
    for (int i = 0; i < cascadeCount - 1; ++i) {
        if (viewPosition.z < cascadeSplits[i]) {
            cascadeIndex = i + 1;
        }
    }
    return cascadeIndex;
}

float Shadow(const bool DIRECTIONAL,
const highp sampler2DArray shadowMap,
const int index,
highp vec4 shadowPosition,
vec4 scissorNormalized,
float shadowBias) {
    uint layer = index;
    return ShadowSample_DPCF(DIRECTIONAL,
    shadowMap,
    scissorNormalized,
    layer,
    index,
    shadowPosition,
    shadowBias);
}