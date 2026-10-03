#include "lo_semantics/instance_copy_initializer_family.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
using namespace instance_copy_initializer_family;
constexpr GuestAddress Base = 0x83238000u;
constexpr std::size_t Bytes = 0x6000u;
constexpr GuestAddress Source = 0x8323a520u;
constexpr GuestAddress Object = Base + 0x800u;
constexpr GuestAddress Sp = Base + 0x5000u;
constexpr EntryAbi Abi{0x1234567800000000ull | Sp,
    0xabc0000082700000ull, 0x0123456789abcdefull, 0xfedcba9876543210ull};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(VirtualAlloc(nullptr,
        std::size_t{1} << 32, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes + Base, Bytes,
            MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve/commit copy initializer window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

bool Compare(Window& original, std::uint64_t incoming_r3)
{
    std::array<std::uint8_t, Bytes> recovered;
    for (std::size_t i = 0; i < Bytes; ++i)
        recovered[i] = static_cast<std::uint8_t>(i * 37u + 13u);
    std::memcpy(original.bytes + Base, recovered.data(), Bytes);
    GuestMemory memory(Base, recovered);
    PPCContext context{};
    context.r1.u64 = Abi.caller_sp;
    context.r3.u64 = incoming_r3;
    context.r30.u64 = Abi.incoming_r30;
    context.r31.u64 = Abi.incoming_r31;
    context.lr = Abi.incoming_lr;
    __imp__sub_8268CC28(context, original.bytes);
    Result result{};
    if (!Apply(0x8268cc28u, memory, incoming_r3, Abi, result))
        throw std::runtime_error("missing copy initializer");
    const bool same = std::memcmp(original.bytes + Base, recovered.data(), Bytes) == 0 &&
        result.r3 == context.r3.u64 && result.lr == context.lr &&
        result.r30 == context.r30.u64 && result.r31 == context.r31.u64 &&
        context.r1.u64 == Abi.caller_sp;
    if (!same)
        std::fprintf(stderr, "FAIL copy initializer %016llx return %016llx/%016llx\n",
            static_cast<unsigned long long>(incoming_r3),
            static_cast<unsigned long long>(result.r3),
            static_cast<unsigned long long>(context.r3.u64));
    return same;
}
} // namespace

int main()
{
    try
    {
        Window original;
        for (GuestAddress object : {Object, Object + 3u, 0u,
            Source - 128u, Sp - 328u, Sp - 240u})
            if (!Compare(original, 0xabcdef0000000000ull | object))
                return 1;
        std::array<std::uint8_t, 16> bytes{};
        GuestMemory memory(0, bytes);
        Result result{1u, 2u, 3u, 4u};
        if (Apply(0xffffffffu, memory, Object, EntryAbi{}, result) ||
            result.r3 != 1u || result.lr != 2u || result.r30 != 3u ||
            result.r31 != 4u || bytes != std::array<std::uint8_t, 16>{})
            throw std::runtime_error("unknown copy initializer changed state");
        std::puts("PASS instance-copy 6 original-PPC comparisons plus 1 unknown-entry check");
        std::puts("LIMIT full r3, restored LR/r30/r31/SP and ordinary memory; cached actual copy body used, synthetic source bytes; volatile GPR/CR/CTR and fault/MMIO widths excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
