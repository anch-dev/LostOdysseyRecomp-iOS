#include "lo_semantics/instance_fill_initializer_family.h"

#include <array>
#include <cstdio>
#include <exception>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
using namespace instance_fill_initializer_family;

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
    std::array<std::uint8_t, 0x2000> original_bytes;
    original_bytes.fill(0xbd);
    auto recovered_bytes = original_bytes;
    GuestMemory recovered_memory(0, recovered_bytes);
    constexpr EntryAbi Abi{0x1234000000001800ull,
        0xabc0000082700000ull, 0x0123456789abcdefull};
    PPCContext context{};
    context.r1.u64 = Abi.caller_sp;
    context.r3.u64 = incoming_r3;
    context.r31.u64 = Abi.incoming_r31;
    context.lr = Abi.incoming_lr;
    entry.original(context, original_bytes.data());
    Result result{};
    if (!Apply(entry.address, recovered_memory, incoming_r3, Abi, result))
        throw std::runtime_error("missing UI fill initializer");
    const bool same = original_bytes == recovered_bytes &&
        result.r3 == context.r3.u64 && result.lr == context.lr &&
        result.r31 == context.r31.u64 && context.r1.u64 == Abi.caller_sp;
    if (!same)
        std::fprintf(stderr, "FAIL UI fill %08x r3 %016llx\n", entry.address,
            static_cast<unsigned long long>(incoming_r3));
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
            if (!Compare(entry, 0x1234567800000100ull))
                return 1;
            ++cases;
        }
        if (!Compare(Entries[0], 0x1234567800000103ull) ||
            !Compare(Entries[0], 0x1234567800000000ull) ||
            !Compare(Entries[2], 0x1234567800000000ull) ||
            !Compare(Entries[2], 0x12345678000017b0ull))
            return 1;
        cases += 4;
        std::array<std::uint8_t, 16> bytes{};
        GuestMemory memory(0, bytes);
        Result result{1u, 2u, 3u};
        if (Apply(0xffffffffu, memory, 0x100u, EntryAbi{}, result) ||
            result.r3 != 1u || result.lr != 2u || result.r31 != 3u ||
            bytes != std::array<std::uint8_t, 16>{})
            throw std::runtime_error("unknown address changed state");
        ++cases;
        std::printf("PASS instance-fill %zu shape representatives %u cases\n",
            std::size(Entries), cases);
        std::puts("LIMIT full r3, restored LR/r31/SP and ordinary memory including frame aliases compared; volatile GPR/CR/CTR, fault and MMIO widths excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
