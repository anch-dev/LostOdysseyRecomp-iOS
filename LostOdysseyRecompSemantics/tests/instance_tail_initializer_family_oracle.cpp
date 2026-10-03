#include "lo_semantics/instance_tail_initializer_family.h"
#include "lo_semantics/memory_fill.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;

constexpr GuestAddress kObject = 0x1000u;
constexpr GuestAddress kStack = 0x3000u;
constexpr std::size_t kBytes = 0x5000u;

struct NoResize final : ArrayResizeServices
{
    unsigned calls = 0;
    void InitializeManager() override { ++calls; }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    {
        ++calls;
        return 0;
    }
};

struct Entry
{
    GuestAddress address;
    PPCFunc* original;
    bool staged;
};

constexpr Entry kEntries[] = {
/* ENTRY_TABLE */
};

NoResize original_resize;

bool Compare(const Entry& entry, std::uint64_t incoming_r3,
    GuestAddress stack, bool compare_staged_stack)
{
    alignas(32) std::array<std::uint8_t, kBytes> original_bytes;
    alignas(32) std::array<std::uint8_t, kBytes> recovered_bytes;
    original_bytes.fill(0xbd);
    recovered_bytes = original_bytes;
    GuestMemory recovered(0, recovered_bytes);
    original_resize.calls = 0;
    NoResize recovered_resize;

    PPCContext context{};
    context.r3.u64 = incoming_r3;
    context.r1.u64 = stack;
    context.lr = 0x12345678u;
    context.r30.u64 = 0xabcdef0123456789ull;
    context.r31.u64 = 0x9876543212345678ull;
    entry.original(context, original_bytes.data());

    std::uint64_t result = 0xdeadbeefcafebabeull;
    if (!instance_tail_initializer_family::Apply(entry.address, recovered,
            recovered_resize, incoming_r3, stack, result))
        throw std::runtime_error("tail initializer mapping missing");
    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    bool same = result == context.r3.u64 &&
        original_resize.calls == recovered_resize.calls;
    if (compare_staged_stack || object == 0)
        same = same && original_bytes == recovered_bytes;
    else
    {
        const auto first = original_bytes.begin() + object;
        const auto last = first + 0x400;
        same = same && std::equal(first, last, recovered_bytes.begin() + object);
    }
    if (!same)
        std::fprintf(stderr, "FAIL tail %08x r3 %016llx sp %08x staged %d\n",
            entry.address, static_cast<unsigned long long>(incoming_r3), stack,
            compare_staged_stack);
    return same;
}
} // namespace

PPC_FUNC(sub_82B7BC40)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, kBytes));
    ctx.r3.u64 = FillGuestMemory(memory, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
}

PPC_FUNC(sub_8229F678)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, kBytes));
    ResizeArray(memory, original_resize, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
}

PPC_FUNC(sub_82419178) { __imp__sub_82419178(ctx, base); }
PPC_FUNC(sub_8255DBD0) { __imp__sub_8255DBD0(ctx, base); }
PPC_FUNC(sub_825A7F18) { __imp__sub_825A7F18(ctx, base); }
PPC_FUNC(sub_825FC498) { __imp__sub_825FC498(ctx, base); }
PPC_FUNC(sub_826B2850) { __imp__sub_826B2850(ctx, base); }

int main()
{
    try
    {
        unsigned comparisons = 0;
        for (const Entry& entry : kEntries)
        {
            if (!Compare(entry, 0x1234567800001000ull, kStack, entry.staged))
                return 1;
            ++comparisons;
        }
        // Low-word-null must bypass the target while retaining the full r3.
        if (!Compare(kEntries[1], 0x1234567800000000ull, kStack, true))
            return 1;
        ++comparisons;
        // Stack words are live: object writes can alias previously staged data.
        for (const Entry& entry : kEntries)
            if (entry.staged)
            {
                if (!Compare(entry, 0x1234567800001f64ull, 0x2000u, true))
                    return 1;
                ++comparisons;
            }
        std::array<std::uint8_t, 16> untouched{};
        GuestMemory memory(0, untouched);
        NoResize services;
        std::uint64_t result = 0xdeadbeefcafebabeull;
        if (instance_tail_initializer_family::Apply(0xffffffffu, memory,
                services, 0x1234567800001000ull, kStack, result) ||
            result != 0xdeadbeefcafebabeull ||
            untouched != std::array<std::uint8_t, 16>{})
            throw std::runtime_error("unknown tail entry changed state");
        ++comparisons;
        std::printf("PASS instance-tail 5 parents + 4 new callees, %u bounded comparisons\n",
                    comparisons);
        std::puts("LIMIT generic PPC volatile state and helper ABI spills outside bounded API");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
