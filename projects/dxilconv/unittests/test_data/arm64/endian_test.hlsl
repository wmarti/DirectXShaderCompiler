// RUN: %dxbc2dxil %s -o %t.dxil
// RUN: %opt-exe %t.dxil -S -o %t.ll
// CHECK: define void @main
// CHECK: call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32
// CHECK: call void @dx.op.bufferStore.i32
// CHECK-NOT: bswap

// Test endianness handling in ARM64 (little-endian)
RWByteAddressBuffer dataBuffer : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint baseOffset = id.x * 16;
    
    // Test various data type loads/stores
    // These should maintain little-endian byte order on ARM64
    
    // 32-bit integer
    uint value32 = 0x12345678;
    dataBuffer.Store(baseOffset + 0, value32);
    
    // 16-bit values
    uint value16_1 = 0x1234;
    uint value16_2 = 0x5678;
    dataBuffer.Store(baseOffset + 4, (value16_2 << 16) | value16_1);
    
    // Float value (IEEE 754)
    float floatValue = 1.0f; // Should be 0x3F800000
    dataBuffer.Store(baseOffset + 8, asuint(floatValue));
    
    // Double precision (stored as two 32-bit values)
    double doubleValue = 1.0; // Should be 0x3FF0000000000000
    uint2 doubleBits = asuint(doubleValue);
    dataBuffer.Store2(baseOffset + 12, doubleBits);
    
    // Read back and verify byte order is preserved
    uint readBack = dataBuffer.Load(baseOffset + 0);
    
    // This should be 0x12345678 on little-endian ARM64
    // If byte order is wrong, it would be 0x78563412
    if (readBack != 0x12345678) {
        // Store error marker
        dataBuffer.Store(baseOffset + 16, 0xDEADBEEF);
    }
}