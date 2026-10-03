#include "lo_semantics/instance_integer_sequence_family.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>

namespace
{
constexpr std::uint32_t Object = 0x1000u;
constexpr std::uint32_t Stack = 0x3000u;
constexpr std::size_t Space = 0x10000u;

struct Entry { std::uint32_t address; PPCFunc* original; };
const Entry Entries[] = {
/* ENTRY_TABLE */
};

const Entry* Find(std::uint32_t address)
{
    for (const Entry& entry : Entries)
        if (entry.address == address) return &entry;
    return nullptr;
}

bool Compare(std::uint32_t address, std::uint64_t r3,
    std::uint32_t stack, std::uint32_t flag_offset = 0,
    std::uint32_t flag_value = 0)
{
    const Entry* entry = Find(address);
    if (!entry) return false;
    alignas(32) std::array<std::uint8_t, Space> original_bytes;
    alignas(32) std::array<std::uint8_t, Space> recovered_bytes;
    original_bytes.fill(0xa5);
    recovered_bytes.fill(0xa5);
    lo::semantic::gpu::GuestMemory original_memory(0, original_bytes);
    lo::semantic::gpu::GuestMemory recovered_memory(0, recovered_bytes);
    if (flag_offset != 0)
    {
        original_memory.WriteU32(Object + flag_offset, flag_value);
        recovered_memory.WriteU32(Object + flag_offset, flag_value);
    }
    PPCContext ctx{};
    ctx.r3.u64 = r3;
    ctx.r1.u64 = stack;
    ctx.lr = 0x1234567887654321ull;
    ctx.xer.so = 1;
    entry->original(ctx, original_bytes.data());
    std::uint64_t result = 0xfeedfeedfeedfeedull;
    if (!lo::semantic::gpu::instance_integer_sequence_family::Apply(
            address, recovered_memory, r3, stack, result) ||
        result != ctx.r3.u64 || original_bytes != recovered_bytes)
    {
        std::size_t first = 0;
        while (first < Space && original_bytes[first] == recovered_bytes[first])
            ++first;
        std::fprintf(stderr,
            "FAIL integer-sequence %08x r3 %llx/%llx first-diff %zx\n",
            address, static_cast<unsigned long long>(ctx.r3.u64),
            static_cast<unsigned long long>(result), first);
        return false;
    }
    return true;
}
} // namespace

int main()
{
    constexpr std::uint64_t NonNull = 0xabcdef0000000000ull | Object;
    unsigned comparisons = 0;
    for (const Entry& entry : Entries)
    {
        const std::uint32_t flag_offset =
            entry.address == 0x8245c678u ? 12u :
            entry.address == 0x8240af30u || entry.address == 0x825f4450u ? 92u :
            entry.address == 0x825f2860u ? 96u :
            entry.address == 0x826980d0u ? 76u : 0u;
        const std::uint32_t flag_value =
            entry.address == 0x8245c678u ? 0u :
            entry.address == 0x826980d0u ? 0x80000000u : 0x40000000u;
        if (!Compare(entry.address, NonNull, Stack, flag_offset, flag_value))
            return 1;
        ++comparisons;
    }
    if (!Compare(0x8245c678u, NonNull, Stack, 12u, 0x200u) ||
        !Compare(0x824772f8u, 0xdeadbeef00000000ull, Stack) ||
        !Compare(0x824772f8u, NonNull, Object + 168u) ||
        !Compare(0x826a0d28u, 0xdeadbeef00000000ull, Stack))
        return 1;
    comparisons += 4;

    alignas(32) std::array<std::uint8_t, Space> untouched{};
    lo::semantic::gpu::GuestMemory memory(0, untouched);
    std::uint64_t result = 0x123456789abcdef0ull;
    if (lo::semantic::gpu::instance_integer_sequence_family::Apply(
            0x82000000u, memory, NonNull, Stack, result) ||
        result != 0x123456789abcdef0ull ||
        std::memcmp(untouched.data(),
                    std::array<std::uint8_t, Space>{}.data(), Space) != 0)
        return 1;
    std::printf("PASS instance-integer-sequence %u original PPC comparisons\n",
                comparisons);
    std::puts("LIMIT ordinary bounded memory and full r3; volatile registers and original U64 fault/MMIO width excluded");
    return 0;
}
