// Appended after exact original generated bodies and kFieldBitEntries.
#include "lo_semantics/field_bits.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <windows.h>

namespace
{

using lo::semantic::field_bits::Registers;
using lo::semantic::gpu::GuestMemory;

static_assert(std::is_trivially_copyable_v<PPCContext>);
constexpr std::size_t kAddressSpace = std::size_t{1} << 32;
constexpr unsigned kCasesPerEntry = 8;
constexpr std::uint32_t kFieldValues[kCasesPerEntry] = {
    0u, 0xffffffffu, 0x80000000u, 0x7fffffffu,
    0x55555555u, 0xaaaaaaaau, 0x12345678u, 0x87654321u,
};

class SparseGuestSpace
{
public:
    SparseGuestSpace()
    {
        base_ = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, kAddressSpace, MEM_RESERVE, PAGE_NOACCESS));
        if (!base_) throw std::runtime_error("reserve guest address space");
        for (std::uint32_t page : {0x00002000u, 0x00003000u, 0x00004000u,
                                   0xfffff000u})
            if (!VirtualAlloc(base_ + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit guest test page");
    }

    ~SparseGuestSpace() { VirtualFree(base_, 0, MEM_RELEASE); }
    SparseGuestSpace(const SparseGuestSpace&) = delete;
    SparseGuestSpace& operator=(const SparseGuestSpace&) = delete;

    [[nodiscard]] std::uint8_t* data() const { return base_; }

private:
    std::uint8_t* base_ = nullptr;
};

void FillContext(PPCContext& context, std::uint32_t address, unsigned case_index)
{
    auto* bytes = reinterpret_cast<std::uint8_t*>(&context);
    for (std::size_t i = 0; i < sizeof(context); ++i)
        bytes[i] = static_cast<std::uint8_t>(i * 37u + address + case_index * 19u);
}

bool Test(const FieldBitEntry& entry, unsigned case_index, SparseGuestSpace& space)
{
    const std::array<std::uint32_t, 3> targets = {
        case_index == 7 ? 0xffffffe0u : 0x00002100u + case_index * 64u,
        0x00003100u + case_index * 64u,
        0x00004100u + case_index * 64u,
    };
    std::array<std::array<std::uint8_t, 32>, 3> before{};
    std::array<std::array<std::uint8_t, 32>, 3> expected{};
    for (unsigned node = 0; node < targets.size(); ++node)
    {
        for (unsigned byte = 0; byte < before[node].size(); ++byte)
            before[node][byte] = static_cast<std::uint8_t>(
                entry.address + case_index * 31u + node * 47u + byte * 13u);
        std::memcpy(space.data() + targets[node] - 8, before[node].data(),
                    before[node].size());
    }

    GuestMemory memory(0, {space.data(), kAddressSpace});
    if (entry.family == FieldBitFamily::ReadPointerChainFieldBits)
    {
        for (unsigned node = 0; node + 1 < entry.load_count; ++node)
            memory.WriteU32(targets[node], targets[node + 1] -
                            static_cast<std::uint32_t>(entry.offsets[node + 1]));
    }
    const unsigned field_node = entry.load_count - 1;
    const auto value = entry.family == FieldBitFamily::InsertFieldBits ?
        kFieldValues[(case_index + 3) % kCasesPerEntry] : kFieldValues[case_index];
    if (entry.width == 8) memory.WriteU8(targets[field_node], static_cast<std::uint8_t>(value));
    else if (entry.width == 16) memory.WriteU16(targets[field_node], static_cast<std::uint16_t>(value));
    else memory.WriteU32(targets[field_node], value);
    for (unsigned node = 0; node < targets.size(); ++node)
        std::memcpy(before[node].data(), space.data() + targets[node] - 8,
                    before[node].size());

    PPCContext original{};
    FillContext(original, entry.address, case_index);
    const std::uint64_t high_bits = case_index & 1u ?
        0xfedcba9800000000ull : 0x1234567800000000ull;
    const std::uint32_t base = targets[0] -
        static_cast<std::uint32_t>(entry.offsets[0]);
    if (entry.base_r4)
        original.r4.u64 = high_bits | base;
    else
        original.r3.u64 = high_bits | base;
    if (entry.family == FieldBitFamily::InsertFieldBits)
        original.r4.u64 = (high_bits ^ 0x5555555500000000ull) |
                          kFieldValues[case_index];
    PPCContext recovered{};
    std::memcpy(&recovered, &original, sizeof(original));

    entry.original(original, space.data());
    for (unsigned node = 0; node < targets.size(); ++node)
        std::memcpy(expected[node].data(), space.data() + targets[node] - 8,
                    expected[node].size());
    for (unsigned node = 0; node < targets.size(); ++node)
        std::memcpy(space.data() + targets[node] - 8, before[node].data(),
                    before[node].size());

    Registers registers{recovered.r3.u64, recovered.r4.u64, recovered.r11.u64};
    if (!lo::semantic::field_bits::Apply(entry.address, registers, memory))
    {
        std::fprintf(stderr, "Unmapped field bits %08X\n", entry.address);
        return false;
    }
    recovered.r3.u64 = registers.r3;
    recovered.r4.u64 = registers.r4;
    recovered.r11.u64 = registers.r11;

    if (std::memcmp(&original, &recovered, sizeof(original)) != 0)
    {
        std::fprintf(stderr, "FAIL context %08X case %u r3 %016llX/%016llX r11 %016llX/%016llX\n",
            entry.address, case_index,
            static_cast<unsigned long long>(original.r3.u64),
            static_cast<unsigned long long>(recovered.r3.u64),
            static_cast<unsigned long long>(original.r11.u64),
            static_cast<unsigned long long>(recovered.r11.u64));
        return false;
    }
    for (unsigned node = 0; node < targets.size(); ++node)
        if (std::memcmp(expected[node].data(), space.data() + targets[node] - 8,
                        expected[node].size()) != 0)
        {
            std::fprintf(stderr, "FAIL memory %08X case %u node %u\n",
                         entry.address, case_index, node);
            return false;
        }
    return true;
}

} // namespace

int main()
{
    SparseGuestSpace space;
    for (const FieldBitEntry& entry : kFieldBitEntries)
        for (unsigned i = 0; i < kCasesPerEntry; ++i)
            if (!Test(entry, i, space)) return 1;

    std::array<std::uint8_t, 4> bytes{};
    GuestMemory memory(0, bytes);
    Registers unknown{0x1234u, 0x5678u, 0x9abcu};
    if (lo::semantic::field_bits::Apply(0, unknown, memory) ||
        unknown.r3 != 0x1234u || unknown.r4 != 0x5678u ||
        unknown.r11 != 0x9abcu)
        return 1;

    std::printf("PASS field-bits %zu entries %zu cases\n",
                std::size(kFieldBitEntries),
                std::size(kFieldBitEntries) * kCasesPerEntry);
    return 0;
}
