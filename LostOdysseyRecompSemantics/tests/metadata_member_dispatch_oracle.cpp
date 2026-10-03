#include "lo_semantics/metadata_member_dispatch.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace family = metadata_member_dispatch;
constexpr std::size_t Space = std::size_t{1} << 32u;
constexpr std::size_t Mapped = 0x90000u;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Type = 0x10000u;
constexpr GuestAddress First = 0x20000u;
constexpr GuestAddress Second = 0x21000u;
constexpr GuestAddress Redirect = 0x22000u;
constexpr GuestAddress Third = 0x23000u;
constexpr GuestAddress Vtable = 0x30000u;
constexpr GuestAddress Method = 0x4237u;
enum class Mode { Empty, Skipped, ChainAlias };

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, Mapped, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve member guest RAM");
        std::memset(bytes, 0, Mapped);
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
void Write64(GuestMemory& memory, GuestAddress where, std::uint64_t value)
{
    memory.WriteU32(where, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(where + 4u, static_cast<std::uint32_t>(value));
}
struct Services final : family::Services
{
    GuestMemory memory;
    std::vector<std::array<std::uint64_t, 6>> events;
    explicit Services(std::uint8_t* bytes)
        : memory(0, std::span<std::uint8_t>(bytes, Space)) {}
    std::uint64_t CallMember(GuestAddress method, std::uint64_t member,
        std::uint64_t destination, std::uint64_t sp,
        family::FrameRegisters& frame) override
    {
        events.push_back({method, member, destination, sp, frame.lr, frame.ctr});
        if (method != (Method & ~3u) || sp != Stack - 112u ||
            frame.lr != 0x823fd458u || frame.ctr != Method)
            throw std::runtime_error("wrong member method arguments");
        if (events.size() == 1u)
        {
            if (member != Second || destination != 0xaabbccdd00040010ull)
                throw std::runtime_error("wrong first member receiver");
            frame.r31 = 0xfedcba9800000000ull | Redirect;
            frame.r30 = 0x1122334400041000ull;
            memory.WriteU32(Redirect + 112u, Third);
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 8u, 0xcafebabeu);
            Write64(memory, static_cast<GuestAddress>(Stack) - 24u,
                0xaabbccdd00000030ull);
            Write64(memory, static_cast<GuestAddress>(Stack) - 16u,
                0xaabbccdd00000031ull);
        }
        else if (events.size() != 2u || member != Third ||
                 destination != 0x1122334400041020ull)
            throw std::runtime_error("wrong redirected member receiver");
        return 0x8877665500000000ull | events.size();
    }
};
Services* active = nullptr;

void Initialize(Services& services, Mode mode)
{
    auto& memory = services.memory;
    memory.WriteU32(Type + 120u, mode == Mode::Empty ? 0u : First);
    Write64(memory, First + 8u, 1ull << 41u);
    if (mode == Mode::ChainAlias)
    {
        memory.WriteU32(First + 112u, Second);
        memory.WriteU32(Second, Vtable);
        memory.WriteU32(Third, Vtable);
        memory.WriteU32(Second + 100u, 16u);
        memory.WriteU32(Third + 100u, 32u);
        memory.WriteU32(Vtable + 340u, Method);
    }
}

bool Compare(Mode mode)
{
    Window original, recovered;
    Services expected(original.bytes), actual(recovered.bytes);
    Initialize(expected, mode);
    Initialize(actual, mode);
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xaabbccdd00040000ull;
    raw.r4.u64 = 0x8877665500000000ull | Type;
    raw.lr = 0x9988776655443322ull;
    raw.r30.u64 = 0xabcdef0000000030ull;
    raw.r31.u64 = 0xabcdef0000000031ull;
    raw.ctr.u64 = 0xabcdef0000000042ull;
    const PPCContext initial = raw;
    active = &expected;
    __imp__sub_823FD400(raw, original.bytes);
    family::FrameRegisters frame{initial.lr, initial.r30.u64,
        initial.r31.u64, initial.ctr.u64};
    std::uint64_t result = 0;
    if (!family::Apply(0x823fd400u, actual.memory, actual,
            initial.r3.u64, initial.r4.u64, Stack, frame, result))
        throw std::runtime_error("member dispatch entry missing");
    if (std::memcmp(original.bytes, recovered.bytes, Mapped) != 0 ||
        expected.events != actual.events || raw.r3.u64 != result ||
        raw.lr != frame.lr || raw.r30.u64 != frame.r30 ||
        raw.r31.u64 != frame.r31 || raw.ctr.u64 != frame.ctr ||
        raw.r1.u64 != Stack)
    {
        std::fprintf(stderr, "FAIL member dispatch mode=%u result=%llx/%llx\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result));
        return false;
    }
    return true;
}
} // namespace

void OriginalMemberCall(PPCContext& ctx, std::uint8_t*, std::uint32_t target)
{
    family::FrameRegisters frame{ctx.lr, ctx.r30.u64, ctx.r31.u64, ctx.ctr.u64};
    ctx.r3.u64 = active->CallMember(target, ctx.r3.u64, ctx.r4.u64,
        ctx.r1.u64, frame);
    ctx.lr = frame.lr;
    ctx.r30.u64 = frame.r30;
    ctx.r31.u64 = frame.r31;
    ctx.ctr.u64 = frame.ctr;
}

int main()
{
    try
    {
        for (Mode mode : {Mode::Empty, Mode::Skipped, Mode::ChainAlias})
            if (!Compare(mode)) return 1;
        Window spare;
        Services service(spare.bytes);
        family::FrameRegisters frame{1, 2, 3, 4};
        std::uint64_t result = 5;
        if (family::Apply(0xffffffffu, service.memory, service, 6, 7,
                Stack, frame, result) || result != 5 || frame.lr != 1 ||
            frame.r30 != 2 || frame.r31 != 3 || frame.ctr != 4 ||
            !service.events.empty() || spare.bytes[Type] != 0)
            throw std::runtime_error("unknown member dispatch changed state");
        std::puts("PASS metadata-member-dispatch 3 original PPC cases + unknown");
        std::puts("LIMIT dynamic method internals/generic volatile ABI/MMIO/concurrency external");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
