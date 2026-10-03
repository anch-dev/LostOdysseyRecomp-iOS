// Appended after the exact original bodies and kAccessorEntries table.
#include "lo_semantics/accessor_family.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <windows.h>

namespace
{

using lo::semantic::gpu::GuestMemory;
using lo::semantic::gpu::ReadField;
using lo::semantic::gpu::WriteField;

static_assert(std::is_trivially_copyable_v<PPCContext>);
constexpr std::size_t kAddressSpace = std::size_t{1} << 32;
constexpr unsigned kCasesPerEntry = 6;
constexpr std::uint32_t kTargets[kCasesPerEntry] = {
    0x00001020u, 0x80001024u, 0xFFFFF028u,
    0x00001038u, 0x8000103Cu, 0xFFFFFFA0u,
};
constexpr std::uint32_t kValues[kCasesPerEntry] = {
    0u, 0xFFFFFFFFu, 0xA5C33C5Au,
    0x12345678u, 0x80000001u, 0x00FF00FEu,
};

class SparseGuestSpace
{
public:
    SparseGuestSpace()
    {
        base_ = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, kAddressSpace, MEM_RESERVE, PAGE_NOACCESS));
        if (!base_) throw std::runtime_error("reserve guest address space");
        for (const std::uint32_t page : {0x00001000u, 0x80001000u, 0xFFFFF000u})
        {
            if (!VirtualAlloc(base_ + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit guest test page");
        }
    }

    ~SparseGuestSpace() { VirtualFree(base_, 0, MEM_RELEASE); }
    SparseGuestSpace(const SparseGuestSpace&) = delete;
    SparseGuestSpace& operator=(const SparseGuestSpace&) = delete;

    [[nodiscard]] std::uint8_t* data() const { return base_; }

private:
    std::uint8_t* base_ = nullptr;
};

std::uint64_t RegisterValue(const PPCContext& context, unsigned index)
{
    switch (index)
    {
    case 3: return context.r3.u64;
    case 4: return context.r4.u64;
    case 5: return context.r5.u64;
    case 6: return context.r6.u64;
    case 13: return context.r13.u64;
    }
    throw std::invalid_argument("unsupported accessor register");
}

void SetRegister(PPCContext& context, unsigned index, std::uint64_t value)
{
    switch (index)
    {
    case 3: context.r3.u64 = value; return;
    case 4: context.r4.u64 = value; return;
    case 5: context.r5.u64 = value; return;
    case 6: context.r6.u64 = value; return;
    case 13: context.r13.u64 = value; return;
    }
    throw std::invalid_argument("unsupported accessor register");
}

bool Test(const AccessorEntry& entry, unsigned case_index, SparseGuestSpace& space)
{
    const std::uint32_t target = kTargets[case_index];
    const std::uint32_t register_base = target -
        static_cast<std::uint32_t>(entry.displacement);
    const std::uint64_t high_bits =
        (case_index & 1u) ? 0xFEDCBA9800000000ull : 0x1234567800000000ull;

    PPCContext original{};
    auto* original_bytes = reinterpret_cast<std::uint8_t*>(&original);
    for (std::size_t i = 0; i < sizeof(original); ++i)
        original_bytes[i] = static_cast<std::uint8_t>(i * 37u + entry.address + case_index);
    SetRegister(original, entry.base_register, high_bits | register_base);
    if (!entry.is_getter)
        SetRegister(original, entry.value_register,
                    0x89ABCDEF00000000ull | kValues[case_index]);
    PPCContext recovered{};
    std::memcpy(&recovered, &original, sizeof(original));

    std::array<std::uint8_t, 64> before{};
    for (std::size_t i = 0; i < before.size(); ++i)
        before[i] = static_cast<std::uint8_t>(i * 19u + entry.address + case_index);
    auto* window = space.data() + target - 32;
    std::memcpy(window, before.data(), before.size());
    entry.original(original, space.data());
    std::array<std::uint8_t, 64> expected_memory{};
    std::memcpy(expected_memory.data(), window, expected_memory.size());

    std::memcpy(window, before.data(), before.size());
    GuestMemory memory(target - 32, {window, before.size()});
    const std::uint32_t base = static_cast<std::uint32_t>(
        RegisterValue(recovered, entry.base_register));
    if (entry.is_getter)
        recovered.r3.u64 = ReadField(memory, base, entry.displacement, entry.width);
    else
        WriteField(memory, base, entry.displacement, entry.width,
                   RegisterValue(recovered, entry.value_register));

    if (std::memcmp(&original, &recovered, sizeof(original)) != 0 ||
        std::memcmp(window, expected_memory.data(), before.size()) != 0)
    {
        std::fprintf(stderr, "FAIL accessor %08X case %u\n", entry.address,
                     case_index);
        return false;
    }
    return true;
}

} // namespace

int main()
{
    SparseGuestSpace space;
    unsigned getters = 0;
    unsigned setters = 0;
    for (const AccessorEntry& entry : kAccessorEntries)
    {
        if (entry.is_getter) ++getters;
        else ++setters;
        for (unsigned i = 0; i < kCasesPerEntry; ++i)
            if (!Test(entry, i, space)) return 1;
    }
    std::printf("PASS accessor-family %zu entries %zu cases %u getters %u setters\n",
                std::size(kAccessorEntries),
                std::size(kAccessorEntries) * kCasesPerEntry, getters, setters);
    return 0;
}
