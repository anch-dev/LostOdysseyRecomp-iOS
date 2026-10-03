// This source is appended to one generated translation unit containing every
// separately extracted original PPC body and the kLeafEntries table.
#include "lo_semantics/leaf_family.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace
{

static_assert(std::is_trivially_copyable_v<PPCContext>);
constexpr std::size_t kMappedBytes = 4096;
constexpr unsigned kCasesPerEntry = 5;

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
    for (std::size_t i = 0; i < N; i += sizeof(std::uint64_t))
    {
        const std::uint64_t value = Next(state);
        const auto count = (N - i < sizeof(value)) ? N - i : sizeof(value);
        std::memcpy(bytes.data() + i, &value, count);
    }
}

bool Test(const LeafEntry& entry, unsigned case_index)
{
    std::uint64_t state = 0x83e7032a5bb51d4full ^
                          (std::uint64_t{entry.address} << 17) ^ case_index;
    std::array<std::uint8_t, sizeof(PPCContext)> context_bytes{};
    Fill(context_bytes, state);
    PPCContext original{};
    PPCContext recovered{};
    std::memcpy(&original, context_bytes.data(), sizeof(original));
    if (case_index == 0) original.r3.u64 = 0;
    if (case_index == 1) original.r3.u64 = UINT64_MAX;
    if (case_index == 2) original.r3.u64 = 0x8000000000000000ull;
    std::memcpy(&recovered, &original, sizeof(original));

    alignas(64) std::array<std::uint8_t, kMappedBytes> original_memory{};
    alignas(64) std::array<std::uint8_t, kMappedBytes> recovered_memory{};
    Fill(original_memory, state);
    std::memcpy(recovered_memory.data(), original_memory.data(), kMappedBytes);

    entry.original(original, original_memory.data());
    if (entry.mode == LeafMode::PreserveR3)
        recovered.r3.u64 = lo::semantic::leaf::PreserveR3(recovered.r3.u64);
    else
        recovered.r3.u64 = lo::semantic::leaf::ReturnOne();

    if (std::memcmp(&original, &recovered, sizeof(original)) != 0 ||
        std::memcmp(original_memory.data(), recovered_memory.data(),
                    kMappedBytes) != 0)
    {
        std::fprintf(stderr, "FAIL %08X case %u mode %u\n", entry.address,
                     case_index, static_cast<unsigned>(entry.mode));
        return false;
    }
    return true;
}

} // namespace

int main()
{
    unsigned cases = 0;
    unsigned preserve = 0;
    unsigned one = 0;
    for (const LeafEntry& entry : kLeafEntries)
    {
        if (entry.mode == LeafMode::PreserveR3)
            ++preserve;
        else
            ++one;
        for (unsigned i = 0; i < kCasesPerEntry; ++i)
        {
            if (!Test(entry, i)) return 1;
            ++cases;
        }
    }
    std::printf("PASS leaf-family %zu entries %u cases %u preserve %u one\n",
                std::size(kLeafEntries), cases, preserve, one);
    return 0;
}
