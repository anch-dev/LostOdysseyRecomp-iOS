#include "lo_semantics/crt_thread_data.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Stack = 0xf000;
constexpr GuestAddress Environment = 0x2000;
constexpr GuestAddress State = 0x3000;
constexpr GuestAddress Record = 0x5000;
constexpr GuestAddress ContextGlobal = 0x83214d74;
constexpr GuestAddress TlsIndexGlobal = 0x83214d78;
constexpr GuestAddress GetterFallbackGlobal = 0x832d3adc;
constexpr GuestAddress BinderGlobal = 0x832d3ae0;
constexpr GuestAddress Getter = 0x82200103;
constexpr GuestAddress Binder = 0x82200203;

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve guest window");
        if (!VirtualAlloc(bytes, 0x10000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83214000, 0x2000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x832d3000, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit guest pages");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

using Event = std::array<std::uint64_t, 4>;
struct Services : CrtThreadDataServices, CrtErrorOutputServices
{
    GuestMemory memory;
    unsigned variant;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, unsigned test)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), variant(test) {}

    std::uint64_t GetTlsValue(std::uint32_t index) override
    {
        events.push_back({1, index, 0, 0});
        if (variant == 1)
        {
            memory.WriteU32(TlsIndexGlobal, 9);
            memory.WriteU32(ContextGlobal, 0x5678);
        }
        return variant == 1 || variant == 3 ? 0 : Getter;
    }
    void SetTlsValue(std::uint32_t index, std::uint64_t value) override
    { events.push_back({2, index, value, 0}); }
    std::uint64_t CallThreadDataGetter(GuestAddress function,
                                       std::uint64_t context) override
    {
        events.push_back({3, function, context, 0});
        memory.WriteU32(State + 352, 0xeeeeeeee);
        return variant == 0 || variant == 1 ? 0x1234567800006000ull : 0;
    }
    std::uint64_t AllocateThreadData(std::uint32_t count,
                                     std::uint32_t bytes_each) override
    {
        events.push_back({4, count, bytes_each, 0});
        if (variant == 2) memory.WriteU32(ContextGlobal, 0x5678);
        return variant == 3 ? 0 : 0xabcdef0000005000ull;
    }
    std::uint64_t BindThreadData(GuestAddress function,
                                 std::uint64_t context,
                                 std::uint64_t data) override
    {
        events.push_back({5, function, context, data});
        return variant == 4 ? 0 : 1;
    }
    void FreeThreadData(std::uint64_t data) override
    { events.push_back({6, data, 0, 0}); }
    void InitAnsiString(GuestMemory& mem, GuestAddress descriptor,
                        GuestAddress message) override
    {
        events.push_back({7, descriptor, message,
                          mem.ReadU32(descriptor + 4u) |
                          (std::uint64_t{mem.ReadU16(descriptor)} << 32)});
        if (variant == 8)
        {
            mem.WriteU16(descriptor, 7);
            mem.WriteU16(descriptor + 2, 9);
            mem.WriteU32(descriptor + 4, 0x3600);
            return;
        }
        std::uint16_t length = 0;
        if (message)
            while (mem.ReadU8(message + length) != 0) ++length;
        mem.WriteU16(descriptor, length);
        mem.WriteU16(descriptor + 2, message ? length + 1u : 0);
        mem.WriteU32(descriptor + 4, message);
    }
    std::uint64_t WriteAnsi(GuestAddress buffer, std::uint16_t length) override
    {
        events.push_back({8, buffer, length, 0});
        return buffer; // The extracted 82BECB10 trap leaf leaves r3 unchanged.
    }
};

Services* active = nullptr;

void Initialize(std::uint8_t* bytes, unsigned variant)
{
    std::memset(bytes, 0xbd, 0x10000);
    std::memset(bytes + 0x83214000, 0xbd, 0x2000);
    std::memset(bytes + 0x832d3000, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(Environment + 256, State);
    memory.WriteU32(Environment + 336, variant == 5 ? 1 : 0);
    memory.WriteU32(State + 332, 0x778899aa);
    memory.WriteU32(State + 352, 0x11223344);
    memory.WriteU32(ContextGlobal, 0x1234);
    memory.WriteU32(TlsIndexGlobal, 7);
    memory.WriteU32(GetterFallbackGlobal, Getter);
    memory.WriteU32(BinderGlobal, Binder);
    memory.WriteU8(0x3400, 'A');
    memory.WriteU8(0x3401, 'B');
    memory.WriteU8(0x3402, 0);
}

bool Test(unsigned variant, Window& original, Window& recovered)
{
    Initialize(original.bytes, variant);
    Initialize(recovered.bytes, variant);
    Services expected(original.bytes, variant);
    Services actual(recovered.bytes, variant);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.r13.u64 = Environment;
    context.r3.u64 = variant >= 6 ? variant == 6 ? 0 : 0x3400 : 0x9999;
    context.lr = 0x82200000;
    active = &expected;
    std::uint64_t result;
    if (variant < 6)
    {
        __imp__sub_822CA048(context, original.bytes);
        result = GetCrtThreadData(actual.memory, actual, Environment);
    }
    else
    {
        __imp__sub_823ADD70(context, original.bytes);
        result = OutputCrtErrorMessage(actual.memory, actual,
                                       variant == 6 ? 0 : 0x3400, Stack);
    }
    const bool same = context.r3.u64 == result && expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x83214000, recovered.bytes + 0x83214000,
                    0x2000) == 0 &&
        std::memcmp(original.bytes + 0x832d3000, recovered.bytes + 0x832d3000,
                    0x1000) == 0 &&
        (variant < 6 || std::memcmp(original.bytes + Stack - 16,
                                    recovered.bytes + Stack - 16, 8) == 0);
    if (!same)
    {
        std::fprintf(stderr, "FAIL crt-thread-data case %u result %llx/%llx events %zu/%zu\n",
                     variant, static_cast<unsigned long long>(context.r3.u64),
                     static_cast<unsigned long long>(result), expected.events.size(),
                     actual.events.size());
        return false;
    }
    return true;
}
} // namespace

PPC_FUNC(__savegprlr_29) { (void)ctx; (void)base; }
PPC_FUNC(__restgprlr_29) { (void)ctx; (void)base; }
PPC_FUNC(sub_822CA100) { __imp__sub_822CA100(ctx, base); }
PPC_FUNC(sub_822CA108) { __imp__sub_822CA108(ctx, base); }
PPC_FUNC(sub_822CA128) { __imp__sub_822CA128(ctx, base); }
PPC_FUNC(sub_822CA180) { __imp__sub_822CA180(ctx, base); }
PPC_FUNC(sub_822CA188) { __imp__sub_822CA188(ctx, base); }
PPC_FUNC(sub_82290AA8) { __imp__sub_82290AA8(ctx, base); }
PPC_FUNC(sub_823ADDB0) { __imp__sub_823ADDB0(ctx, base); }
PPC_FUNC(__imp__KeTlsGetValue)
{ (void)base; ctx.r3.u64 = active->GetTlsValue(ctx.r3.u32); }
PPC_FUNC(__imp__KeTlsSetValue)
{ (void)base; active->SetTlsValue(ctx.r3.u32, ctx.r4.u64); }
PPC_FUNC(sub_82B81778)
{ (void)base; ctx.r3.u64 = active->AllocateThreadData(ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_823ADDC0)
{ (void)base; active->FreeThreadData(ctx.r3.u64); }
PPC_FUNC(__imp__RtlInitAnsiString)
{ active->InitAnsiString(active->memory, ctx.r3.u32, ctx.r4.u32); (void)base; }
PPC_FUNC(sub_82BECB10)
{ (void)base; ctx.r3.u64 = active->WriteAnsi(ctx.r3.u32, ctx.r4.u16); }
void CrtIndirect(PPCContext& ctx, std::uint8_t* base, std::uint32_t function)
{
    (void)base;
    if (function == (Getter & ~3u))
        ctx.r3.u64 = active->CallThreadDataGetter(function, ctx.r3.u64);
    else if (function == (Binder & ~3u))
        ctx.r3.u64 = active->BindThreadData(function, ctx.r3.u64,
                                            ctx.r4.u64);
    else
        throw std::runtime_error("unexpected callback");
}

int main()
{
    try
    {
        Window original, recovered;
        for (unsigned variant = 0; variant != 9; ++variant)
            if (!Test(variant, original, recovered)) return 1;
        std::puts("PASS crt-thread-data 6 acquisition +3 output cases");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
