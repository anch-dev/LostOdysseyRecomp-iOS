#pragma once

#include "lo_semantics/crt_thread_data.h"

namespace lo::semantic::gpu::crt_formatting_support
{

struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
    bool operator==(const Condition&) const = default;
};

// Selected full-width entry state. Native services may mutate this state and
// guest RAM; the wrapper uses the live stack after each service returns.
struct Registers
{
    std::uint64_t sp = 0, lr = 0;
    std::uint64_t r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0, r8 = 0;
    std::uint64_t r9 = 0, r10 = 0, r11 = 0, r12 = 0, r13 = 0, r31 = 0;
    std::uint8_t xer_so = 0;
    Condition cr0{}, cr6{};
    bool operator==(const Registers&) const = default;
};

class NativeServices : public CrtErrorOutputServices
{
public:
    virtual void InitializeUnicodeString(GuestMemory&, Registers&) = 0;
    virtual void UnicodeStringToAnsiString(GuestMemory&, Registers&) = 0;
    virtual void FreeAnsiString(GuestMemory&, Registers&) = 0;
};

// 822A07A0 reads one narrow byte as a code unit; 82B85420 compares tagged
// globals; 82BE4700 converts and outputs a Unicode error message. The latter
// retains its own frame/save ABI but reuses OutputCrtErrorMessage's accepted
// descriptor/result contract, whose generic callee ABI remains outside scope.
// Unknown entries return false without changing state, memory, or services.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& services, Registers& state);

} // namespace lo::semantic::gpu::crt_formatting_support
