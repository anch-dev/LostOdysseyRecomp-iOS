#include "lo_semantics/instance_flag_initializer_family.h"

#include <array>
#include <cstdio>
#include <exception>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;

struct Entry
{
    GuestAddress address;
    GuestAddress vtable;
    PPCFunc* original;
};
constexpr Entry Entries[] = {
/* ENTRY_TABLE */
};

bool Compare(const Entry& entry, std::uint64_t incoming_r3,
    std::uint32_t initial_flags)
{
    std::array<std::uint8_t, 0x400> original_bytes;
    std::array<std::uint8_t, 0x400> recovered_bytes;
    original_bytes.fill(0xbd);
    recovered_bytes.fill(0xbd);
    GuestMemory original_memory(0, original_bytes);
    GuestMemory recovered_memory(0, recovered_bytes);
    constexpr GuestAddress Object = 0x100u;
    original_memory.WriteU32(Object + 120u, initial_flags);
    recovered_memory.WriteU32(Object + 120u, initial_flags);
    PPCContext context{};
    context.r3.u64 = incoming_r3;
    context.lr = 0x82700000u;
    entry.original(context, original_bytes.data());

    std::uint64_t result = 0xdeadcafe12345678ull;
    if (!instance_flag_initializer_family::Apply(entry.address,
            recovered_memory, incoming_r3, result))
        throw std::runtime_error("missing flag initializer");
    const bool nonnull = static_cast<GuestAddress>(incoming_r3) != 0;
    const bool same = result == context.r3.u64 &&
        context.lr == 0x82700000u &&
        original_bytes == recovered_bytes &&
        (!nonnull ||
         (recovered_memory.ReadU32(Object) == entry.vtable &&
          recovered_memory.ReadU32(Object + 120u) ==
              (initial_flags | 0x80000000u)));
    if (!same)
        std::fprintf(stderr, "FAIL flag initializer %08x r3 %016llx flags %08x\n",
            entry.address, static_cast<unsigned long long>(incoming_r3),
            initial_flags);
    return same;
}
} // namespace

int main()
{
    try
    {
        unsigned cases = 0;
        for (const Entry& entry : Entries)
        {
            if (!Compare(entry, 0x1234567800000100ull, 0x01234567u))
                return 1;
            ++cases;
        }
        if (!Compare(Entries[0], 0x1234567800000100ull, 0x81234567u) ||
            !Compare(Entries[0], 0x1234567800000000ull, 0x01234567u))
            return 1;
        cases += 2;

        std::array<std::uint8_t, 16> bytes{};
        GuestMemory memory(0, bytes);
        std::uint64_t result = 0x1234567800000000ull;
        if (instance_flag_initializer_family::Apply(0xffffffffu, memory,
                0x100u, result) || result != 0x1234567800000000ull ||
            bytes != std::array<std::uint8_t, 16>{})
            throw std::runtime_error("unknown address changed state");
        ++cases;
        std::printf("PASS instance-flag %zu address representatives %u cases\n",
            std::size(Entries), cases);
        std::puts("LIMIT r3 and guest memory compared; temporary r10/r11/cr6 and fault/MMIO write observations are outside the readable API");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
