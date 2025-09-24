// RUN: %dxbc2dxil %s -o %t.dxil
// RUN: %opt-exe %t.dxil -S -o %t.ll
// CHECK: define void @main
// CHECK: call %dx.types.ResRet.f32 @dx.op.sample.f32
// CHECK: call void @dx.op.storeOutput.f32
// CHECK-NOT: arm_neon
// CHECK-NOT: arm64_neon

// Test vector operations that might use ARM64 NEON instructions
Texture2D tex1 : register(t0);
Texture2D tex2 : register(t1);
SamplerState samp : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    // Vector operations that could benefit from NEON
    float4 color1 = tex1.Sample(samp, input.texCoord);
    float4 color2 = tex2.Sample(samp, input.texCoord);
    
    // Vector arithmetic
    float4 result = color1 * color2;
    result += float4(0.1, 0.2, 0.3, 0.4);
    
    // Vector cross product (should use efficient ARM64 implementation)
    float3 a = color1.xyz;
    float3 b = color2.xyz;
    float3 cross_product = cross(a, b);
    
    // Vector normalize (should use ARM64 rsqrt if available)
    float3 normalized = normalize(cross_product);
    
    result.xyz = normalized;
    return result;
}