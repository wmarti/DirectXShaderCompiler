///////////////////////////////////////////////////////////////////////////////
// winapifamily.h - Minimal Windows API family header
// Provides minimal definitions to satisfy DirectX headers
///////////////////////////////////////////////////////////////////////////////

#ifndef DXILCONV_WINAPIFAMILY_H
#define DXILCONV_WINAPIFAMILY_H

// Windows API family partition macros
#define WINAPI_FAMILY_PARTITION(Partition) 1
#define WINAPI_PARTITION_DESKTOP 1
#define WINAPI_PARTITION_APP 1
#define WINAPI_PARTITION_PC_APP 1
#define WINAPI_PARTITION_SYSTEM 1

// Default to desktop partition
#ifndef WINAPI_FAMILY
#define WINAPI_FAMILY 100
#endif

#endif // DXILCONV_WINAPIFAMILY_H