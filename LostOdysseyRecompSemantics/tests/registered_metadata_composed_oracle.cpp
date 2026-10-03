#include "lo_semantics/registered_metadata_composed.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
namespace composed = lo::semantic::gpu::registered_metadata_composed;
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Source = 0x12000u;
constexpr GuestAddress Destination = 0x13000u;
constexpr GuestAddress Vtable = 0x14000u;
constexpr GuestAddress Method = 0x827abcdcu;

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x20000, MEM_COMMIT,
                                    PAGE_READWRITE))
            throw std::runtime_error("commit metadata guest window");
        std::memset(bytes, 0xbd, 0x20000);
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

struct Event
{
    GuestAddress method;
    std::uint64_t r3;
    std::uint64_t sp;
    std::uint64_t lr;
    bool operator==(const Event&) const = default;
};

struct Services final : composed::VirtualServices
{
    GuestMemory memory;
    std::vector<Event> events;
    explicit Services(std::uint8_t* bytes)
        : memory(0, std::span<std::uint8_t>(bytes, Space)) {}

    std::uint64_t TailCall(GuestAddress method, GuestMemory& target_memory,
        std::uint64_t r3, composed::TailControl& control) override
    {
        if (&target_memory != &memory)
            throw std::runtime_error("wrong virtual callback memory");
        events.push_back({method, r3, control.sp, control.lr});
        if (method != Method || static_cast<GuestAddress>(r3) != Object)
            throw std::runtime_error("wrong virtual callback target/r3");
        target_memory.WriteU32(Object + 16u, 0x47524150u);
        control.sp ^= 0x1000000000000010ull;
        control.lr ^= 0x1122334455667788ull;
        return 0xfedcba9876543210ull;
    }
};

Services* active = nullptr;

bool CompareVirtual(bool alias_vtable)
{
    Window original, recovered;
    Services expected(original.bytes), actual(recovered.bytes);
    const GuestAddress table = alias_vtable ? Object : Vtable;
    for (Services* service : {&expected, &actual})
    {
        service->memory.WriteU32(Object, table);
        service->memory.WriteU32(table + 292u, Method | 3u);
    }
    PPCContext raw{};
    raw.r3.u64 = 0xabcdef0000010000ull;
    raw.r1.u64 = 0x1234567800070000ull;
    raw.lr = 0x9988776655443322ull;
    PPCContext state = raw;
    active = &expected;
    __imp__sub_826D6C10(raw, original.bytes);
    composed::TailControl control{state.r1.u64, state.lr};
    std::uint64_t result = 0;
    if (!composed::Apply(0x826d6c10u, actual.memory, actual,
                         state.r3.u64, control, result))
        throw std::runtime_error("virtual metadata thunk missing");
    state.r3.u64 = result;
    state.r1.u64 = control.sp;
    state.lr = control.lr;
    const bool same = raw.r3.u64 == state.r3.u64 &&
        raw.r1.u64 == state.r1.u64 && raw.lr == state.lr &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, 0x20000) == 0;
    if (!same)
        std::fprintf(stderr, "FAIL metadata virtual alias=%d\n", alias_vtable);
    return same;
}

bool CompareCopy(int mode)
{
    Window original, recovered;
    GuestMemory expected(0, std::span<std::uint8_t>(original.bytes, Space));
    GuestMemory actual(0, std::span<std::uint8_t>(recovered.bytes, Space));
    const std::array<std::uint16_t, 3> input = mode == 1 ?
        std::array<std::uint16_t, 3>{0, 0x2468u, 0x1357u} :
        std::array<std::uint16_t, 3>{0x0041u, 0x4e2du, 0};
    for (GuestMemory* memory : {&expected, &actual})
        for (std::size_t index = 0; index < input.size(); ++index)
            memory->WriteU16(Source + static_cast<GuestAddress>(index * 2),
                             input[index]);
    PPCContext raw{};
    raw.r3.u64 = 0xabcdef0000000000ull |
        (mode == 2 ? Source - 2u : Destination);
    raw.r4.u64 = 0x1234567800012000ull;
    PPCContext state = raw;
    __imp__sub_8230BAC0(raw, original.bytes);
    std::uint64_t source_after = 0;
    state.r3.u64 = composed::CopyUtf16UntilNull(actual, state.r3.u64,
        state.r4.u64, source_after);
    state.r4.u64 = source_after;
    const bool same = raw.r3.u64 == state.r3.u64 &&
        raw.r4.u64 == state.r4.u64 &&
        std::memcmp(original.bytes, recovered.bytes, 0x20000) == 0;
    if (!same)
        std::fprintf(stderr, "FAIL UTF16 copy mode=%d r4 %016llx/%016llx\n",
            mode, static_cast<unsigned long long>(raw.r4.u64),
            static_cast<unsigned long long>(state.r4.u64));
    return same;
}
} // namespace

void MetadataIndirect(PPCContext& context, std::uint8_t*,
                      GuestAddress method)
{
    composed::TailControl control{context.r1.u64, context.lr};
    context.r3.u64 = active->TailCall(method, active->memory,
                                      context.r3.u64, control);
    context.r1.u64 = control.sp;
    context.lr = control.lr;
}

int main()
{
    try
    {
        if (!CompareVirtual(false) || !CompareVirtual(true) ||
            !CompareCopy(0) || !CompareCopy(1) || !CompareCopy(2))
            return 1;
        Window spare;
        Services services(spare.bytes);
        composed::TailControl control{7, 9};
        std::uint64_t result = 11;
        if (composed::Apply(0xffffffffu, services.memory, services,
                            Object, control, result) || control.sp != 7 ||
            control.lr != 9 || result != 11 || !services.events.empty())
            throw std::runtime_error("unknown metadata thunk changed state");
        std::puts("PASS metadata composed 2 exact PPC bodies, 5 bounded comparisons + unknown");
        std::puts("LIMIT virtual target is external; ordinary RAM and selected full r3/r4/SP/LR only");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
