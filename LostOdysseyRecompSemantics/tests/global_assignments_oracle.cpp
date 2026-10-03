#include "lo_semantics/global_assignments.h"

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
constexpr std::size_t kHighSize = 0x10000;
constexpr std::uint32_t kHighBase = 0xffff0000u;
constexpr std::size_t kGlobalSize = 0x300000;
constexpr std::uint32_t kGlobalBase = 0x83100000u;
constexpr unsigned kCases = 4;

struct Space
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, kSpace, MEM_RESERVE, PAGE_NOACCESS));
    Space()
    {
        if (!bytes || !VirtualAlloc(bytes, kLowSize, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + kHighBase, kHighSize, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + kGlobalBase, kGlobalSize, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve global-assignment guest space");
    }
    ~Space() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

std::uint64_t Get(const PPCContext& context, GlobalAssignmentRegister reg)
{
    switch (reg)
    {
    case GlobalAssignmentRegister::R3: return context.r3.u64;
    case GlobalAssignmentRegister::R4: return context.r4.u64;
    case GlobalAssignmentRegister::R5: return context.r5.u64;
    case GlobalAssignmentRegister::R6: return context.r6.u64;
    case GlobalAssignmentRegister::R7: return context.r7.u64;
    case GlobalAssignmentRegister::R8: return context.r8.u64;
    case GlobalAssignmentRegister::R9: return context.r9.u64;
    case GlobalAssignmentRegister::R10: return context.r10.u64;
    case GlobalAssignmentRegister::R11: return context.r11.u64;
    }
    throw std::invalid_argument("unrecognized register");
}

void Set(PPCContext& context, GlobalAssignmentRegister reg, std::uint64_t value)
{
    switch (reg)
    {
    case GlobalAssignmentRegister::R3: context.r3.u64 = value; return;
    case GlobalAssignmentRegister::R4: context.r4.u64 = value; return;
    case GlobalAssignmentRegister::R5: context.r5.u64 = value; return;
    case GlobalAssignmentRegister::R6: context.r6.u64 = value; return;
    case GlobalAssignmentRegister::R7: context.r7.u64 = value; return;
    case GlobalAssignmentRegister::R8: context.r8.u64 = value; return;
    case GlobalAssignmentRegister::R9: context.r9.u64 = value; return;
    case GlobalAssignmentRegister::R10: context.r10.u64 = value; return;
    case GlobalAssignmentRegister::R11: context.r11.u64 = value; return;
    }
    throw std::invalid_argument("unrecognized register");
}

GlobalAssignmentRegisters CopyRegisters(const PPCContext& context)
{
    return {context.r3.u64, context.r4.u64, context.r5.u64, context.r6.u64,
            context.r7.u64, context.r8.u64, context.r9.u64, context.r10.u64,
            context.r11.u64};
}

void CopyRegisters(PPCContext& context, const GlobalAssignmentRegisters& registers)
{
    for (unsigned number = 3; number <= 11; ++number)
    {
        const auto reg = static_cast<GlobalAssignmentRegister>(number - 3);
        Set(context, reg, registers.Get(reg));
    }
}

void Fill(std::uint8_t* bytes, std::size_t size, std::uint32_t seed)
{
    for (std::size_t i = 0; i < size; ++i)
        bytes[i] = static_cast<std::uint8_t>(i * 19u + seed + (i >> 9));
}

void Prepare(const GlobalAssignmentEntry& entry, unsigned scenario, PPCContext& context)
{
    const std::uint64_t upper = scenario & 1 ? 0xdeadbeef00000000ull :
                                               0x1234567800000000ull;
    for (unsigned number = 3; number <= 11; ++number)
    {
        const auto reg = static_cast<GlobalAssignmentRegister>(number - 3);
        Set(context, reg, upper | (0xa5b6c7d0u + number * 0x01010101u +
                                   scenario * 0x00100010u));
    }

    for (const auto& write : entry.writes)
    {
        if (write.address_kind != AssignmentAddressKind::ObjectField) continue;
        const unsigned number = static_cast<unsigned>(write.base_register) + 3;
        std::uint32_t base = 0x10000u + number * 0x10000u;
        if (scenario == 1) base = 0x25000u; // Outputs can alias one another.
        if (scenario == 2) base = 0xfffff000u; // Displacements can wrap to low memory.
        Set(context, write.base_register, upper | base);
    }

    if (scenario == 3)
    {
        const AssignmentFieldWrite* object = nullptr;
        const AssignmentFieldWrite* global = nullptr;
        for (const auto& write : entry.writes)
        {
            if (!object && write.address_kind == AssignmentAddressKind::ObjectField)
                object = &write;
            if (!global && write.address_kind == AssignmentAddressKind::Global)
                global = &write;
        }
        if (object && global)
        {
            const std::uint32_t base = global->global_address -
                                       static_cast<std::uint32_t>(object->displacement);
            Set(context, object->base_register, upper | base);
        }
    }
}

bool EqualMemory(const Space& original, const Space& recovered)
{
    return std::memcmp(original.bytes, recovered.bytes, kLowSize) == 0 &&
           std::memcmp(original.bytes + kHighBase, recovered.bytes + kHighBase,
                       kHighSize) == 0 &&
           std::memcmp(original.bytes + kGlobalBase,
                       recovered.bytes + kGlobalBase, kGlobalSize) == 0;
}

bool Test(const GlobalAssignmentEntry& entry, unsigned scenario, Space& original_space,
          Space& recovered_space)
{
    Fill(original_space.bytes, kLowSize, entry.address + scenario);
    Fill(original_space.bytes + kHighBase, kHighSize, entry.address ^ scenario);
    Fill(original_space.bytes + kGlobalBase, kGlobalSize, entry.address + scenario * 31u);
    std::memcpy(recovered_space.bytes, original_space.bytes, kLowSize);
    std::memcpy(recovered_space.bytes + kHighBase,
                original_space.bytes + kHighBase, kHighSize);
    std::memcpy(recovered_space.bytes + kGlobalBase,
                original_space.bytes + kGlobalBase, kGlobalSize);

    PPCContext original{};
    Fill(reinterpret_cast<std::uint8_t*>(&original), sizeof(original),
         entry.address + scenario * 17u);
    Prepare(entry, scenario, original);
    PPCContext recovered = original;

    entry.original(original, original_space.bytes);
    GuestMemory recovered_memory(0, {recovered_space.bytes, kSpace});
    GlobalAssignmentRegisters registers = CopyRegisters(recovered);
    WriteFieldAssignments(recovered_memory, registers, entry.writes,
                          entry.register_values_after);
    CopyRegisters(recovered, registers);

    if (std::memcmp(&original, &recovered, sizeof(original)) != 0 ||
        !EqualMemory(original_space, recovered_space))
    {
        std::fprintf(stderr, "FAIL global-assignments %08X case %u\n",
                     entry.address, scenario);
        return false;
    }
    return true;
}

} // namespace

int main()
{
    Space original, recovered;
    for (const auto& entry : kGlobalAssignmentEntries)
        for (unsigned scenario = 0; scenario < kCases; ++scenario)
            if (!Test(entry, scenario, original, recovered)) return 1;
    std::printf("PASS global-assignments %zu entries %zu cases\n",
                std::size(kGlobalAssignmentEntries),
                std::size(kGlobalAssignmentEntries) * kCases);
    return 0;
}
