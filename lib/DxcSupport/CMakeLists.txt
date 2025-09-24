# Copyright (C) Microsoft Corporation. All rights reserved.
# This file is distributed under the University of Illinois Open Source License. See LICENSE.TXT for details.

# Enable exceptions for DxcSupport library (required for dxilconv)
set(LLVM_REQUIRES_EH ON)
set(LLVM_REQUIRES_RTTI ON)

add_llvm_library(LLVMDxcSupport
  dxcapi.use.cpp
  dxcmem.cpp
  FileIOHelper.cpp
  Global.cpp
  HLSLOptions.cpp
  Unicode.cpp
  WinAdapter.cpp
  WinFunctions.cpp
  )

# Enable exceptions for this target
if(APPLE OR UNIX)
  target_compile_options(LLVMDxcSupport PRIVATE -fexceptions -frtti)
endif()

add_dependencies(LLVMDxcSupport TablegenHLSLOptions)
