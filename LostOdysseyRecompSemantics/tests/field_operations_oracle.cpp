#include "lo_semantics/field_operations.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;

constexpr std::size_t kSpace = std::size_t{1} << 32;
constexpr std::size_t kLowSize = 0x100000;
constexpr std::uint32_t kGlobalBase = 0x82000000u;
constexpr std::size_t kGlobalSize = 0x1400000;
constexpr std::size_t kGlobalWindow = 128;
constexpr unsigned kCases = 4;

struct Space
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, kSpace, MEM_RESERVE, PAGE_NOACCESS));
    Space()
    {
        if (!bytes || !VirtualAlloc(bytes, kLowSize, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + kGlobalBase, kGlobalSize, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve field-operation guest space");
    }
    ~Space() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

FieldOperationRegisters CopyRegisters(const PPCContext& context)
{
    return {context.r3.u64, context.r4.u64, context.r5.u64, context.r6.u64,
            context.r7.u64, context.r8.u64, context.r9.u64, context.r10.u64,
            context.r11.u64};
}

void CopyRegisters(PPCContext& context, const FieldOperationRegisters& registers)
{
    context.r3.u64 = registers.r3;
    context.r4.u64 = registers.r4;
    context.r5.u64 = registers.r5;
    context.r6.u64 = registers.r6;
    context.r7.u64 = registers.r7;
    context.r8.u64 = registers.r8;
    context.r9.u64 = registers.r9;
    context.r10.u64 = registers.r10;
    context.r11.u64 = registers.r11;
}

void Set(PPCContext& context, FieldOperationRegister reg, std::uint64_t value)
{
    switch (reg)
    {
    case FieldOperationRegister::R3: context.r3.u64 = value; return;
    case FieldOperationRegister::R4: context.r4.u64 = value; return;
    case FieldOperationRegister::R5: context.r5.u64 = value; return;
    case FieldOperationRegister::R6: context.r6.u64 = value; return;
    case FieldOperationRegister::R7: context.r7.u64 = value; return;
    case FieldOperationRegister::R8: context.r8.u64 = value; return;
    case FieldOperationRegister::R9: context.r9.u64 = value; return;
    case FieldOperationRegister::R10: context.r10.u64 = value; return;
    case FieldOperationRegister::R11: context.r11.u64 = value; return;
    }
}

void Write(GuestMemory& memory, std::uint32_t address, std::uint8_t width,
           std::uint32_t value)
{
    switch (width)
    {
    case 8: memory.WriteU8(address, static_cast<std::uint8_t>(value)); return;
    case 16: memory.WriteU16(address, static_cast<std::uint16_t>(value)); return;
    case 32: memory.WriteU32(address, value); return;
    }
    throw std::runtime_error("unexpected test field width");
}

void Prepare(const Entry& entry, unsigned scenario, PPCContext& context,
             GuestMemory& memory)
{
    const std::uint64_t upper = (scenario & 1) ? 0xdeadbeef00000000ull :
                                                 0x1234567800000000ull;
    switch (entry.family)
    {
    case Family::Copy:
    case Family::CopyReturn:
    {
        constexpr std::uint32_t bases[4][5] = {
            {0x10000, 0x20000, 0x30000, 0x40000, 0x50000},
            {0x10000, 0x10000, 0x10000, 0x10000, 0x10000},
            {0x10004, 0x10000, 0x10008, 0x1000c, 0x10010},
            {0x10000, 0x10004, 0x10008, 0x1000c, 0x10010},
        };
        context.r3.u64 = upper | bases[scenario][0];
        context.r4.u64 = upper | bases[scenario][1];
        context.r5.u64 = upper | bases[scenario][2];
        context.r6.u64 = upper | bases[scenario][3];
        context.r7.u64 = upper | bases[scenario][4];
        break;
    }
    case Family::Global:
        Write(memory, entry.first_global_address, entry.loads[0].width,
              0x8192a3b4u ^ (scenario * 0x10101u));
        break;
    case Family::GlobalChain:
    {
        std::uint32_t current = 0;
        for (std::size_t i = 0; i < entry.load_count; ++i)
        {
            const std::uint32_t address = i == 0 ? entry.first_global_address :
                current + static_cast<std::uint32_t>(entry.loads[i].displacement);
            if (i + 1 == entry.load_count)
                memory.WriteU32(address, 0x8192a3b4u ^ (scenario * 0x10101u));
            else
            {
                current = 0x60000u + static_cast<std::uint32_t>(i) * 0x1000u +
                          scenario * 0x100u;
                memory.WriteU32(address, current);
            }
        }
        break;
    }
    case Family::Equals:
    case Family::NotEquals:
    {
        const std::uint32_t base = 0x70000u + scenario * 0x1000u;
        Set(context, entry.base, upper | base);
        const std::uint32_t values[] = {entry.comparison, entry.comparison + 1u,
                                        0xffffffffu, 0x80000000u};
        Write(memory, base + static_cast<std::uint32_t>(entry.displacement),
              entry.width, values[scenario]);
        break;
    }
    }
}

bool Test(const Entry& entry, unsigned scenario, Space& original_space,
          Space& recovered_space)
{
    for (std::size_t i = 0; i < kLowSize; ++i)
        original_space.bytes[i] = static_cast<std::uint8_t>(i * 19u + scenario + entry.address);
    const std::uint32_t global_window = entry.first_global_address & ~std::uint32_t{63};
    if (entry.family == Family::Global || entry.family == Family::GlobalChain)
        for (std::size_t i = 0; i < kGlobalWindow; ++i)
            original_space.bytes[global_window + i] =
                static_cast<std::uint8_t>(i * 37u + scenario + entry.address);
    PPCContext original{};
    auto* context_bytes = reinterpret_cast<std::uint8_t*>(&original);
    for (std::size_t i = 0; i < sizeof(original); ++i)
        context_bytes[i] = static_cast<std::uint8_t>(i * 13u + scenario + entry.address);
    GuestMemory original_memory(0, {original_space.bytes, kSpace});
    Prepare(entry, scenario, original, original_memory);
    PPCContext recovered = original;
    std::memcpy(recovered_space.bytes, original_space.bytes, kLowSize);
    if (entry.family == Family::Global || entry.family == Family::GlobalChain)
        std::memcpy(recovered_space.bytes + global_window,
                    original_space.bytes + global_window, kGlobalWindow);

    entry.original(original, original_space.bytes);
    GuestMemory memory(0, {recovered_space.bytes, kSpace});
    auto registers = CopyRegisters(recovered);
    switch (entry.family)
    {
    case Family::Copy:
        CopyFixedFields(memory, registers, {entry.steps, entry.step_count});
        break;
    case Family::CopyReturn:
        CopyFixedFieldsAndReturnConstant(memory, registers, {entry.steps, entry.step_count});
        break;
    case Family::Global:
        ReadGlobalField(memory, registers, entry.global_base,
                        entry.has_adjustment ? std::optional<std::int32_t>{entry.adjustment} :
                                               std::nullopt,
                        {entry.loads, entry.load_count});
        break;
    case Family::GlobalChain:
        ReadGlobalPointerChainField(memory, registers, entry.global_base,
                                    {entry.loads, entry.load_count});
        break;
    case Family::Equals:
        FieldEqualsConstant(memory, registers, entry.base, entry.displacement,
                            entry.width, entry.comparison);
        break;
    case Family::NotEquals:
        FieldNotEqualsConstant(memory, registers, entry.base, entry.displacement,
                               entry.width, entry.comparison);
        break;
    }
    CopyRegisters(recovered, registers);
    if (std::memcmp(&original, &recovered, sizeof(original)) != 0 ||
        std::memcmp(original_space.bytes, recovered_space.bytes, kLowSize) != 0 ||
        ((entry.family == Family::Global || entry.family == Family::GlobalChain) &&
         std::memcmp(original_space.bytes + global_window,
                     recovered_space.bytes + global_window, kGlobalWindow) != 0))
    {
        std::fprintf(stderr, "FAIL field-operations %08X case %u\n", entry.address, scenario);
        return false;
    }
    return true;
}

} // namespace

int main()
{
    Space original, recovered;
    for (const auto& entry : kEntries)
        for (unsigned scenario = 0; scenario < kCases; ++scenario)
            if (!Test(entry, scenario, original, recovered)) return 1;
    std::printf("PASS field-operations %zu entries %zu cases\n",
                std::size(kEntries), std::size(kEntries) * kCases);
    return 0;
}
