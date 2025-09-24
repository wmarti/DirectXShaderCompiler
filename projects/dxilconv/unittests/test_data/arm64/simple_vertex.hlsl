// RUN: %dxbc2dxil %s -o %t.dxil
// RUN: %opt-exe %t.dxil -S -o %t.ll
// CHECK: define void @main
// CHECK-NOT: align 1
// CHECK: align 4
// CHECK: ret void

struct VSInput {
    float4 position : POSITION;
    float2 texCoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = input.position;
    output.texCoord = input.texCoord;
    return output;
}