// RUN: %dxbc2dxil %s -o %t.dxil
// RUN: %opt-exe %t.dxil -S -o %t.ll
// CHECK: define void @main
// CHECK: call %dx.types.Handle @dx.op.createHandle
// CHECK: call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32
// CHECK: call void @dx.op.bufferStore.f32

// Test large buffer access patterns for ARM64 memory subsystem
RWStructuredBuffer<float4> largeBuffer : register(u0);

cbuffer ComputeConstants : register(b0) {
    uint bufferSize;
    uint threadCount;
    float scale;
    float bias;
};

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint index = id.x;
    
    // Bounds check
    if (index >= bufferSize) return;
    
    // Large stride access pattern to test ARM64 cache behavior
    uint stride = 1024; // 4KB stride
    uint sourceIndex = (index * stride) % bufferSize;
    
    // Load with potential cache miss
    float4 data = largeBuffer[sourceIndex];
    
    // Vector computation
    data = data * scale + bias;
    
    // Write back with different access pattern
    uint destIndex = (index + bufferSize / 2) % bufferSize;
    largeBuffer[destIndex] = data;
    
    // Memory barrier to ensure ordering on ARM64
    AllMemoryBarrier();
}