#include "lo_semantics/instance_string_initializer_family.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace initializer = lo::semantic::gpu::instance_string_initializer_family;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Data = 0x30000u;
constexpr GuestAddress Source = 0x40000u;
constexpr GuestAddress Stack = 0x60000u;
constexpr GuestAddress Manager = 0x70000u;
constexpr GuestAddress Vtable = 0x70100u;
constexpr GuestAddress ConstantSource = 0x821a83d0u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress ResizeMethod = 0x82345680u;

struct Entry { GuestAddress address; PPCFunc* original; };
constexpr Entry Entries[] = {
/* ENTRY_TABLE */
};

enum class Mode { Direct, Tail, Null, SourceAlias, SaveAlias };

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000, MEM_COMMIT,
                                    PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x821a8000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE))
            throw std::runtime_error("commit instance string guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Services final : ArrayResizeServices
{
    GuestMemory memory;
    std::vector<std::uint32_t> requests;
    Services(std::uint8_t* bytes)
        : memory(0, std::span<std::uint8_t>(bytes, Space)) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        if (method != ResizeMethod || manager != Manager ||
                old_storage != 0 || argument != 8u)
            throw std::runtime_error("unexpected string resize");
        requests.push_back(bytes);
        return Data;
    }
};

Services* active = nullptr;

void Initialize(std::uint8_t* bytes, Mode mode)
{
    std::memset(bytes, 0xbd, 0x90000);
    std::memset(bytes + 0x821a8000u, 0, 0x1000);
    std::memset(bytes + 0x8330b000u, 0, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 8u, ResizeMethod | 3u);
    memory.WriteU16(Source, 0x0041u);
    memory.WriteU16(Source + 2u, 0x0042u);
    memory.WriteU16(Source + 4u, 0u);
    memory.WriteU16(ConstantSource, 0x0058u);
    memory.WriteU16(ConstantSource + 2u, 0u);
    if (mode == Mode::SourceAlias)
        memory.WriteU32(ConstantSource + 4u, 0);
}

bool Compare(const Entry& entry, Mode mode, Window& original,
    Window& recovered)
{
    Initialize(original.bytes, mode);
    Initialize(recovered.bytes, mode);
    Services expected(original.bytes), actual(recovered.bytes);
    const GuestAddress object = mode == Mode::SaveAlias ? Stack - 76u :
        mode == Mode::SourceAlias ? ConstantSource - 60u :
        mode == Mode::Null ? 0u : Object;
    const std::uint64_t incoming_r3 = 0xabcdef0000000000ull | object;
    const std::uint64_t incoming_r4 = mode == Mode::SourceAlias ?
        0x1234567800000000ull | (Object + 60u) :
        0x1234567800000000ull | Source;
    PPCContext context{};
    context.r1.u64 = Stack;
    context.r3.u64 = incoming_r3;
    context.r4.u64 = incoming_r4;
    context.r31.u64 = 0x1122334455667788ull;
    context.lr = 0xabcdef0082200000ull;
    active = &expected;
    entry.original(context, original.bytes);

    const initializer::EntryAbi abi{Stack, 0xabcdef0082200000ull,
                                    0x1122334455667788ull};
    initializer::Result result{};
    active = &actual;
    if (!initializer::Apply(entry.address, actual.memory, actual,
                             incoming_r3, incoming_r4, abi, result))
        throw std::runtime_error("instance string entry missing");
    const bool same = result.r3 == context.r3.u64 &&
        result.lr == context.lr && result.r31 == context.r31.u64 &&
        context.r1.u64 == Stack && expected.requests == actual.requests &&
        std::memcmp(original.bytes, recovered.bytes, 0x90000) == 0 &&
        std::memcmp(original.bytes + 0x821a8000u,
                    recovered.bytes + 0x821a8000u, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x8330b000u,
                    recovered.bytes + 0x8330b000u, 0x1000) == 0;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL instance-string %08x mode %u r3 %llx/%llx lr %llx/%llx r31 %llx/%llx requests %zu/%zu\n",
            entry.address, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result.r3),
            static_cast<unsigned long long>(context.lr),
            static_cast<unsigned long long>(result.lr),
            static_cast<unsigned long long>(context.r31.u64),
            static_cast<unsigned long long>(result.r31),
            expected.requests.size(), actual.requests.size());
        for (std::size_t i = 0, shown = 0; i < 0x90000 && shown < 8; ++i)
            if (original.bytes[i] != recovered.bytes[i])
            {
                std::fprintf(stderr, "  memory %08zx %02x/%02x\n", i,
                             original.bytes[i], recovered.bytes[i]);
                ++shown;
            }
    }
    return same;
}
} // namespace

PPC_FUNC(sub_8229C8B0)
{
    (void)base;
    ctx.r3.u64 = registered_metadata_string::InitializeString(
        active->memory, *active, ctx.r3.u64, ctx.r4.u64, ctx.r1.u32);
}
PPC_FUNC(sub_82407300) { __imp__sub_82407300(ctx, base); }

int main()
{
    try
    {
        Window original, recovered;
        unsigned cases = 0;
        for (const Entry& entry : Entries)
        {
            const Mode mode = entry.address == 0x82407300u ?
                Mode::Direct : Mode::Tail;
            if (!Compare(entry, mode, original, recovered)) return 1;
            ++cases;
        }
        if (!Compare(Entries[1], Mode::Null, original, recovered) ||
            !Compare(Entries[0], Mode::SourceAlias, original, recovered) ||
            !Compare(Entries[0], Mode::SaveAlias, original, recovered))
            return 1;
        cases += 3;
        Services untouched(recovered.bytes);
        const initializer::EntryAbi abi{Stack, 0, 0};
        initializer::Result result{0x1234, 0x2345, 0x3456};
        if (initializer::Apply(0x82407304u, untouched.memory, untouched,
                               Object, Source, abi, result) ||
                result.r3 != 0x1234 || result.lr != 0x2345 ||
                result.r31 != 0x3456 || !untouched.requests.empty()) return 1;
        std::printf("PASS instance-string %u PPC comparisons and 1 unknown\n",
                    cases);
        std::puts("LIMIT original two wrapper bodies with proven InitializeString lower model; lower generic ABI frames and volatile register state excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
