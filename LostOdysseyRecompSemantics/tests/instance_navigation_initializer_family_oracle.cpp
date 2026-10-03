#include "lo_semantics/instance_navigation_initializer_family.h"

#include <array>
#include <cstdio>
#include <exception>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
using namespace lo::semantic::gpu::instance_navigation_initializer_family;

struct Entry
{
    GuestAddress address;
    GuestAddress derived_vtable;
    PPCFunc* original;
};

const Entry Entries[] = {
/* ENTRY_TABLE */
};

bool Compare(const Entry& entry, std::uint64_t incoming_r3,
             GuestAddress stack = 0x4000u)
{
    std::array<std::uint8_t, 0x6000> original_bytes;
    std::array<std::uint8_t, 0x6000> recovered_bytes;
    original_bytes.fill(0xbd);
    recovered_bytes.fill(0xbd);
    GuestMemory recovered_memory(0, recovered_bytes);

    PPCContext context{};
    context.r1.u64 = stack;
    context.r3.u64 = incoming_r3;
    context.r30.u64 = 0x1234567800003120ull;
    context.r31.u64 = 0xabcdef0100003340ull;
    context.lr = 0x82700000u;
    const EntryAbi abi{stack, context.lr, context.r30.u64, context.r31.u64};
    entry.original(context, original_bytes.data());

    std::uint64_t result = 0xdeadbeefull;
    if (!Apply(entry.address, recovered_memory, incoming_r3, abi, result))
        throw std::runtime_error("navigation initializer missing");
    const bool same = original_bytes == recovered_bytes &&
                      result == context.r3.u64 &&
                      context.r1.u32 == stack &&
                      context.lr == abi.incoming_lr &&
                      context.r30.u64 == abi.incoming_r30 &&
                      context.r31.u64 == abi.incoming_r31;
    if (!same)
        std::fprintf(stderr, "FAIL navigation %08x r3=%016llx stack=%08x memory=%d result=%d\n",
            entry.address, static_cast<unsigned long long>(incoming_r3), stack,
            original_bytes == recovered_bytes, result == context.r3.u64);
    return same;
}

const Entry& Find(GuestAddress address)
{
    for (const Entry& entry : Entries)
        if (entry.address == address)
            return entry;
    throw std::runtime_error("missing oracle entry");
}
} // namespace

int main()
{
    try
    {
        unsigned cases = 0;
        for (GuestAddress address : {0x82b55028u, 0x82b550e8u, 0x82b56728u})
        {
            const Entry& entry = Find(address);
            if (!Compare(entry, 0x1234567800001000ull))
                return 1;
            ++cases;
        }
        if (!Compare(Find(0x82b55028u), 0x1234567800000000ull) ||
            !Compare(Find(0x82b54518u), 0x1234567800001000ull) ||
            !Compare(Find(0x82b56728u), 0x1234567800003f80ull))
            return 1;
        cases += 3;
        std::array<std::uint8_t, 16> bytes{};
        GuestMemory memory(0, bytes);
        std::uint64_t result = 0x12345678ull;
        if (Apply(0xffffffffu, memory, 0x1000u,
                  EntryAbi{8u, 0, 0, 0}, result) ||
            result != 0x12345678ull || bytes != std::array<std::uint8_t, 16>{})
            return 1;
        ++cases;
        std::printf("PASS instance-navigation 16 statically validated entries %u focused PPC cases\n", cases);
        std::puts("LIMIT temporary GPR/CR state and fault/MMIO width outside API");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
