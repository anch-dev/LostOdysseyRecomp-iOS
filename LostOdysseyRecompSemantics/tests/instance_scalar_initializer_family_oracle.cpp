#include "lo_semantics/instance_scalar_initializer_family.h"

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
    PPCFunc* original;
};
constexpr Entry Entries[] = {
/* ENTRY_TABLE */
};

bool Compare(const Entry& entry, std::uint64_t incoming_r3)
{
    std::array<std::uint8_t, 0x600> original_bytes;
    std::array<std::uint8_t, 0x600> recovered_bytes;
    original_bytes.fill(0xbd);
    recovered_bytes.fill(0xbd);
    GuestMemory memory(0, recovered_bytes);
    PPCContext context{};
    context.r3.u64 = incoming_r3;
    context.lr = 0x1234567887654321ull;
    entry.original(context, original_bytes.data());

    std::uint64_t result = 0xDEADCAFE12345678ull;
    if (!instance_scalar_initializer_family::Apply(entry.address, memory,
            incoming_r3, result))
        throw std::runtime_error("scalar initializer mapping missing");
    const bool same = context.r3.u64 == incoming_r3 && result == incoming_r3 &&
        context.lr == 0x1234567887654321ull &&
        original_bytes == recovered_bytes;
    if (!same)
        std::fprintf(stderr, "FAIL scalar initializer %08x r3 %016llx\n",
            entry.address, static_cast<unsigned long long>(incoming_r3));
    return same;
}
} // namespace

int main()
{
    try
    {
        unsigned comparisons = 0;
        for (const Entry& entry : Entries)
            for (std::uint64_t incoming : {0x1234567800000100ull,
                                           0x1234567800000000ull})
            {
                if (!Compare(entry, incoming)) return 1;
                ++comparisons;
            }

        std::array<std::uint8_t, 16> bytes{};
        GuestMemory memory(0, bytes);
        std::uint64_t result = 0xDEADCAFE12345678ull;
        if (instance_scalar_initializer_family::Apply(0xFFFFFFFFu, memory,
                0x1234567800000100ull, result) ||
            result != 0xDEADCAFE12345678ull ||
            bytes != std::array<std::uint8_t, 16>{})
            throw std::runtime_error("unknown scalar address changed state");
        ++comparisons;
        std::printf("PASS instance-scalar 19 exact cached bodies, %zu template representatives, %u bounded cases\n",
                    std::size(Entries), comparisons);
        std::puts("LIMIT dynamic comparison covers full r3 and bounded guest memory; volatile GPR/cr6 and all-entry dynamic matrix are outside this API");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
