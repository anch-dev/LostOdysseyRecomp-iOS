#include "lo_semantics/pointer_fields.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;

constexpr std::size_t kSpace = std::size_t{1} << 32;
constexpr std::size_t kLowSize = 0x100000;
constexpr std::size_t kHighSize = 0x10000;
constexpr std::uint32_t kHighBase = 0xffff0000u;
constexpr unsigned kCases = 8;

struct Space
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, kSpace, MEM_RESERVE, PAGE_NOACCESS));
    Space()
    {
        if (!bytes || !VirtualAlloc(bytes, kLowSize, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + kHighBase, kHighSize, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve pointer-field guest space");
    }
    ~Space() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

std::uint64_t Get(const PPCContext& context, PointerFieldRegister reg)
{
    switch (reg)
    {
    case PointerFieldRegister::R3: return context.r3.u64;
    case PointerFieldRegister::R4: return context.r4.u64;
    case PointerFieldRegister::R5: return context.r5.u64;
    case PointerFieldRegister::R6: return context.r6.u64;
    case PointerFieldRegister::R9: return context.r9.u64;
    case PointerFieldRegister::R10: return context.r10.u64;
    case PointerFieldRegister::R11: return context.r11.u64;
    case PointerFieldRegister::R13: return context.r13.u64;
    }
    throw std::invalid_argument("unrecognized register");
}

void Set(PPCContext& context, PointerFieldRegister reg, std::uint64_t value)
{
    switch (reg)
    {
    case PointerFieldRegister::R3: context.r3.u64 = value; return;
    case PointerFieldRegister::R4: context.r4.u64 = value; return;
    case PointerFieldRegister::R5: context.r5.u64 = value; return;
    case PointerFieldRegister::R6: context.r6.u64 = value; return;
    case PointerFieldRegister::R9: context.r9.u64 = value; return;
    case PointerFieldRegister::R10: context.r10.u64 = value; return;
    case PointerFieldRegister::R11: context.r11.u64 = value; return;
    case PointerFieldRegister::R13: context.r13.u64 = value; return;
    }
    throw std::invalid_argument("unrecognized register");
}

PointerFieldRegisters CopyRegisters(const PPCContext& context)
{
    return {context.r3.u64, context.r4.u64, context.r5.u64, context.r6.u64,
            context.r9.u64, context.r10.u64, context.r11.u64, context.r13.u64};
}

void CopyRegisters(PPCContext& context, const PointerFieldRegisters& registers)
{
    context.r3.u64 = registers.r3;
    context.r4.u64 = registers.r4;
    context.r5.u64 = registers.r5;
    context.r6.u64 = registers.r6;
    context.r9.u64 = registers.r9;
    context.r10.u64 = registers.r10;
    context.r11.u64 = registers.r11;
    context.r13.u64 = registers.r13;
}

void Prepare(const PointerFieldEntry& entry, unsigned scenario,
             PPCContext& context, GuestMemory& memory)
{
    const std::uint64_t upper = (scenario & 1) ? 0xdeadbeef00000000ull :
                                                 0x1234567800000000ull;
    constexpr std::uint32_t bases[] = {
        0x10000u, 0x20000u, 0xfffff000u, 0xfffffd00u,
        0x30000u, 0x40000u, 0xffffef00u, 0x50000u,
    };
    switch (entry.family)
    {
    case PointerFieldFamily::Initialize:
        Set(context, entry.base_register, upper | bases[scenario]);
        break;
    case PointerFieldFamily::Chain:
    case PointerFieldFamily::Member:
    {
        std::uint32_t current = 0x10000u + scenario * 0x1000u;
        Set(context, entry.base_register, upper | current);
        for (std::size_t i = 0; i < entry.load_count; ++i)
        {
            const auto& field = entry.loads[i];
            const std::uint32_t address = current + static_cast<std::uint32_t>(field.displacement);
            const bool is_final_field = entry.family == PointerFieldFamily::Chain &&
                                        i + 1 == entry.load_count;
            if (is_final_field)
            {
                const std::uint32_t value = 0xa58000f0u ^ (scenario * 0x110101u);
                switch (field.width)
                {
                case PointerFieldWidth::Byte: memory.WriteU8(address, static_cast<std::uint8_t>(value)); break;
                case PointerFieldWidth::Halfword: memory.WriteU16(address, static_cast<std::uint16_t>(value)); break;
                case PointerFieldWidth::Word: memory.WriteU32(address, value); break;
                }
            }
            else
            {
                const std::uint32_t next = (scenario == 7 && i == 0) ? current :
                    0x20000u + static_cast<std::uint32_t>(i) * 0x10000u + scenario * 0x1000u;
                memory.WriteU32(address, next);
                current = next;
            }
        }
        break;
    }
    case PointerFieldFamily::ByteIndex:
    {
        constexpr std::uint32_t indices[] = {0, 1, 5, 0xffffffffu, 0x80000000u,
                                             0x7fffffffu, 0x100u, 0xfffffff0u};
        const std::uint32_t target = 0x50000u + scenario * 16u;
        Set(context, PointerFieldRegister::R3,
            upper | (target - indices[scenario] - static_cast<std::uint32_t>(entry.displacement)));
        context.r4.u64 = (upper ^ 0xffff000000000000ull) | indices[scenario];
        memory.WriteU8(target, static_cast<std::uint8_t>(0x81u + scenario));
        break;
    }
    case PointerFieldFamily::BiasedIndex:
    case PointerFieldFamily::PointerArray:
    {
        constexpr std::uint32_t indices[] = {0, 1, 5, 0xffffffffu, 0x80000000u,
                                             0x7fffffffu, 0x100u, 0xfffffff0u};
        const std::uint32_t target = 0x60000u + scenario * 16u;
        context.r4.u64 = upper | indices[scenario];
        if (entry.family == PointerFieldFamily::BiasedIndex)
        {
            const std::uint32_t scaled = (indices[scenario] +
                static_cast<std::uint32_t>(entry.displacement)) << 2;
            context.r3.u64 = upper | (target - scaled);
        }
        else
        {
            context.r3.u64 = upper | (0x10000u + scenario * 0x1000u);
            memory.WriteU32(context.r3.u32 + entry.displacement,
                            target - (indices[scenario] << 2));
        }
        memory.WriteU32(target, 0x8090a0b0u ^ (scenario * 0x10101u));
        break;
    }
    }
}

bool Test(const PointerFieldEntry& entry, unsigned scenario, Space& original_space,
          Space& recovered_space)
{
    for (std::size_t i = 0; i < kLowSize; ++i)
        original_space.bytes[i] = static_cast<std::uint8_t>(i * 19u + scenario + entry.address);
    for (std::size_t i = 0; i < kHighSize; ++i)
        original_space.bytes[kHighBase + i] = static_cast<std::uint8_t>(i * 37u + scenario + entry.address);
    PPCContext original{};
    auto* context_bytes = reinterpret_cast<std::uint8_t*>(&original);
    for (std::size_t i = 0; i < sizeof(original); ++i)
        context_bytes[i] = static_cast<std::uint8_t>(i * 13u + scenario + entry.address);
    GuestMemory original_memory(0, {original_space.bytes, kSpace});
    Prepare(entry, scenario, original, original_memory);
    PPCContext recovered = original;
    std::memcpy(recovered_space.bytes, original_space.bytes, kLowSize);
    std::memcpy(recovered_space.bytes + kHighBase,
                original_space.bytes + kHighBase, kHighSize);

    entry.original(original, original_space.bytes);
    GuestMemory memory(0, {recovered_space.bytes, kSpace});
    PointerFieldRegisters registers = CopyRegisters(recovered);
    switch (entry.family)
    {
    case PointerFieldFamily::Initialize:
        InitializeConstantFields(memory, registers, entry.base_register,
            {entry.assignments, entry.assignment_count}, {entry.writes, entry.write_count});
        break;
    case PointerFieldFamily::Chain:
        ReadPointerChainField(memory, registers, entry.base_register,
                              {entry.loads, entry.load_count});
        break;
    case PointerFieldFamily::Member:
        AddressOfPointerChainMember(memory, registers, entry.base_register,
                                    {entry.loads, entry.load_count}, entry.displacement);
        break;
    case PointerFieldFamily::ByteIndex:
        ReadBaseByteIndexField(memory, registers, entry.displacement);
        break;
    case PointerFieldFamily::BiasedIndex:
        ReadBiasedIndexField(memory, registers, entry.displacement);
        break;
    case PointerFieldFamily::PointerArray:
        ReadPointerArrayElement(memory, registers, entry.displacement);
        break;
    }
    CopyRegisters(recovered, registers);
    if (std::memcmp(&original, &recovered, sizeof(original)) != 0 ||
        std::memcmp(original_space.bytes, recovered_space.bytes, kLowSize) != 0 ||
        std::memcmp(original_space.bytes + kHighBase,
                    recovered_space.bytes + kHighBase, kHighSize) != 0)
    {
        std::fprintf(stderr, "FAIL pointer-fields %08X case %u\n", entry.address, scenario);
        return false;
    }
    return true;
}

} // namespace

int main()
{
    Space original, recovered;
    for (const auto& entry : kPointerFieldEntries)
        for (unsigned scenario = 0; scenario < kCases; ++scenario)
            if (!Test(entry, scenario, original, recovered)) return 1;
    std::printf("PASS pointer-fields %zu entries %zu cases\n",
                std::size(kPointerFieldEntries), std::size(kPointerFieldEntries) * kCases);
    return 0;
}
