#pragma once

#include "cpu/semantic_accessor.h"
#include "lo_semantics/registered_getter_family.h"

namespace lo::runtime::semantic_registered
{

inline bool Enabled() noexcept
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_SEMANTIC_REGISTERED_RUNTIME");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

// Recover the signed lis/addi pair from its resulting guest address.
inline std::int64_t AdjustedHigh(std::uint32_t address) noexcept
{
    return static_cast<std::int32_t>((address + 0x8000u) & 0xffff0000u);
}

inline std::int32_t LowOffset(std::uint32_t address) noexcept
{
    return static_cast<std::int16_t>(address);
}

struct GuestCalls
{
    PPCContext& ctx;
    std::uint8_t* base;

    using Singleton = lo::semantic::gpu::registered_getter_family::Singleton;

    std::uint32_t Global(const Singleton& entry) const
    {
        // The original reads r31 again after each call; keep that live value.
        return ctx.r31.u32 + static_cast<std::uint32_t>(LowOffset(entry.global));
    }

    void LoadInitial(std::uint32_t object)
    {
        ctx.r3.u64 = object;
        ctx.cr6.compare<std::uint32_t>(object, 0, ctx.xer);
    }

    void LoadResult(std::uint32_t object) { ctx.r3.u64 = object; }

    std::uint64_t Construct(const Singleton& entry)
    {
        ctx.r11.s64 = AdjustedHigh(static_cast<std::uint32_t>(entry.owner));
        ctx.r3.s64 = ctx.r11.s64 + LowOffset(static_cast<std::uint32_t>(entry.owner));
        ctx.lr = entry.address + 44u;
        (PPC_LOOKUP_FUNC(base, entry.constructor))(ctx, base);
        return ctx.r3.u64;
    }

    void Register(const Singleton& entry, std::uint64_t)
    {
        ctx.lr = entry.address + 52u;
        (PPC_LOOKUP_FUNC(base, entry.registration))(ctx, base);
    }
};

// These getters have one fixed 96-byte frame. Apply the original ABI operations
// around the same singleton algorithm used by the standalone semantic library.
// Downstream guest calls retain their full live context and function-table route.
[[nodiscard]] inline bool Apply(PPCContext& ctx, std::uint8_t* base,
    std::uint32_t address)
{
    namespace getters = lo::semantic::gpu::registered_getter_family;
    const auto* entry = getters::Find(address);
    if (entry == nullptr) return false;

    ctx.r12.u64 = ctx.lr;
    PPC_STORE_U32(ctx.r1.u32 - 8u, ctx.r12.u32);
    PPC_STORE_U64(ctx.r1.u32 - 16u, ctx.r31.u64);
    const auto frame = ctx.r1.u64 - 96u;
    PPC_STORE_U32(static_cast<std::uint32_t>(frame), ctx.r1.u32);
    ctx.r1.u64 = frame;
    ctx.r31.s64 = AdjustedHigh(entry->global);

    semantic_accessor::NativeAccessorMemory memory(base);
    GuestCalls calls{ctx, base};
    (void)getters::FetchWith(*entry, memory, calls);

    ctx.r1.u64 += 96u;
    ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 - 8u);
    ctx.lr = ctx.r12.u64;
    ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 - 16u);
    return true;
}

} // namespace lo::runtime::semantic_registered
