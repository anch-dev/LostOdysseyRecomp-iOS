#include "lo_semantics/heap_lock_exit.h"

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
namespace family = lo::semantic::gpu::heap_lock_exit;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Object = 0x20000u;
enum class Mode { Skip, Call, Redirect, MarkerAlias };
constexpr Mode Cases[] = {Mode::Skip, Mode::Call, Mode::Redirect,
    Mode::MarkerAlias};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve heap-lock guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

using Event = std::array<std::uint64_t, 8>;
struct Services final : family::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(Window& window, Mode selected)
        : memory(0, std::span<std::uint8_t>(window.bytes, Space)), mode(selected) {}

    void LeaveCriticalSection(GuestMemory& guest,
        family::Registers& registers) override
    {
        events.push_back({registers.sp, registers.lr, registers.r3,
            registers.r11, registers.r12, registers.r22, registers.r31,
            registers.cr6.gt});
        if (&guest != &memory || mode == Mode::Skip ||
            registers.sp != Stack - 112u ||
            registers.lr != 0x827cd7e8u ||
            registers.r3 != 0x1234abcdu || registers.cr6.gt != 1u)
            throw std::runtime_error("incorrect native leave boundary");
        registers.r3 = 0xaabbccdd77665544ull;
        registers.r11 = 0x8877665544332211ull;
        if (mode == Mode::Redirect)
        {
            // The PPC epilogue must follow this live SP, then the changed
            // backchain and saved slots at the redirected caller frame.
            registers.sp = Stack - 0x100u;
            memory.WriteU32(static_cast<GuestAddress>(registers.sp), 0x76000u);
            WriteU64(memory, 0x76000u - 8u, 0x1122334455667788ull);
            WriteU64(memory, 0x76000u - 16u, 0x99aabbccddeeff00ull);
            memory.WriteU32(0x76000u - 24u, 0xface1234u);
            registers.r22 = 0;
            registers.r31 = 0;
            registers.r12 = 0;
            registers.lr = 0;
        }
    }
};

Services* active = nullptr;

family::Registers FromPpc(const PPCContext& context)
{
    return {context.r1.u64, context.lr, context.r3.u64, context.r11.u64,
        context.r12.u64, context.r22.u64, context.r31.u64, context.xer.so,
        {context.cr6.lt, context.cr6.gt, context.cr6.eq, context.cr6.so}};
}

void ToPpc(PPCContext& context, const family::Registers& registers)
{
    context.r1.u64 = registers.sp;
    context.lr = registers.lr;
    context.r3.u64 = registers.r3;
    context.r11.u64 = registers.r11;
    context.r12.u64 = registers.r12;
    context.r22.u64 = registers.r22;
    context.r31.u64 = registers.r31;
    context.cr6.lt = registers.cr6.lt;
    context.cr6.gt = registers.cr6.gt;
    context.cr6.eq = registers.cr6.eq;
    context.cr6.so = registers.cr6.so;
}

void Seed(Window& window, Mode mode)
{
    std::memset(window.bytes, 0, 0x90000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(Object + 1408u, 0x1234abcdu);
    if (mode == Mode::Call || mode == Mode::Redirect)
        memory.WriteU32(0x30000u + 96u, 7u);
}

PPCContext Initial(Mode mode)
{
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x123456789abcdef0ull;
    context.r3.u64 = 0x5566778899aabbccull;
    context.r11.u64 = 0xabcdef0123456789ull;
    context.r12.u64 = mode == Mode::MarkerAlias ? Stack + 216u :
        0xaabbccdd00030140ull;
    context.r22.u64 = 0x2233445500020000ull;
    context.r31.u64 = 0x1122334400000031ull;
    context.xer.so = 1;
    context.cr6 = {1, 0, 1, {0}};
    return context;
}

bool Check(Mode mode)
{
    Window original, recovered;
    Seed(original, mode); Seed(recovered, mode);
    Services expected(original, mode), actual(recovered, mode);
    PPCContext raw = Initial(mode);
    PPCContext initial = raw;
    active = &expected;
    __imp__sub_827CD7BC(raw, original.bytes);
    auto registers = FromPpc(initial);
    if (!family::Apply(0x827cd7bcu, actual.memory, actual, registers))
        throw std::runtime_error("heap-lock exit entry missing");
    PPCContext translated = initial;
    ToPpc(translated, registers);
    const bool same = std::memcmp(&raw, &translated, sizeof(PPCContext)) == 0 &&
        std::memcmp(original.bytes, recovered.bytes, 0x90000u) == 0 &&
        expected.events == actual.events &&
        (mode == Mode::Skip ? expected.events.empty() :
            expected.events.size() == 1u) &&
        (mode == Mode::Skip ? raw.r3.u64 == initial.r3.u64 :
            raw.r3.u64 == 0xaabbccdd77665544ull) &&
        raw.r1.u64 == (mode == Mode::Redirect ? 0x76000u :
            static_cast<GuestAddress>(Stack));
    if (!same)
        std::fprintf(stderr,
            "FAIL heap-lock-exit mode=%u r3=%llx/%llx SP=%llx/%llx "
            "LR=%llx/%llx events=%zu/%zu\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(translated.r3.u64),
            static_cast<unsigned long long>(raw.r1.u64),
            static_cast<unsigned long long>(translated.r1.u64),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(translated.lr),
            expected.events.size(), actual.events.size());
    return same;
}
} // namespace

void OriginalLeave(PPCContext& context, std::uint8_t*)
{
    auto registers = FromPpc(context);
    active->LeaveCriticalSection(active->memory, registers);
    ToPpc(context, registers);
}

int main()
{
    try
    {
        for (Mode mode : Cases) if (!Check(mode)) return 1;
        Window window;
        Services services(window, Mode::Skip);
        family::Registers registers = FromPpc(Initial(Mode::Skip));
        const auto saved = registers;
        if (family::Apply(0xffffffffu, services.memory, services, registers) ||
            std::memcmp(&saved, &registers, sizeof(saved)) != 0 ||
            !services.events.empty())
            throw std::runtime_error("unknown address changed state");
        std::puts("PASS heap-lock-exit 4 original PPC cases + unknown");
        std::puts("LIMIT selected registers, own frame and ordinary RAM; native return/memory effects are explicit; other volatile ABI, faults, MMIO and concurrency unverified");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
