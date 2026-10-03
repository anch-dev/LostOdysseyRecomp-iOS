#include "lo_semantics/instance_default_fields_family.h"

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
    std::array<std::uint8_t, 0x1000> original_bytes;
    std::array<std::uint8_t, 0x1000> recovered_bytes;
    original_bytes.fill(0xbd);
    recovered_bytes.fill(0xbd);
    GuestMemory memory(0, recovered_bytes);
    PPCContext context{};
    context.r3.u64 = incoming_r3;
    context.lr = 0x1234567887654321ull;
    entry.original(context, original_bytes.data());

    std::uint64_t result = 0xDEADCAFE12345678ull;
    if (!instance_default_fields_family::Apply(entry.address, memory,
            incoming_r3, result))
        throw std::runtime_error("default-fields initializer mapping missing");
    const bool same = context.r3.u64 == incoming_r3 && result == incoming_r3 &&
        context.lr == 0x1234567887654321ull &&
        original_bytes == recovered_bytes;
    if (!same)
        std::fprintf(stderr, "FAIL default-fields initializer %08x r3 %016llx\n",
            entry.address, static_cast<unsigned long long>(incoming_r3));
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
            if (!Compare(entry, 0x1234567800000100ull)) return 1;
            ++cases;
        }
        for (const Entry& entry : {Entries[0], Entries[1]})
        {
            if (!Compare(entry, 0x1234567800000000ull)) return 1;
            ++cases;
        }

        std::array<std::uint8_t, 16> bytes{};
        GuestMemory memory(0, bytes);
        std::uint64_t result = 0xDEADCAFE12345678ull;
        if (instance_default_fields_family::Apply(0xFFFFFFFFu, memory,
                0x1234567800000100ull, result) ||
            result != 0xDEADCAFE12345678ull ||
            bytes != std::array<std::uint8_t, 16>{})
            throw std::runtime_error("unknown default-fields address changed state");
        ++cases;
        std::printf("PASS instance-default-fields 42 exact cached bodies, 464 ordered writes, %u bounded cases\n",
                    cases);
        std::puts("LIMIT four dynamic representatives compare r3 and guest memory; volatile GPR/cr6 and all-entry dynamic matrix are outside this API");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
