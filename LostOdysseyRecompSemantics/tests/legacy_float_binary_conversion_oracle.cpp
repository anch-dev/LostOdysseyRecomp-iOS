#include "lo_semantics/legacy_float_binary_conversion.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_float_binary_conversion;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Input = 0x20000u, Output = 0x30000u;
constexpr GuestAddress Format = 0x83215c50u;
constexpr test::Region Regions[] = {
    {0u, 0x100000u}, {0x83215000u, 0x2000u}
};
struct Case
{
    const char* name;
    std::uint16_t sign_exponent;
    std::uint64_t significand_high;
    std::uint16_t significand_low;
    bool known_output;
    std::uint64_t expected_bits;
};
constexpr Case Cases[] = {
    {"zero", 0x0000u, 0u, 0u, true, 0x0000000000000000ull},
    {"one", 0x3fffu, 0x8000000000000000ull, 0u,
        true, 0x3ff0000000000000ull},
    {"negative-one-half", 0xbfffu, 0xc000000000000000ull, 0u,
        true, 0xbff8000000000000ull},
    {"ties-to-even", 0x3fffu, 0x8000000000000400ull, 0u,
        true, 0x3ff0000000000000ull},
    {"minimum-normal", 0x3c01u, 0x8000000000000000ull, 0u,
        true, 0x0010000000000000ull},
    {"overflow-boundary", 0x43ffu, 0x8000000000000000ull, 0u,
        false, 0u},
    {"underflow-boundary", 0x3bc7u, 0x8000000000000000ull, 0u,
        false, 0u}
};

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}
family::Registers FromPpc(PPCContext& c)
{
    family::Registers state{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        state.r[index] = fields[index]->u64;
    state.ctr = c.ctr.u64; state.lr = c.lr;
    state.xer = {c.xer.so, c.xer.ov, c.xer.ca};
    state.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    state.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    return state;
}
bool Same(const family::Registers& left, const family::Registers& right)
{
    return left.r == right.r && left.ctr == right.ctr &&
        left.lr == right.lr && left.xer == right.xer &&
        left.cr0 == right.cr0 && left.cr6 == right.cr6;
}
GuestMemory* active_memory = nullptr;

void Initialize(const Case& item, GuestMemory& memory)
{
    memory.WriteU16(Input, item.sign_exponent);
    memory.WriteU32(Input + 2u,
        static_cast<std::uint32_t>(item.significand_high >> 32u));
    memory.WriteU32(Input + 6u,
        static_cast<std::uint32_t>(item.significand_high));
    memory.WriteU16(Input + 10u, item.significand_low);
    // Exact six big-endian words from image_disc1.bin at 83215C50.
    constexpr std::array<std::uint32_t, 6> Parameters = {
        1024u, 0xfffffc01u, 53u, 11u, 64u, 1023u
    };
    for (unsigned index = 0; index < Parameters.size(); ++index)
        memory.WriteU32(Format + index * 4u, Parameters[index]);
}

void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xbd); recovered.Fill(0xbd);
    auto left = original.Memory(), right = recovered.Memory();
    Initialize(item, left); Initialize(item, right);
    PPCContext context{};
    const auto fields = Fields(context);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1234000000000000ull | (index * 0x10101u);
    context.r1.u64 = 0x1234567800080000ull;
    context.r3.u64 = 0x1122334400020000ull;
    context.r4.u64 = 0x5566778800030000ull;
    context.ctr.u64 = 0x99aabbccddeeff00ull;
    context.lr = 0x1234567887654321ull;
    context.xer.so = 1; context.xer.ov = 1; context.xer.ca = 1;
    context.cr0.compare<int32_t>(-2, 3, context.xer);
    context.cr6.compare<int32_t>(3, -2, context.xer);
    auto state = FromPpc(context);

    active_memory = &left;
    __imp__sub_822981C8(context, original.Bytes());
    active_memory = nullptr;
    if (!family::Apply(0x822981c8u, right, state))
        throw std::runtime_error("missing 822981C8 lower");
    if (!Same(FromPpc(context), state) ||
        !original.EqualCommitted(recovered))
        throw std::runtime_error(item.name);
    if (item.known_output)
    {
        const auto high = left.ReadU32(Output);
        const auto low = left.ReadU32(Output + 4u);
        const auto actual = (std::uint64_t{high} << 32u) | low;
        if (actual != item.expected_bits)
            throw std::runtime_error("unexpected binary64 fixture value");
    }
}
} // namespace

void OriginalSave(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 19u; index <= 31u; ++index)
        WriteU64(*active_memory, c.r1.u32 - 16u - (31u - index) * 8u,
            fields[index]->u64);
    active_memory->WriteU32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 19u; index <= 31u; ++index)
        fields[index]->u64 = ReadU64(*active_memory,
            c.r1.u32 - 16u - (31u - index) * 8u);
    c.r12.u64 = active_memory->ReadU32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-float-binary-conversion %zu original PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT ordinary RAM and selected 12-byte records; faults, MMIO, alternate tables and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
