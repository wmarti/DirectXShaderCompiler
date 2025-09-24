// RUN: %dxbc2dxil %s -o %t.dxil
// RUN: %opt-exe %t.dxil -S -o %t.ll
// CHECK: !dx.entryPoints = !{![[ENTRY:[0-9]+]]}
// CHECK: ![[ENTRY]] = !{void ()* @main, !"ps", null, ![[RES:[0-9]+]], null}
// CHECK: ![[RES]] = !{null, null, ![[CBV:[0-9]+]], null}
// CHECK: ![[CBV]] = !{![[CB:[0-9]+]]}
// CHECK: ![[CB]] = !{i32 0, %struct.Constants* undef, !"", i32 0, i32 0, i32 1, i32 64, null}
// CHECK-NOT: align 1
// CHECK-NOT: align 2
// CHECK: align 16

// Test ARM64-specific alignment requirements for constant buffers
struct Constants {
    float4x4 matrix;        // 64 bytes, requires 16-byte alignment
    float4 vector;          // 16 bytes, requires 16-byte alignment  
    float2 padding;         // 8 bytes
    float scalar1;          // 4 bytes
    float scalar2;          // 4 bytes
};

ConstantBuffer<Constants> cb : register(b0);

float4 main() : SV_TARGET {
    // Access all fields to ensure they're not optimized away
    float4 result = mul(cb.vector, cb.matrix);
    result.xy += cb.padding;
    result.z += cb.scalar1;
    result.w += cb.scalar2;
    return result;
}