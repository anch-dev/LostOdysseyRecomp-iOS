#include "lo_semantics/instance_vtable_family.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;

struct Entry
{
    GuestAddress address;
    GuestAddress vtable;
    GuestAddress field_offset;
    GuestAddress final_field_word;
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
    if (!instance_vtable_family::Apply(entry.address, memory, incoming_r3, result))
        throw std::runtime_error("missing instance vtable entry");
    const bool same = result == context.r3.u64 &&
        context.lr == 0x82700000u &&
        original_bytes == recovered_bytes &&
        (static_cast<GuestAddress>(incoming_r3) == 0 ||
         (memory.ReadU32(static_cast<GuestAddress>(incoming_r3)) == entry.vtable &&
          (entry.field_offset == 0 ||
           memory.ReadU32(static_cast<GuestAddress>(incoming_r3) +
               entry.field_offset) == entry.final_field_word)));
    if (!same)
        std::fprintf(stderr, "FAIL instance vtable %08x r3 %016llx\n",
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
        const std::uint64_t sentinel = 0x1234567800000000ull;
        std::uint64_t result = sentinel;
        if (instance_vtable_family::Apply(0xffffffffu, memory, 0x100u, result) ||
            result != sentinel || bytes != std::array<std::uint8_t, 16>{})
            throw std::runtime_error("unknown address changed state");
        ++cases;
        std::printf("PASS instance-initializer %zu representative entries %u cases\n",
            std::size(Entries), cases);
        std::puts("LIMIT compares r3 and guest memory; volatile r11/cr6 are outside the readable API");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
