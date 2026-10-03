#pragma once

#include <ppc_context.h>
#include "lo_semantics/accessor_family.h"

#include <cstdlib>
#include <cstring>

namespace lo::runtime::semantic_accessor
{

inline bool Enabled() noexcept
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_SEMANTIC_ACCESSOR_RUNTIME");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

// Only the reviewed plain-load/store accessor family uses this backend. Each
// access retains the original PPC macro's width, volatility and byte order.
// eieio/MMIO, atomics and multi-instruction memory functions are not included.
class NativeAccessorMemory
{
public:
    explicit NativeAccessorMemory(std::uint8_t* guest_base) : base(guest_base) {}
    std::uint8_t ReadU8(std::uint32_t address) const { return PPC_LOAD_U8(address); }
    std::uint16_t ReadU16(std::uint32_t address) const { return PPC_LOAD_U16(address); }
    std::uint32_t ReadU32(std::uint32_t address) const { return PPC_LOAD_U32(address); }
    void WriteU8(std::uint32_t address, std::uint8_t value) { PPC_STORE_U8(address, value); }
    void WriteU16(std::uint32_t address, std::uint16_t value) { PPC_STORE_U16(address, value); }
    void WriteU32(std::uint32_t address, std::uint32_t value) { PPC_STORE_U32(address, value); }

private:
    std::uint8_t* base;
};

} // namespace lo::runtime::semantic_accessor
