#include "lo_semantics/record_buffer_extent.h"
#include "semantic_oracle_support.h"
#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = record_buffer_extent;
constexpr GuestAddress Record = 0x30000u;
constexpr std::array<test::Region, 1> Regions{{{Record, 0x1000u}}};
struct Case { std::uint32_t count, flag; std::uint64_t extent; };
constexpr std::array<Case, 4> Cases{{
    {7u, 0u, 604u},
    {0x40000001u, 0u, 580u},
    {0xffffffffu, 0u, 0x10000023cull},
    {0xffffffffu, 0x80000000u, 588u},
}};

family::Registers FromPpc(const PPCContext& context)
{
    family::Registers state{};
    const PPCRegister* fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16, &context.r17,
        &context.r18, &context.r19, &context.r20, &context.r21, &context.r22,
        &context.r23, &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
    for (unsigned index = 0; index < 32u; ++index)
        state.r[index] = index == 1u ? 0u : fields[index]->u64;
    state.sp = context.r1.u64;
    state.lr = context.lr;
    state.ctr = context.ctr.u64;
    state.xer_so = context.xer.so;
    state.xer_ca = context.xer.ca;
    state.cr0 = {std::uint8_t(context.cr0.lt),
        std::uint8_t(context.cr0.gt), std::uint8_t(context.cr0.eq),
        std::uint8_t(context.cr0.so)};
    state.cr6 = {std::uint8_t(context.cr6.lt),
        std::uint8_t(context.cr6.gt), std::uint8_t(context.cr6.eq),
        std::uint8_t(context.cr6.so)};
    return state;
}

bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr && a.ctr == b.ctr &&
        a.xer_so == b.xer_so && a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so;
}

bool Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    for (auto* window : {&original, &recovered})
    {
        window->Fill(0xa5u);
        window->Memory().WriteU32(Record + 252u, item.count);
        window->Memory().WriteU32(Record + 508u, item.flag);
    }
    PPCContext raw{};
    raw.r1.u64 = 0x1234567800080000ull;
    raw.r3.u64 = 0x8765432100000000ull | Record;
    raw.r10.u64 = 0x1122334455667788ull;
    raw.r11.u64 = 0x8899aabbccddeeffull;
    raw.r31.u64 = 0x9988776655443322ull;
    raw.lr = 0xabcdef0123456789ull;
    raw.ctr.u64 = 0x5566778899aabbccull;
    raw.xer.so = 1;
    raw.xer.ca = 1;
    raw.cr0.gt = 1;
    raw.cr6.lt = 1;
    auto state = FromPpc(raw);
    __imp__sub_82373190(raw, original.Bytes());
    if (raw.r3.u64 != item.extent || raw.r10.u64 != item.flag ||
        raw.r11.u64 != item.extent - 576u)
        throw std::runtime_error("extent fixture missed original path");
    auto memory = recovered.Memory();
    if (!family::Apply(0x82373190u, memory, state) ||
        !Same(FromPpc(raw), state) || !original.EqualCommitted(recovered))
        throw std::runtime_error("extent original/recovered mismatch");
    return true;
}
}

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS record-buffer-extent %zu focused original PPC cases\n", Cases.size());
        std::puts("LIMIT selected GPR/CR/XER and ordinary RAM; faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
