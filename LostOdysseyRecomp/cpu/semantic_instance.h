#pragma once

#include "cpu/semantic_accessor.h"
#include "lo_semantics/instance_vtable_family.h"
#include "lo_semantics/instance_field_initializer_family.h"
#include "lo_semantics/instance_property_initializer_family.h"
#include "lo_semantics/instance_marker_initializer_family.h"
#include "lo_semantics/instance_scalar_initializer_family.h"
#include "lo_semantics/instance_ui_initializer_family.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace lo::runtime::semantic_instance
{

inline bool Enabled() noexcept
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_SEMANTIC_INSTANCE_RUNTIME");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

// Values of r7..r11 after the reviewed lis/li/addi prefix, immediately before
// the first guest write. Bit 0 represents r7 and bit 4 represents r11.
struct AbiSpec
{
    std::uint32_t address;
    std::uint8_t register_mask;
    std::int64_t registers[5];
};

[[nodiscard]] const AbiSpec* FindAbi(std::uint32_t address) noexcept;

[[nodiscard]] inline bool Apply(PPCContext& ctx, std::uint8_t* base,
    std::uint32_t address)
{
    namespace gpu = lo::semantic::gpu;
    const AbiSpec* abi = FindAbi(address);
    if (!abi) return false;
    const auto* vtable = gpu::instance_vtable_family::Find(address);
    const auto* fields = gpu::instance_field_initializer_family::Find(address);
    const auto* property = gpu::instance_property_initializer_family::Find(address);
    const auto* marker = gpu::instance_marker_initializer_family::Find(address);
    const auto* scalar = gpu::instance_scalar_initializer_family::Find(address);
    const auto* ui = gpu::instance_ui_initializer_family::Find(address);
    if (!vtable && !fields && !property && !marker && !scalar && !ui)
        return false;

    // cmplwi cr6,r3,0 runs even on the null path. The comparison uses the
    // low 32 bits and copies XER.SO into CR6.
    ctx.cr6.compare<std::uint32_t>(ctx.r3.u32, 0, ctx.xer);
    if (ctx.cr6.eq) return true;

    if (abi->register_mask & 0x01) ctx.r7.s64 = abi->registers[0];
    if (abi->register_mask & 0x02) ctx.r8.s64 = abi->registers[1];
    if (abi->register_mask & 0x04) ctx.r9.s64 = abi->registers[2];
    if (abi->register_mask & 0x08) ctx.r10.s64 = abi->registers[3];
    if (abi->register_mask & 0x10) ctx.r11.s64 = abi->registers[4];

    semantic_accessor::NativeAccessorMemory memory(base);
    const auto object = ctx.r3.u32;
    if (vtable) gpu::instance_vtable_family::InitializeWith(*vtable, memory, object);
    else if (fields) gpu::instance_field_initializer_family::InitializeWith(*fields, memory, object);
    else if (property) gpu::instance_property_initializer_family::InitializeWith(*property, memory, object);
    else if (marker) gpu::instance_marker_initializer_family::InitializeWith(*marker, memory, object);
    else if (scalar) gpu::instance_scalar_initializer_family::InitializeWith(*scalar, memory, object);
    else gpu::instance_ui_initializer_family::InitializeWith(*ui, memory, object);
    return true;
}

} // namespace lo::runtime::semantic_instance
