#pragma once

namespace DMZ {
// Forward declaration
struct ResolvedType;
class CodegenUtils {
   public:
    static int ptrBitSize();
    static int typeBitSize(const ResolvedType &type);
    static int target_simd_size();
};
}  // namespace DMZ