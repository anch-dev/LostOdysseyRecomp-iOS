// Appended after the two complete pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/crt_float_conversion.h"
#include "lo_semantics/memory_move.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_float_conversion;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Record = 0x40000u;
constexpr GuestAddress Descriptor = 0x50000u;
constexpr GuestAddress Output = 0x60000u;
constexpr std::array<Region, 3> Regions{{
    {0, 0x90000u}, {0x820d4000u, 0x2000u},
    {0x83215000u, 0x2000u}}};

enum class Mode
{
    PositiveZero, NegativeZero, Infinity, QuietNan, NormalOne,
    NormalFraction, RoundCarry, SmallMagnitude, LargeMagnitude,
    WrapperOne, WrapperFraction
};

struct Case
{
    GuestAddress entry;
    Mode mode;
    const char* name;
};

constexpr std::array<Case, 11> Cases{{
    {0x8231a470u, Mode::PositiveZero, "core +0"},
    {0x8231a470u, Mode::NegativeZero, "core -0"},
    {0x8231a470u, Mode::Infinity, "core +infinity"},
    {0x8231a470u, Mode::QuietNan, "core NaN"},
    {0x8231a470u, Mode::NormalOne, "core 1"},
    {0x8231a470u, Mode::NormalFraction, "core fraction"},
    {0x8231a470u, Mode::RoundCarry, "core round carry"},
    {0x8231a470u, Mode::SmallMagnitude, "core small scaling"},
    {0x8231a470u, Mode::LargeMagnitude, "core large scaling"},
    {0x8231a2f0u, Mode::WrapperOne, "wrapper 1"},
    {0x8231a2f0u, Mode::WrapperFraction, "wrapper fraction"},
}};

struct Services final : CrtThreadDataServices, InvalidParameterServices,
    crt_float_environment::NativeServices
{
    GuestMemory memory;
    unsigned native_calls = 0;
    explicit Services(GuestWindow& window) : memory(window.Memory()) {}
    [[noreturn]] static void Unexpected()
    { throw std::runtime_error("unexpected CRT native boundary"); }
    std::uint64_t GetTlsValue(std::uint32_t) override { Unexpected(); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override { Unexpected(); }
    std::uint64_t CallThreadDataGetter(GuestAddress,
        std::uint64_t) override { Unexpected(); }
    std::uint64_t AllocateThreadData(std::uint32_t,
        std::uint32_t) override { Unexpected(); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override { Unexpected(); }
    void FreeThreadData(std::uint64_t) override { Unexpected(); }
    void CallHandler(GuestMemory&, GuestAddress,
        InvalidParameterCall&) override { Unexpected(); }
    void Trap(const InvalidParameterCall&) override { Unexpected(); }
    void CallDebugMonitor(GuestAddress, GuestMemory&,
        family::Registers&) override { Unexpected(); }
    void CallExceptionHandler(GuestAddress, GuestMemory&,
        family::Registers&) override { Unexpected(); }
    void BugCheck(GuestMemory&, family::Registers&) override
    { ++native_calls; Unexpected(); }
};

Services* active = nullptr;

std::array<PPCRegister*, 18> Saved(PPCContext& context)
{
    return {&context.r14, &context.r15, &context.r16, &context.r17,
        &context.r18, &context.r19, &context.r20, &context.r21,
        &context.r22, &context.r23, &context.r24, &context.r25,
        &context.r26, &context.r27, &context.r28, &context.r29,
        &context.r30, &context.r31};
}

void ReadImageRange(GuestMemory& memory, GuestAddress address,
    std::size_t length)
{
    std::ifstream image("LostOdysseyRecompLib/private/image_disc1.bin",
        std::ios::binary);
    if (!image)
        throw std::runtime_error("missing private image_disc1.bin fixture");
    image.seekg(static_cast<std::streamoff>(address - 0x82000000u));
    std::vector<char> bytes(length);
    image.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (image.gcount() != static_cast<std::streamsize>(bytes.size()))
        throw std::runtime_error("short image_disc1.bin fixture range");
    for (std::size_t offset = 0; offset < bytes.size(); ++offset)
        memory.WriteU8(address + static_cast<GuestAddress>(offset),
            static_cast<std::uint8_t>(bytes[offset]));
}

void Seed(GuestWindow& window, const Case& item)
{
    window.Fill(0xa5u);
    auto memory = window.Memory();
    ReadImageRange(memory, 0x820d4c80u, 128u);
    ReadImageRange(memory, 0x83215c00u, 1024u);
    (void)item;
}

void Extended(Mode mode, std::uint16_t& exponent,
    std::uint32_t& high, std::uint32_t& low)
{
    exponent = 0x3fffu; high = 0x80000000u; low = 0;
    switch (mode)
    {
    case Mode::PositiveZero: exponent = 0; high = 0; break;
    case Mode::NegativeZero: exponent = 0x8000u; high = 0; break;
    case Mode::Infinity: exponent = 0x7fffu; break;
    case Mode::QuietNan: exponent = 0x7fffu;
        high = 0xc0000000u; low = 0x800u; break;
    case Mode::NormalFraction: exponent = 0x3ffbu;
        high = 0xccccccccu; low = 0xcccccccdu; break;
    case Mode::RoundCarry: exponent = 0x4002u;
        high = 0x9fffffffu; low = 0xffffffffu; break;
    case Mode::SmallMagnitude: exponent = 0x3f9au; break;
    case Mode::LargeMagnitude: exponent = 0x4063u; break;
    default: break;
    }
}

PPCContext Initial(const Case& item)
{
    PPCContext context{};
    context.r1.u64 = 0x1234567800000000ull | Stack;
    context.lr = 0xabcdef0101020304ull;
    context.ctr.u64 = 0x8877665544332211ull;
    context.r13.u64 = 0xeeeeeeee00070000ull;
    context.xer.so = 1;
    context.xer.ca = 1;
    context.cr0 = {1, 0, 0, {1}};
    context.cr6 = {0, 1, 0, {1}};
    const auto registers = Saved(context);
    for (unsigned index = 0; index < registers.size(); ++index)
        registers[index]->u64 = 0x7171717100000000ull | (index + 14u);
    if (item.entry == 0x8231a2f0u)
    {
        context.r3.u64 = item.mode == Mode::WrapperOne ?
            0x3ff0000000000000ull : 0x3fb999999999999aull;
        context.r4.u64 = 0x2222222200000000ull | Descriptor;
        context.r5.u64 = 0x3333333300000000ull | Output;
        context.r6.u64 = 32;
    }
    else
    {
        std::uint16_t exponent = 0;
        std::uint32_t high = 0, low = 0;
        Extended(item.mode, exponent, high, low);
        context.r3.u64 = (std::uint64_t{exponent} << 48u) |
            (std::uint64_t{high} << 16u) | (low >> 16u);
        context.r4.u64 = std::uint64_t{low & 0xffffu} << 48u;
        context.r5.u64 = 17;
        context.r6.u64 = 0;
        context.r7.u64 = 0x7777777700000000ull | Record;
    }
    return context;
}

family::Registers FromPpc(const PPCContext& context)
{
    family::Registers state{};
    state.sp = context.r1.u64;
    state.lr = context.lr;
    state.ctr = context.ctr.u64;
    state.r = {context.r0.u64, context.r1.u64, context.r2.u64,
        context.r3.u64, context.r4.u64, context.r5.u64,
        context.r6.u64, context.r7.u64, context.r8.u64,
        context.r9.u64, context.r10.u64, context.r11.u64,
        context.r12.u64, context.r13.u64, context.r14.u64,
        context.r15.u64, context.r16.u64, context.r17.u64,
        context.r18.u64, context.r19.u64, context.r20.u64,
        context.r21.u64, context.r22.u64, context.r23.u64,
        context.r24.u64, context.r25.u64, context.r26.u64,
        context.r27.u64, context.r28.u64, context.r29.u64,
        context.r30.u64, context.r31.u64};
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

void ToPpc(const family::Registers& state, PPCContext& context)
{
    context.r1.u64 = state.sp;
    context.lr = state.lr;
    context.ctr.u64 = state.ctr;
    PPCRegister* registers[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6,
        &context.r7, &context.r8, &context.r9, &context.r10,
        &context.r11, &context.r12, &context.r13, &context.r14,
        &context.r15, &context.r16, &context.r17, &context.r18,
        &context.r19, &context.r20, &context.r21, &context.r22,
        &context.r23, &context.r24, &context.r25, &context.r26,
        &context.r27, &context.r28, &context.r29, &context.r30,
        &context.r31};
    for (unsigned index = 0; index < 32; ++index)
        registers[index]->u64 = state.r[index];
    context.r1.u64 = state.sp;
    context.xer.so = state.xer_so;
    context.xer.ca = state.xer_ca;
    context.cr0 = {bool(state.cr0.lt), bool(state.cr0.gt),
        bool(state.cr0.eq), {bool(state.cr0.so)}};
    context.cr6 = {bool(state.cr6.lt), bool(state.cr6.gt),
        bool(state.cr6.eq), {bool(state.cr6.so)}};
}

bool Same(const family::Registers& state, const PPCContext& raw)
{
    const auto expected = FromPpc(raw);
    return state.sp == expected.sp && state.lr == expected.lr &&
        state.ctr == expected.ctr && state.r == expected.r &&
        state.xer_so == expected.xer_so && state.xer_ca == expected.xer_ca &&
        state.cr0.lt == expected.cr0.lt &&
        state.cr0.gt == expected.cr0.gt &&
        state.cr0.eq == expected.cr0.eq &&
        state.cr0.so == expected.cr0.so &&
        state.cr6.lt == expected.cr6.lt &&
        state.cr6.gt == expected.cr6.gt &&
        state.cr6.eq == expected.cr6.eq &&
        state.cr6.so == expected.cr6.so;
}

bool ExpectedPath(const Case& item, const PPCContext& raw,
    const Services& service)
{
    const auto memory = service.memory;
    if (service.native_calls)
        return false;
    if (item.entry == 0x8231a2f0u)
        return raw.r3.u64 == (0x2222222200000000ull | Descriptor) &&
            memory.ReadU32(Descriptor + 12u) == Output &&
            memory.ReadU8(Output) >= '0' &&
            memory.ReadU8(Output) <= '9';
    if (item.mode == Mode::PositiveZero || item.mode == Mode::NegativeZero)
        return raw.r3.u64 == 1u && memory.ReadU8(Record + 3u) == 1u &&
            memory.ReadU8(Record + 4u) == '0';
    if (item.mode == Mode::Infinity || item.mode == Mode::QuietNan)
        return raw.r3.u64 == 0u &&
            (memory.ReadU8(Record + 3u) == 5u ||
                memory.ReadU8(Record + 3u) == 6u);
    return raw.r3.u64 == 1u && memory.ReadU8(Record + 3u) >= 1u &&
        memory.ReadU8(Record + 3u) <= 21u &&
        memory.ReadU8(Record + 4u) >= '0' &&
        memory.ReadU8(Record + 4u) <= '9';
}

bool Check(const Case& item)
{
    std::fprintf(stderr, "begin %08X %s\n", item.entry, item.name);
    std::fflush(stderr);
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, item);
    Seed(recovered, item);
    Services expected(original), actual(recovered);
    auto raw = Initial(item);
    auto state = FromPpc(raw);
    active = &expected;
    if (item.entry == 0x8231a470u)
        __imp__sub_8231A470(raw, original.Bytes());
    else
        __imp__sub_8231A2F0(raw, original.Bytes());
    active = nullptr;
    std::fprintf(stderr, "original returned %08X %s\n", item.entry, item.name);
    std::fflush(stderr);
    if (!ExpectedPath(item, raw, expected))
    {
        std::fprintf(stderr, "fixture path %s: r3=%llX count=%u first=%02X\n",
            item.name, static_cast<unsigned long long>(raw.r3.u64),
            expected.memory.ReadU8(item.entry == 0x8231a470u ?
                Record + 3u : Descriptor),
            expected.memory.ReadU8(item.entry == 0x8231a470u ?
                Record + 4u : Output));
        throw std::runtime_error("fixture missed selected original branch");
    }
    if (!family::Apply(item.entry, actual.memory,
            {{actual, actual}, actual}, state))
        throw std::runtime_error("recovered conversion entry missing");
    std::fprintf(stderr, "recovered returned %08X %s\n", item.entry, item.name);
    std::fflush(stderr);
    if (!Same(state, raw) || !original.EqualCommitted(recovered))
    {
        const auto expected_state = FromPpc(raw);
        const auto U = [](auto value) { return static_cast<unsigned>(value); };
        std::fprintf(stderr, "mismatch %s: r3=%llX/%llX LR=%llX/%llX "
            "SP=%llX/%llX memory=%u\n", item.name,
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(raw.r1.u64),
            static_cast<unsigned long long>(state.sp),
            unsigned(original.EqualCommitted(recovered)));
        unsigned register_differences = 0;
        for (unsigned index = 0; index < 32; ++index)
            if (expected_state.r[index] != state.r[index] &&
                register_differences++ < 12)
                std::fprintf(stderr, "  r%u %016llX/%016llX\n", index,
                    static_cast<unsigned long long>(expected_state.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        if (expected_state.ctr != state.ctr ||
            expected_state.xer_ca != state.xer_ca ||
            expected_state.xer_so != state.xer_so ||
            expected_state.cr0.lt != state.cr0.lt ||
            expected_state.cr0.gt != state.cr0.gt ||
            expected_state.cr0.eq != state.cr0.eq ||
            expected_state.cr6.lt != state.cr6.lt ||
            expected_state.cr6.gt != state.cr6.gt ||
            expected_state.cr6.eq != state.cr6.eq)
            std::fprintf(stderr, "  CTR %016llX/%016llX XER %u,%u/%u,%u "
                "CR0 %u%u%u/%u%u%u CR6 %u%u%u/%u%u%u\n",
                static_cast<unsigned long long>(expected_state.ctr),
                static_cast<unsigned long long>(state.ctr),
                U(expected_state.xer_so), U(expected_state.xer_ca),
                U(state.xer_so), U(state.xer_ca),
                U(expected_state.cr0.lt), U(expected_state.cr0.gt),
                U(expected_state.cr0.eq), U(state.cr0.lt), U(state.cr0.gt),
                U(state.cr0.eq), U(expected_state.cr6.lt),
                U(expected_state.cr6.gt), U(expected_state.cr6.eq),
                U(state.cr6.lt), U(state.cr6.gt), U(state.cr6.eq));
        unsigned memory_differences = 0;
        const auto inspect = [&](GuestAddress begin, GuestAddress end)
        {
            for (auto address = begin; address < end; ++address)
                if (original.Bytes()[address] != recovered.Bytes()[address] &&
                    memory_differences++ < 16)
                    std::fprintf(stderr, "  RAM %08X %02X/%02X\n", address,
                        U(original.Bytes()[address]),
                        U(recovered.Bytes()[address]));
        };
        inspect(Record, Record + 64u);
        inspect(Stack - 320u, Stack + 80u);
        return false;
    }
    return true;
}
} // namespace

void OriginalSave(unsigned first, PPCContext& context)
{
    auto memory = active->memory;
    memory.WriteU32(Address(context.r1.u64 - 8u), context.r12.u32);
    const auto saved = Saved(context);
    for (unsigned index = first; index <= 31; ++index)
        WriteU64(memory, Address(context.r1.u64 - 16u -
            8u * (31u - index)), saved[index - 14u]->u64);
}

void OriginalRestore(unsigned first, PPCContext& context)
{
    auto memory = active->memory;
    context.r12.u64 = memory.ReadU32(Address(context.r1.u64 - 8u));
    context.lr = context.r12.u64;
    const auto saved = Saved(context);
    for (unsigned index = first; index <= 31; ++index)
        saved[index - 14u]->u64 = ReadU64(memory,
            Address(context.r1.u64 - 16u - 8u * (31u - index)));
}

void OriginalCallee(GuestAddress entry, PPCContext& context, std::uint8_t*)
{
    auto state = FromPpc(context);
    if (entry == 0x82b7a0b0u)
        state.r[3] = CopyGuestMemory(active->memory, state.r[3],
            Address(state.r[4]), state.r[5], Address(state.sp));
    else if (entry == 0x8231a390u || entry == 0x8231b0d0u)
    {
        if (!crt_float_core_helpers::Apply(entry, active->memory,
                {*active, *active}, state))
            throw std::runtime_error("accepted float helper unavailable");
    }
    else if (entry == 0x82b7ff08u)
    {
        if (!crt_float_environment::Apply(entry, active->memory,
                *active, state))
            throw std::runtime_error("accepted float environment unavailable");
    }
    else
        throw std::runtime_error("unknown original callee");
    ToPpc(state, context);
}

int main()
{
    try
    {
        for (const auto& item : Cases)
            if (!Check(item)) return 1;
        GuestWindow window(Regions), baseline(Regions);
        Seed(window, Cases[0]);
        Seed(baseline, Cases[0]);
        Services services(window);
        auto state = FromPpc(Initial(Cases[0]));
        const auto saved = state;
        if (family::Apply(0xffffffffu, services.memory,
                {{services, services}, services}, state) ||
            state.sp != saved.sp || state.lr != saved.lr ||
            state.ctr != saved.ctr || state.r != saved.r ||
            !window.EqualCommitted(baseline))
            throw std::runtime_error("unknown entry changed context");
        std::printf("PASS crt-float-conversion %zu original PPC cases + unknown\n",
            Cases.size());
        std::puts("LIMIT selected PPC context and guest RAM; native internals, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
