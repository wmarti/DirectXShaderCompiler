///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// AdaptiveMap.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Simplified adaptive map using DenseMap with size hints for optimization. //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXC_SUPPORT_ADAPTIVE_MAP_H__
#define __DXC_SUPPORT_ADAPTIVE_MAP_H__

#include "llvm/ADT/DenseMap.h"

namespace hlsl {

// Adaptive map wrapper around DenseMap with size hints for better initial allocation
// This avoids complex union/switching logic while still optimizing for small datasets
template<typename KeyT, typename ValueT, unsigned InitialSize = 4>
class AdaptiveMap : public llvm::DenseMap<KeyT, ValueT> {
private:
  using BaseMap = llvm::DenseMap<KeyT, ValueT>;
  
public:
  AdaptiveMap() : BaseMap(InitialSize) {
    // Start with a small initial allocation
  }
  
  // Constructor with initial capacity hint
  explicit AdaptiveMap(unsigned initSize) : BaseMap(initSize) {}
  
  // Performance hint: reserve space if we know the size
  void reserveHint(unsigned expectedSize) {
    if (expectedSize > this->size()) {
      this->reserve(expectedSize);
    }
  }
  
  // Check if still in "small" mode (for debugging/statistics)
  bool isSmallMap() const {
    return this->size() <= InitialSize;
  }
};

// Convenience typedefs for common use cases
template<typename KeyT, typename ValueT>
using SmallAdaptiveMap = AdaptiveMap<KeyT, ValueT, 4>;

template<typename KeyT, typename ValueT>
using MediumAdaptiveMap = AdaptiveMap<KeyT, ValueT, 8>;

template<typename KeyT, typename ValueT>
using LargeAdaptiveMap = AdaptiveMap<KeyT, ValueT, 16>;

} // namespace hlsl

#endif // __DXC_SUPPORT_ADAPTIVE_MAP_H__