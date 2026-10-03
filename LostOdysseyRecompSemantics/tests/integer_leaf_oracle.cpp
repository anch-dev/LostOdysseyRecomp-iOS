// Appended after all extracted originals and kIntegerLeafEntries.
#include "lo_semantics/integer_leaf.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace
{

static_assert(std::is_trivially_copyable_v<PPCContext>);
constexpr unsigned kCasesPerEntry = 10;

std::uint64_t Next(std::uint64_t& state)
{
    state += 0x9e3779b97f4a7c15ull;
    std::uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

template <std::size_t N>
void Fill(std::array<std::uint8_t, N>& bytes, std::uint64_t& state)
{
    for (std::size_t offset = 0; offset < N; offset += sizeof(std::uint64_t))
    {
        const auto value = Next(state);
        const auto count = N - offset < sizeof(value) ? N - offset : sizeof(value);
        std::memcpy(bytes.data() + offset, &value, count);
    }
}

bool Test(const IntegerLeafEntry& entry, unsigned case_index)
{
    std::uint64_t seed = 0x71a6810fead2f1c3ull ^
                         (std::uint64_t{entry.address} << 13) ^ case_index;
    std::array<std::uint8_t, sizeof(PPCContext)> raw{};
    Fill(raw, seed);
    PPCContext original{};
    std::memcpy(&original, raw.data(), sizeof(original));

    switch (case_index)
    {
    case 0:
        original.r3.u64 = original.r4.u64 = original.r5.u64 = original.r6.u64 = 0;
        break;
    case 1:
        original.r3.u64 = original.r4.u64 = original.r5.u64 = original.r6.u64 = UINT64_MAX;
        break;
    case 2:
        original.r3.u64 = 0x8000000000000000ull;
        original.r4.u64 = 0x80000000ull;
        original.r5.u64 = 0x7fffffffull;
        original.r6.u64 = 0x2000ull;
        break;
    case 3:
        original.r3.u64 = 0x1234567800000003ull;
        original.r4.u64 = original.r3.u64;
        original.r5.u64 = original.r3.u64;
        break;
    case 4:
        original.r3.u64 = 0x100000002ull;
        original.r4.u64 = 2;
        original.r5.u64 = 2;
        break;
    case 5:
        original.r3.u64 = 0xffffffff00000003ull;
        original.r4.u64 = 0x100000003ull;
        original.r5.u64 = 3;
        break;
    case 6:
        original.r3.u64 = 0x80000000ull;
        original.r4.u64 = 0x7fffffffull;
        original.r5.u64 = 0x80000001ull;
        original.r6.u64 = 0xffffffffffffdfffull;
        break;
    case 7:
        original.r3.u64 = 0xffffffff00000000ull;
        original.r4.u64 = 0x100000000ull;
        original.r5.u64 = 0;
        break;
    case 8:
        original.r3.u64 = 2;
        original.r4.u64 = 0;
        original.r5.u64 = 3;
        break;
    case 9:
        original.r3.u64 = 3;
        original.r4.u64 = 1;
        original.r5.u64 = 2;
        break;
    }
    PPCContext recovered{};
    std::memcpy(&recovered, &original, sizeof(original));

    std::array<std::uint8_t, 64> original_memory{};
    Fill(original_memory, seed);
    auto recovered_memory = original_memory;
    entry.original(original, original_memory.data());

    lo::semantic::integer_leaf::Registers r{
        recovered.r3.u64, recovered.r4.u64, recovered.r5.u64,
        recovered.r6.u64, recovered.r7.u64, recovered.r10.u64, recovered.r11.u64};
    if (!lo::semantic::integer_leaf::Apply(entry.address, r))
    {
        std::fprintf(stderr, "Unmapped %08X\n", entry.address);
        return false;
    }
    recovered.r3.u64 = r.r3;
    recovered.r10.u64 = r.r10;
    recovered.r11.u64 = r.r11;

    if (std::memcmp(&original, &recovered, sizeof(original)) != 0 ||
        original_memory != recovered_memory)
    {
        std::fprintf(stderr, "FAIL %08X case %u r3 %016llX/%016llX r10 %016llX/%016llX r11 %016llX/%016llX\n",
                     entry.address, case_index,
                     static_cast<unsigned long long>(original.r3.u64),
                     static_cast<unsigned long long>(recovered.r3.u64),
                     static_cast<unsigned long long>(original.r10.u64),
                     static_cast<unsigned long long>(recovered.r10.u64),
                     static_cast<unsigned long long>(original.r11.u64),
                     static_cast<unsigned long long>(recovered.r11.u64));
        return false;
    }
    return true;
}

} // namespace

int main()
{
    for (const auto& entry : kIntegerLeafEntries)
        for (unsigned i = 0; i < kCasesPerEntry; ++i)
            if (!Test(entry, i)) return 1;

    lo::semantic::integer_leaf::Registers unknown{0x1234u};
    if (lo::semantic::integer_leaf::Apply(0, unknown) || unknown.r3 != 0x1234u)
        return 1;

    std::printf("PASS integer-leaf %zu entries %zu cases\n",
                std::size(kIntegerLeafEntries),
                std::size(kIntegerLeafEntries) * kCasesPerEntry);
    return 0;
}
