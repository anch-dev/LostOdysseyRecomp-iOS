#include "lo_semantics/registered_metadata_string.h"
#include "lo_semantics/memory_move.h"
#include "lo_semantics/registered_metadata_words.h"
#include "lo_semantics/registered_constructor_family.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace strings = lo::semantic::gpu::registered_metadata_string;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Header = 0x10000u;
constexpr GuestAddress Source = 0x20000u;
constexpr GuestAddress Storage = 0x30000u;
constexpr GuestAddress Owner = 0x70000u;
constexpr GuestAddress Object = 0x71000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Manager = 0x83000u;
constexpr GuestAddress Vtable = 0x83100u;
constexpr GuestAddress ManagerGlobal = 0x8330B608u;
constexpr GuestAddress Singleton = 0x83315F68u;
constexpr GuestAddress Method = 0x82450000u;

enum class Case { Length, EmptyLength, Initialize, EmptyInitialize,
    Assign, AssignSelf, AssignHeaderAlias, AssignMutate, TailAssign,
    Metadata, Registration, RegistrationNull };

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x82190000u, 0x2000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x821A8000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x82201000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330B000u, 0x20000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit string oracle guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    std::array<std::uint64_t, 5> values;
    bool operator==(const Event&) const = default;
};

struct Services final : ArrayResizeServices, ManagerFacadeServices,
    registered_constructor_family::RegistrationServices
{
    GuestMemory memory;
    Case mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Case selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected manager init"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({{method, manager, old_storage, bytes, argument}});
        if (method != Method || manager != Manager || argument != 8u)
            throw std::runtime_error("unexpected resize arguments");
        if (mode == Case::AssignMutate)
            memory.WriteU32(Header + 4u, 0);
        return Storage;
    }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary constructor"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback constructor"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected release"); }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected constructor allocation"); }
    std::uint64_t Register(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected registration callback"); }
};

Services* active = nullptr;

void PutString(GuestMemory& memory, GuestAddress address, bool empty = false)
{
    memory.WriteU16(address, empty ? 0 : 0x0041u);
    memory.WriteU16(address + 2u, 0);
}

void Setup(Window& window, Case mode)
{
    std::memset(window.bytes, 0, 0x90000);
    std::memset(window.bytes + 0x82190000u, 0, 0x2000);
    std::memset(window.bytes + 0x821A8000u, 0, 0x1000);
    std::memset(window.bytes + 0x82201000u, 0, 0x1000);
    std::memset(window.bytes + 0x8330B000u, 0, 0x20000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 8u, Method | 3u);
    memory.WriteU32(Singleton, 0x12345678u);
    PutString(memory, Source, mode == Case::EmptyLength ||
        mode == Case::EmptyInitialize);
    PutString(memory, 0x821909C8u);
    PutString(memory, 0x821A83D0u);
    PutString(memory, 0x82201354u);
    memory.WriteU32(Header, 0x40000u);
    memory.WriteU32(Header + 4u, 7u);
    memory.WriteU32(Header + 8u, 9u);
    if (mode == Case::AssignHeaderAlias)
        memory.WriteU32(Header + 8u, 0x00410000u);
    memory.WriteU32(Owner + 52u, Object);
    memory.WriteU32(Object + 364u, 0x50000u);
    memory.WriteU32(Object + 368u, 0);
    memory.WriteU32(Object + 372u, 4u);
    memory.WriteU32(Object + 92u, 0x80000020u);
    memory.WriteU32(Object + 72u, 0);
    memory.WriteU32(Object + 76u, 0);
    memory.WriteU32(Object + 80u, mode == Case::Registration ? 0x60000u :
        mode == Case::RegistrationNull ? 0 : 0);
    memory.WriteU32(Object + 84u, 0);
    memory.WriteU32(Object + 88u, mode == Case::Registration ||
        mode == Case::RegistrationNull ? 4u : 0u);
}

bool Compare(Case mode, Window& original, Window& recovered)
{
    Setup(original, mode);
    Setup(recovered, mode);
    Services expected(original.bytes, mode), actual(recovered.bytes, mode);
    PPCContext ctx{};
    ctx.r1.u64 = Stack;
    ctx.lr = 0x12340000u;
    const GuestAddress address = mode == Case::Length || mode == Case::EmptyLength ?
        0x82296830u : mode == Case::Initialize || mode == Case::EmptyInitialize ?
        0x8229C8B0u : mode == Case::Assign || mode == Case::AssignSelf ||
        mode == Case::AssignHeaderAlias || mode == Case::AssignMutate ?
        0x8229F5E0u : mode == Case::TailAssign ? 0x82723DB8u :
        mode == Case::Metadata ? 0x82722D18u : 0x824070C8u;
    const std::uint64_t incoming_r3 = mode == Case::Length ||
        mode == Case::EmptyLength ? 0xABCDEF0000020000ull :
        mode == Case::Registration || mode == Case::RegistrationNull ||
        mode == Case::Metadata || mode == Case::TailAssign ?
        0xABCDEF0000071000ull : 0xABCDEF0000010000ull;
    const std::uint64_t incoming_r4 = mode == Case::AssignSelf ?
        0xDEADBEEF00040000ull : mode == Case::AssignHeaderAlias ?
        0xDEADBEEF00010008ull : 0xDEADBEEF00020000ull;
    ctx.r3.u64 = incoming_r3;
    ctx.r4.u64 = incoming_r4;
    active = &expected;
    switch (address)
    {
    case 0x82296830u: __imp__sub_82296830(ctx, original.bytes); break;
    case 0x8229C8B0u: __imp__sub_8229C8B0(ctx, original.bytes); break;
    case 0x8229F5E0u: __imp__sub_8229F5E0(ctx, original.bytes); break;
    case 0x824070C8u: __imp__sub_824070C8(ctx, original.bytes); break;
    case 0x82722D18u: __imp__sub_82722D18(ctx, original.bytes); break;
    default: __imp__sub_82723DB8(ctx, original.bytes); break;
    }
    std::uint64_t result = 0;
    if (!strings::Apply(address, actual.memory, actual, actual, actual,
            incoming_r3, incoming_r4, Stack, result))
        throw std::runtime_error("missing reviewed string mapping");
    const bool same = ctx.r3.u64 == result && expected.events == actual.events &&
        std::memcmp(original.bytes + 0x10000, recovered.bytes + 0x10000, 0x60000) == 0 &&
        std::memcmp(original.bytes + 0x71000, recovered.bytes + 0x71000, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x82190000u, recovered.bytes + 0x82190000u,
            0x2000) == 0 &&
        std::memcmp(original.bytes + 0x8330B000u, recovered.bytes + 0x8330B000u,
            0x20000) == 0;
    const GuestAddress spill = mode == Case::Metadata || mode == Case::Registration ?
        Stack - 232u : Stack - 120u;
    const bool copied = mode == Case::Initialize || mode == Case::Assign ||
        mode == Case::AssignHeaderAlias || mode == Case::TailAssign ||
        mode == Case::Metadata || mode == Case::Registration;
    const bool same_spill = !copied ||
        (expected.memory.ReadU32(spill) == actual.memory.ReadU32(spill) &&
         expected.memory.ReadU32(spill + 4u) ==
             actual.memory.ReadU32(spill + 4u));
    if (!same || !same_spill)
    {
        std::fprintf(stderr, "FAIL string mode=%u r3=%llx/%llx events=%zu/%zu\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(ctx.r3.u64),
            static_cast<unsigned long long>(result), expected.events.size(),
            actual.events.size());
        for (auto [start, size] : {std::pair{0x10000u, 0x60000u},
             std::pair{0x71000u, 0x1000u},
             std::pair{0x82190000u, 0x2000u},
             std::pair{0x8330B000u, 0x20000u}})
            for (std::uint32_t offset = 0; offset < size; ++offset)
                if (original.bytes[start + offset] != recovered.bytes[start + offset])
                {
                    std::fprintf(stderr, "  first byte %08x = %02x/%02x\n",
                        start + offset, original.bytes[start + offset],
                        recovered.bytes[start + offset]);
                    break;
                }
    }
    return same && same_spill;
}
} // namespace

PPC_FUNC(sub_82296830) { __imp__sub_82296830(ctx, base); }
PPC_FUNC(sub_8229C8B0) { __imp__sub_8229C8B0(ctx, base); }
PPC_FUNC(sub_8229F5E0) { __imp__sub_8229F5E0(ctx, base); }
PPC_FUNC(sub_8229F678)
{ ResizeArray(active->memory, *active, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32); }
PPC_FUNC(sub_82B7A0B0)
{ ctx.r3.u64 = CopyGuestMemory(active->memory, ctx.r3.u64, ctx.r4.u32,
      ctx.r5.u64, ctx.r1.u32); }
PPC_FUNC(sub_822C42D8)
{ ctx.r3.u64 = registered_metadata_words::AddArrayElements(active->memory,
      *active, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32); }
PPC_FUNC(sub_825F41E8)
{ ctx.r3.u64 = registered_metadata_words::AppendMetadataWord(active->memory,
      *active, ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_8240B1B8)
{
    std::uint64_t result = 0;
    if (!registered_constructor_family::Apply(0x8240B1B8u, active->memory,
            *active, *active, ctx.r3.u64, ctx.r1.u32, result))
        throw std::runtime_error("singleton mapping missing");
    ctx.r3.u64 = result;
}

int main()
{
    try
    {
        Window original, recovered;
        unsigned cases = 0;
        for (Case mode : {Case::Length, Case::EmptyLength, Case::Initialize,
             Case::EmptyInitialize, Case::Assign, Case::AssignSelf,
             Case::AssignHeaderAlias, Case::AssignMutate,
             Case::TailAssign, Case::Metadata, Case::Registration,
             Case::RegistrationNull})
        {
            if (!Compare(mode, original, recovered)) return 1;
            ++cases;
        }
        // Unknown dispatch must not touch any guest byte or result register.
        Services services(recovered.bytes, Case::Length);
        std::uint64_t untouched = 0x1122334455667788ull;
        if (strings::Apply(0xFFFFFFFFu, services.memory, services, services, services,
                Header, Source, Stack, untouched) ||
            untouched != 0x1122334455667788ull || !services.events.empty())
            return 1;
        std::printf("PASS registered-metadata-string %u original-PPC cases; unknown entry\n",
            cases);
        std::puts("LIMIT mapped resize/copy/metadata/singleton dependencies use their previously recovered models; singleton allocation path and ABI saves excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
