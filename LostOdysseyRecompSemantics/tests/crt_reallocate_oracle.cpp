#include "lo_semantics/crt_reallocate.h"
#include "lo_semantics/crt_last_error.h"
#include "lo_semantics/memory_services.h"
#include "lo_semantics/raw_allocation.h"

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
namespace family = lo::semantic::gpu::crt_reallocate;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Heap = 0x20000u;
constexpr GuestAddress Payload = 0x40000u;
constexpr GuestAddress Thread = 0x50000u;
constexpr GuestAddress ThreadData = 0x51000u;
constexpr GuestAddress Error = 0x83215210u;
enum class Mode { NullOversize, FreeZero, Oversize, HeapSuccess,
    FailNoRetry, FailReject, FailRetryOnce };
struct Case { Mode mode; std::uint64_t pointer; std::uint64_t size; };
constexpr Case Cases[] = {
    {Mode::NullOversize, 0, 0x12345678fffff001ull},
    {Mode::FreeZero, 0xaabbccdd00040000ull, 0},
    {Mode::Oversize, 0xaabbccdd00040000ull, 0xfffff001u},
    {Mode::HeapSuccess, 0xaabbccdd00040000ull, 64},
    {Mode::FailNoRetry, 0xaabbccdd00040000ull, 0x80000000u},
    {Mode::FailReject, 0xaabbccdd00040000ull, 0x80000000u},
    {Mode::FailRetryOnce, 0xaabbccdd00040000ull, 0x80000000u},
};
struct Region { GuestAddress start; std::size_t size; };
constexpr Region Regions[] = {
    {0, 0x90000}, {0x83215000u, 0x3000},
    {0x83245000u, 0x2000}, {0x832d3000u, 0x2000},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve CRT guest RAM");
        for (const auto region : Regions)
            if (!VirtualAlloc(bytes + region.start, region.size,
                    MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit CRT guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}
std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32) |
        memory.ReadU32(address + 4u);
}
std::uint64_t Unexpected()
{ throw std::runtime_error("unexpected lower native boundary"); }

struct Event
{
    char kind;
    std::array<std::uint64_t, 4> args;
    bool operator==(const Event&) const = default;
};

struct Services final : CrtAllocationServices, heap_reallocate::Services,
    HeapSegmentServices, heap_block_resize::NativeServices,
    heap_lock_exit::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(Window& window, Mode selected)
        : memory(0, std::span<std::uint8_t>(window.bytes, Space)), mode(selected) {}

    // The bounded cases never enter the older heap growth/VM paths.
    std::uint32_t GetCurrentProcessType() override
    { return static_cast<std::uint32_t>(Unexpected()); }
    void BugCheck(std::uint32_t, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override { (void)Unexpected(); }
    void EnterCriticalSection(GuestAddress) override { (void)Unexpected(); }
    void LeaveCriticalSection(GuestAddress) override { (void)Unexpected(); }
    GuestAddress GrowHeap(GuestAddress, std::uint32_t) override
    { return static_cast<GuestAddress>(Unexpected()); }
    std::int32_t AllocateVirtualMemory(GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t, std::uint32_t) override
    { return static_cast<std::int32_t>(Unexpected()); }
    void RaiseException(GuestAddress) override { (void)Unexpected(); }
    std::int32_t FreeVirtualMemory(GuestAddress base_out,
        GuestAddress size_out, std::uint32_t type, std::uint32_t zero) override
    {
        events.push_back({'F', {base_out, size_out, type, zero}});
        if (mode != Mode::FreeZero || type != 0x8000u || zero != 0u ||
            memory.ReadU32(base_out) != Payload - 48u ||
            memory.ReadU32(size_out) != 0u)
            throw std::runtime_error("unexpected CRT free VM boundary");
        return 0;
    }
    void DecommitFreeBlock(GuestAddress, GuestAddress, std::uint32_t) override
    { (void)Unexpected(); }
    std::uint32_t CompareMemoryUlong(GuestAddress, std::uint32_t,
        std::uint32_t) override { return static_cast<std::uint32_t>(Unexpected()); }
    std::int32_t CommitRange(GuestAddress, GuestAddress, GuestAddress,
        GuestAddress) override { return static_cast<std::int32_t>(Unexpected()); }
    void InitializeUncommittedRange(GuestAddress, GuestAddress,
        std::uint32_t) override { (void)Unexpected(); }
    std::uint64_t heap_block_resize_compare(GuestMemory&, GuestAddress,
        std::uint32_t, std::uint32_t, std::uint64_t,
        heap_block_resize::FrameRegisters&) { return Unexpected(); }
    std::uint64_t CompareMemoryUlong(GuestMemory& guest, GuestAddress source,
        std::uint32_t bytes, std::uint32_t pattern, std::uint64_t sp,
        heap_block_resize::FrameRegisters& frame) override
    { return heap_block_resize_compare(guest, source, bytes, pattern, sp, frame); }
    void LeaveCriticalSection(GuestMemory&,
        heap_lock_exit::Registers&) override { (void)Unexpected(); }

    std::uint64_t OutputErrorMessage(GuestAddress) override
    { return Unexpected(); }
    std::uint64_t BugCheck(std::uint32_t) override { return Unexpected(); }
    std::uint64_t CallNewHandler(GuestAddress handler,
        std::uint64_t requested) override
    {
        events.push_back({'N', {handler, requested}});
        if (handler != 0x7000u ||
            (mode != Mode::FailReject && mode != Mode::FailRetryOnce))
            throw std::runtime_error("unexpected new-handler call");
        if (mode == Mode::FailRetryOnce)
        {
            memory.WriteU32(0x832d3aecu, 0);
            return 1;
        }
        return 0;
    }
    std::uint64_t GetThreadData() override
    { events.push_back({'T', {0}}); return 0; }
    void ReportInvalidParameter() override { (void)Unexpected(); }

    HeapAllocateServices& Allocation() override { return *this; }
    HeapFreeServices& Free() override { return *this; }
    HeapSegmentServices& Segment() override { return *this; }
    heap_block_resize::NativeServices& ResizeNative() override { return *this; }
    heap_lock_exit::NativeServices& LockExitNative() override { return *this; }
    void GetCurrentProcessType(GuestMemory&,
        heap_reallocate::Registers&) override { (void)Unexpected(); }
    void BugCheck(GuestMemory&, heap_reallocate::Registers&) override
    { (void)Unexpected(); }
    void EnterCriticalSection(GuestMemory&,
        heap_reallocate::Registers&) override { (void)Unexpected(); }
    void FreeVirtualMemory(GuestMemory&,
        heap_reallocate::Registers&) override { (void)Unexpected(); }
    void CompareMemoryUlong(GuestMemory&,
        heap_reallocate::Registers&) override { (void)Unexpected(); }
    void RaiseException(GuestMemory&,
        heap_reallocate::Registers&) override { (void)Unexpected(); }
};

Services* active = nullptr;

struct RawAdapter final : RecoveredRawAllocationServices
{
    GuestMemory& memory;
    Services& services;
    GuestAddress frame;
    RawAdapter(GuestMemory& guest, Services& host, GuestAddress nested)
        : RecoveredRawAllocationServices(guest, host), memory(guest),
          services(host), frame(nested) {}
    std::uint64_t AllocateHeap(GuestAddress heap, std::uint32_t flags,
        std::uint64_t bytes) override
    { return AllocateHeapBlock(memory, services, heap, flags,
        static_cast<std::uint32_t>(bytes), frame); }
};

family::Registers FromPpc(const PPCContext& c)
{
    family::Registers r{};
    r.sp=c.r1.u64; r.lr=c.lr; r.ctr=c.ctr.u64;
    r.r3=c.r3.u64; r.r4=c.r4.u64; r.r5=c.r5.u64;
    r.r6=c.r6.u64; r.r7=c.r7.u64; r.r8=c.r8.u64;
    r.r9=c.r9.u64; r.r10=c.r10.u64; r.r11=c.r11.u64;
    r.r12=c.r12.u64; r.r13=c.r13.u64;
    r.r19=c.r19.u64; r.r20=c.r20.u64; r.r21=c.r21.u64;
    r.r22=c.r22.u64; r.r23=c.r23.u64; r.r24=c.r24.u64;
    r.r25=c.r25.u64; r.r26=c.r26.u64; r.r27=c.r27.u64;
    r.r28=c.r28.u64; r.r29=c.r29.u64; r.r30=c.r30.u64;
    r.r31=c.r31.u64; r.xer_so=c.xer.so;
    r.cr0={c.cr0.lt,c.cr0.gt,c.cr0.eq,c.cr0.so};
    r.cr6={c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so};
    return r;
}

void ToPpc(PPCContext& c, const family::Registers& r)
{
    c.r1.u64=r.sp; c.lr=r.lr; c.ctr.u64=r.ctr;
    c.r3.u64=r.r3; c.r4.u64=r.r4; c.r5.u64=r.r5;
    c.r6.u64=r.r6; c.r7.u64=r.r7; c.r8.u64=r.r8;
    c.r9.u64=r.r9; c.r10.u64=r.r10; c.r11.u64=r.r11;
    c.r12.u64=r.r12; c.r13.u64=r.r13;
    c.r19.u64=r.r19; c.r20.u64=r.r20; c.r21.u64=r.r21;
    c.r22.u64=r.r22; c.r23.u64=r.r23; c.r24.u64=r.r24;
    c.r25.u64=r.r25; c.r26.u64=r.r26; c.r27.u64=r.r27;
    c.r28.u64=r.r28; c.r29.u64=r.r29; c.r30.u64=r.r30;
    c.r31.u64=r.r31;
    c.cr0.lt=r.cr0.lt; c.cr0.gt=r.cr0.gt;
    c.cr0.eq=r.cr0.eq; c.cr0.so=r.cr0.so;
    c.cr6.lt=r.cr6.lt; c.cr6.gt=r.cr6.gt;
    c.cr6.eq=r.cr6.eq; c.cr6.so=r.cr6.so;
}

void Seed(Window& window, Mode mode)
{
    for (auto region : Regions)
        std::memset(window.bytes + region.start, 0, region.size);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x83245708u, Heap);
    memory.WriteU32(Heap + 24u, 1u);
    memory.WriteU32(Heap + 84u, 0xffffffffu);
    memory.WriteU32(0x832d3aecu,
        mode == Mode::FailReject || mode == Mode::FailRetryOnce ? 1u : 0u);
    memory.WriteU32(0x832d3ae8u,
        mode == Mode::FailReject || mode == Mode::FailRetryOnce ? 0x7003u : 0u);
    memory.WriteU32(Thread + 256u, ThreadData);
    memory.WriteU32(ThreadData + 352u, 0x345u);
    memory.WriteU32(0x832150a8u, 0x345u);
    memory.WriteU32(0x832150acu, 77u);
    if (mode == Mode::FreeZero)
    {
        memory.WriteU8(Payload - 16u + 5u, 8u);
        memory.WriteU32(Payload - 48u, Payload - 48u);
        memory.WriteU32(Payload - 44u, Payload - 48u);
    }
}

PPCContext Initial(const Case& item)
{
    PPCContext c{};
    c.r1.u64=Stack; c.lr=0x1122334455667788ull;
    c.r3.u64=item.pointer; c.r4.u64=item.size;
    c.r13.u64=0x99aabbcc00050000ull;
    c.r27.u64=0x1111222233330027ull;
    c.r28.u64=0x1111222233330028ull;
    c.r29.u64=0x1111222233330029ull;
    c.r30.u64=0x1111222233330030ull;
    c.r31.u64=0x1111222233330031ull;
    c.xer.so=1;
    return c;
}

bool SameMemory(const Window& a, const Window& b)
{
    for (auto region : Regions)
        if (std::memcmp(a.bytes + region.start, b.bytes + region.start,
                region.size) != 0) return false;
    return true;
}

bool Check(const Case& item)
{
    Window original, recovered;
    Seed(original,item.mode); Seed(recovered,item.mode);
    Services expected(original,item.mode), actual(recovered,item.mode);
    PPCContext raw=Initial(item);
    PPCContext initial=raw;
    active=&expected;
    __imp__sub_823ACAD8(raw, original.bytes);
    auto translated=FromPpc(initial);
    if (!family::Apply(0x823acAD8u, actual.memory, actual, actual,
            translated)) throw std::runtime_error("CRT realloc entry missing");
    const std::uint64_t expected_result =
        item.mode == Mode::HeapSuccess ? Payload : 0u;
    const std::uint32_t expected_error =
        item.mode == Mode::NullOversize || item.mode == Mode::Oversize ? 12u :
        item.mode == Mode::FailNoRetry || item.mode == Mode::FailReject ||
        item.mode == Mode::FailRetryOnce ? 77u : 0u;
    const bool same=raw.r3.u64==translated.r3 &&
        raw.r3.u64==expected_result &&
        raw.r1.u64==translated.sp && raw.lr==translated.lr &&
        raw.r27.u64==translated.r27 && raw.r28.u64==translated.r28 &&
        raw.r29.u64==translated.r29 && raw.r30.u64==translated.r30 &&
        raw.r31.u64==translated.r31 &&
        expected.events==actual.events && SameMemory(original,recovered) &&
        actual.memory.ReadU32(Error)==expected_error;
    if (!same)
        std::fprintf(stderr,"FAIL crt-reallocate mode=%u r3=%llx/%llx "
            "SP=%llx/%llx LR=%llx/%llx events=%zu/%zu\n",
            static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(translated.r3),
            static_cast<unsigned long long>(raw.r1.u64),
            static_cast<unsigned long long>(translated.sp),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(translated.lr),
            expected.events.size(),actual.events.size());
    return same;
}
} // namespace

void OriginalRaw(PPCContext& c, std::uint8_t*)
{
    GuestMemory& m=active->memory;
    const auto sp=c.r1.u64;
    WriteU64(m,static_cast<GuestAddress>(sp-40u),c.r28.u64);
    WriteU64(m,static_cast<GuestAddress>(sp-32u),c.r29.u64);
    WriteU64(m,static_cast<GuestAddress>(sp-24u),c.r30.u64);
    WriteU64(m,static_cast<GuestAddress>(sp-16u),c.r31.u64);
    m.WriteU32(static_cast<GuestAddress>(sp-8u),static_cast<std::uint32_t>(c.lr));
    c.r1.u64=sp-128u;
    m.WriteU32(static_cast<GuestAddress>(c.r1.u64),static_cast<std::uint32_t>(sp));
    RawAdapter adapter(m,*active,static_cast<GuestAddress>(c.r1.u64-320u));
    c.r3.u64=AllocateRawMemory(m,adapter,c.r3.u64);
    c.r1.u64=sp;
    c.r28.u64=ReadU64(m,static_cast<GuestAddress>(sp-40u));
    c.r29.u64=ReadU64(m,static_cast<GuestAddress>(sp-32u));
    c.r30.u64=ReadU64(m,static_cast<GuestAddress>(sp-24u));
    c.r31.u64=ReadU64(m,static_cast<GuestAddress>(sp-16u));
    c.r12.u64=m.ReadU32(static_cast<GuestAddress>(sp-8u));
    c.lr=c.r12.u64;
}

void OriginalFree(PPCContext& c, std::uint8_t*)
{
    GuestMemory& m=active->memory;
    const auto sp=c.r1.u64;
    m.WriteU32(static_cast<GuestAddress>(sp-8u),static_cast<std::uint32_t>(c.lr));
    WriteU64(m,static_cast<GuestAddress>(sp-16u),c.r31.u64);
    c.r1.u64=sp-96u;
    m.WriteU32(static_cast<GuestAddress>(c.r1.u64),static_cast<std::uint32_t>(sp));
    c.r3.u64=FreeCrtRecord(m,*active,c.r3.u64,
        static_cast<GuestAddress>(c.r13.u64),static_cast<GuestAddress>(sp));
    c.r1.u64=sp;
    c.r12.u64=m.ReadU32(static_cast<GuestAddress>(sp-8u));
    c.lr=c.r12.u64;
    c.r31.u64=ReadU64(m,static_cast<GuestAddress>(sp-16u));
}

void OriginalGetHeap(PPCContext& c, std::uint8_t*)
{ c.r3.u64=GetProcessHeap(active->memory); }

void OriginalHeap(PPCContext& c, std::uint8_t*)
{
    auto regs=FromPpc(c);
    if (!heap_reallocate::Apply(0x827ccf80u,active->memory,*active,regs))
        throw std::runtime_error("missing recovered heap realloc");
    ToPpc(c,regs);
}

void OriginalNewHandler(PPCContext& c, std::uint8_t*)
{
    GuestMemory& m=active->memory;
    const auto sp=c.r1.u64;
    m.WriteU32(static_cast<GuestAddress>(sp-8u),static_cast<std::uint32_t>(c.lr));
    c.r1.u64=sp-96u;
    m.WriteU32(static_cast<GuestAddress>(c.r1.u64),static_cast<std::uint32_t>(sp));
    c.r3.u64=InvokeNewHandler(m,*active,c.r3.u64);
    c.r1.u64=sp;
    c.r12.u64=m.ReadU32(static_cast<GuestAddress>(sp-8u));
    c.lr=c.r12.u64;
}

void OriginalErrorAddress(PPCContext& c, std::uint8_t*)
{
    GuestMemory& m=active->memory;
    const auto sp=c.r1.u64;
    m.WriteU32(static_cast<GuestAddress>(sp-8u),static_cast<std::uint32_t>(c.lr));
    c.r1.u64=sp-96u;
    m.WriteU32(static_cast<GuestAddress>(c.r1.u64),static_cast<std::uint32_t>(sp));
    c.r3.u64=GetAllocationErrorAddress(*active);
    c.r1.u64=sp;
    c.r12.u64=m.ReadU32(static_cast<GuestAddress>(sp-8u));
    c.lr=c.r12.u64;
}

void OriginalLastError(PPCContext& c, std::uint8_t*)
{
    crt_last_error::Registers r{c.r3.u64,c.r11.u64,c.r13.u64,c.xer.so,
        {c.cr6.lt,c.cr6.gt,c.cr6.eq,c.cr6.so}};
    if (!crt_last_error::Apply(0x822ca100u,active->memory,r))
        throw std::runtime_error("missing recovered last-error getter");
    c.r3.u64=r.r3; c.r11.u64=r.r11;
    c.cr6.lt=r.cr6.lt; c.cr6.gt=r.cr6.gt;
    c.cr6.eq=r.cr6.eq; c.cr6.so=r.cr6.so;
}

void OriginalTranslate(PPCContext& c, std::uint8_t*)
{ c.r3.u64=TranslateCrtError(active->memory,c.r3.u32); }

int main()
{
    try
    {
        for (const auto& item : Cases) if (!Check(item)) return 1;
        Window window;
        Services services(window,Mode::FreeZero);
        auto regs=FromPpc(Initial(Cases[1]));
        const auto saved=regs;
        if (family::Apply(0xffffffffu,services.memory,services,services,regs) ||
            std::memcmp(&regs,&saved,sizeof(regs)) != 0 ||
            !services.events.empty())
            throw std::runtime_error("unknown CRT realloc changed state");
        std::puts("PASS crt-reallocate 7 original PPC cases + unknown");
        std::puts("LIMIT actual CRT body and own frame; accepted direct lower models with bounded generic ABI/native contracts; no fault/MMIO/concurrency/runtime proof");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr,"%s\n",error.what()); return 1; }
}
