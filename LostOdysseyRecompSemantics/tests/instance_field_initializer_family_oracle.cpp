#include "lo_semantics/instance_field_initializer_family.h"

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
    GuestAddress field_offset;
    GuestAddress vtable;
    GuestAddress first_final;
    GuestAddress second_final;
    PPCFunc* original;
};
constexpr Entry Entries[] = {
/* ENTRY_TABLE */
};

bool Compare(const Entry& entry, std::uint64_t incoming_r3)
{
    std::array<std::uint8_t, 0x400> original_bytes;
    std::array<std::uint8_t, 0x400> recovered_bytes;
    original_bytes.fill(0xbd);
    recovered_bytes.fill(0xbd);
    GuestMemory memory(0, recovered_bytes);
    PPCContext context{};
    context.r3.u64 = incoming_r3;
    context.lr = 0x82700000u;
    entry.original(context, original_bytes.data());

    std::uint64_t result = 0xdeadcafe12345678ull;
    if (!instance_field_initializer_family::Apply(entry.address, memory,
            incoming_r3, result))
        throw std::runtime_error("missing instance field initializer");
    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    const bool same = result == context.r3.u64 &&
        context.lr == 0x82700000u &&
        original_bytes == recovered_bytes &&
        (object == 0 ||
         (memory.ReadU32(object) == entry.vtable &&
          memory.ReadU32(object + entry.field_offset) == entry.first_final &&
          memory.ReadU32(object + entry.field_offset + 4u) == entry.second_final));
    if (!same)
        std::fprintf(stderr, "FAIL instance-field %08x r3 %016llx\n",
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
            for (std::uint64_t incoming : {0x1234567800000100ull,
                                           0x1234567800000000ull})
            {
                if (!Compare(entry, incoming))
                    return 1;
                ++cases;
            }
        }
        std::array<std::uint8_t, 16> bytes{};
        GuestMemory memory(0, bytes);
        std::uint64_t result = 0x1234567800000000ull;
        if (instance_field_initializer_family::Apply(0xffffffffu, memory,
                0x100u, result) || result != 0x1234567800000000ull ||
            bytes != std::array<std::uint8_t, 16>{})
            throw std::runtime_error("unknown address changed state");
        ++cases;
        std::printf("PASS instance-field %zu representative entries %u cases\n",
            std::size(Entries), cases);
        std::puts("LIMIT compares r3 and guest memory; volatile GPR/cr6 are outside the readable API");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
