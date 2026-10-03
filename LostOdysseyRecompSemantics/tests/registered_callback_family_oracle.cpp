#include "lo_semantics/registered_callback_family.h"
#include "lo_semantics/registered_constructor_family.h"
#include "lo_semantics/registered_getter_family.h"
#include "lo_semantics/object_registration.h"
#include "lo_semantics/object_startup.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace callback = lo::semantic::gpu::registered_callback_family;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Stack = 0x5f000u;
constexpr GuestAddress Self = 0x10000u;
constexpr GuestAddress Parent = 0x20000u;
constexpr GuestAddress Meta = 0x24000u;
constexpr GuestAddress Primary = 0x28000u;
constexpr GuestAddress Allocated = 0x30000u;
constexpr GuestAddress Manager = 0x38000u;
constexpr GuestAddress ManagerVtable = 0x38100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr GuestAddress ReadyMethod = 0x823456a0u;
constexpr GuestAddress SelfVtable = 0x12000u;
constexpr GuestAddress ReadyGate = 0x83315ed8u;
constexpr GuestAddress PrimaryGlobal = 0x83315f9cu;
constexpr std::uint64_t InitialR3 = 0xabcddcba00007777ull;
constexpr std::uint64_t AllocatedR3 = 0x1234567800030000ull;

enum class Mode { Equal, Different, Ready, MetaMissing, ParentMissing,
    GateAlias };
struct Entry
{
    GuestAddress address, self_global, parent_getter, parent_singleton,
        parent_global, parent_registration, meta_getter, meta_singleton,
        meta_global;
    unsigned frame_size, shape;
    PPCFunc* original;
};
constexpr Entry Entries[] = {
/* ENTRY_TABLE */
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x70000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83247000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83315000u, 0x5000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 4> arguments;
    bool operator==(const Event&) const = default;
};

struct Services;
extern Services* active;
struct Services final : ManagerFacadeServices, callback::Services,
    registered_constructor_family::RegistrationServices,
    ObjectRegistrationServices
{
    GuestMemory memory;
    std::uint8_t* bytes;
    const Entry& entry;
    Mode mode;
    bool original_side;
    std::vector<Event> events;
    Services(std::uint8_t* window, const Entry& selected, Mode scenario,
        bool original)
        : memory(0, std::span<std::uint8_t>(window, Space)), bytes(window),
          entry(selected), mode(scenario), original_side(original) {}

    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected release"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected resize"); }
    std::uint64_t AllocateStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t bytes, std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment}});
        if (method != AllocateMethod || manager != Manager || alignment != 8)
            throw std::runtime_error("unexpected allocation arguments");
        return AllocatedR3;
    }
    std::uint64_t Register(GuestAddress target, std::uint64_t r3,
        GuestAddress caller_sp) override
    {
        if (target == 0x82403200u)
        {
            if (original_side)
            {
                PPCContext nested{};
                nested.r1.u64 = caller_sp;
                nested.r3.u64 = r3;
                nested.lr = 0x82200000u;
                __imp__sub_82403200(nested, bytes);
                return nested.r3.u64;
            }
            return callback::RegisterSharedMetadataObject(memory, *this,
                *this, r3, caller_sp);
        }
        std::uint64_t result = 0;
        if (callback::LinkRegisteredObject(target, memory, *this, *this,
                r3, caller_sp, result)) return result;
        return CallExternalRegistration(target, r3, caller_sp);
    }
    std::uint64_t CallExternalGetter(GuestAddress target, std::uint64_t r3,
        GuestAddress caller_sp) override
    {
        events.push_back({'G', {target, r3, caller_sp, 0}});
        if (target != entry.parent_getter)
        {
            std::fprintf(stderr, "unexpected getter target %08x in %08x mode %u\n",
                target, entry.address, static_cast<unsigned>(mode));
            throw std::runtime_error("unexpected external getter");
        }
        return mode == Mode::Equal ? Self : Parent;
    }
    std::uint64_t CallExternalConstructor(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unexpected external constructor"); }
    std::uint64_t CallExternalRegistration(GuestAddress target,
        std::uint64_t r3, GuestAddress caller_sp) override
    {
        events.push_back({'R', {target, r3, caller_sp, 0}});
        if (mode == Mode::ParentMissing && target == entry.parent_registration)
            memory.WriteU32(entry.parent_global, Parent);
        return 0x8765432100000000ull | static_cast<std::uint64_t>(r3 & 0xffffffffu);
    }
    std::uint64_t RegisterSecondary(std::uint64_t r3,
        GuestAddress caller_sp) override
    {
        events.push_back({'S', {r3, caller_sp, 0, 0}});
        return 0x4567000000000000ull | (r3 & 0xffffffffu);
    }
    std::uint64_t CallReadyMethod(GuestAddress method, std::uint64_t receiver,
        GuestAddress caller_sp) override
    {
        events.push_back({'V', {method, receiver, caller_sp, 0}});
        return 0x76543210abcdef01ull;
    }
    std::uint64_t GetPrimaryObject() override
    { throw std::runtime_error("unexpected graph primary getter"); }
    std::uint64_t CreateSecondary(std::uint64_t) override
    { throw std::runtime_error("unexpected graph secondary creation"); }
    std::uint64_t RegisterSecondary(std::uint64_t r3) override
    { return RegisterSecondary(r3, 0); }
    std::uint64_t CallReadyMethod(GuestAddress method,
        std::uint64_t receiver) override
    { return CallReadyMethod(method, receiver, 0); }
};

Services* active = nullptr;
struct SavedGprs
{
    unsigned first;
    std::array<std::uint64_t, 7> registers;
    std::uint64_t return_address;
};
std::vector<SavedGprs> saved_gprs;
void SaveGprs(PPCContext& ctx, unsigned first)
{
    saved_gprs.push_back({first, {ctx.r25.u64, ctx.r26.u64,
        ctx.r27.u64, ctx.r28.u64, ctx.r29.u64, ctx.r30.u64,
        ctx.r31.u64}, ctx.r12.u64});
}
void RestoreGprs(PPCContext& ctx, unsigned first)
{
    if (saved_gprs.empty() || saved_gprs.back().first != first)
        throw std::runtime_error("unmatched GPR restore");
    const auto saved = saved_gprs.back();
    saved_gprs.pop_back();
    PPCRegister* registers[] = {&ctx.r25, &ctx.r26, &ctx.r27,
        &ctx.r28, &ctx.r29, &ctx.r30, &ctx.r31};
    for (unsigned number = first; number <= 31; ++number)
        registers[number - 25]->u64 = saved.registers[number - 25];
    ctx.lr = saved.return_address;
}

void Initialize(std::uint8_t* bytes, const Entry& entry, Mode mode)
{
    std::memset(bytes, 0xbd, 0x70000);
    std::memset(bytes + 0x83247000u, 0, 0x1000);
    std::memset(bytes + 0x8330b000u, 0, 0x1000);
    std::memset(bytes + 0x83315000u, 0, 0x5000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4, AllocateMethod | 3u);
    memory.WriteU32(0x83315ef0u, 0x34000u);
    const GuestAddress self = mode == Mode::GateAlias ? ReadyGate - 52u : Self;
    memory.WriteU32(entry.self_global, self);
    const GuestAddress parent = mode == Mode::Equal ? Self : Parent;
    if (entry.parent_singleton) memory.WriteU32(entry.parent_singleton, parent);
    if (entry.parent_global) memory.WriteU32(entry.parent_global,
        mode == Mode::ParentMissing ? 0u : parent);
    if (entry.meta_singleton) memory.WriteU32(entry.meta_singleton, Meta);
    if (entry.meta_global) memory.WriteU32(entry.meta_global,
        mode == Mode::MetaMissing ? 0u : Meta);
    if (entry.parent_global && entry.parent_global == entry.meta_global &&
        mode == Mode::Equal)
        memory.WriteU32(entry.parent_global, Self);
    memory.WriteU32(PrimaryGlobal, Primary);
    memory.WriteU32(ReadyGate, mode == Mode::Ready ? 1u : 0u);
    memory.WriteU32(Self, SelfVtable);
    if (mode == Mode::GateAlias) memory.WriteU32(self, SelfVtable);
    memory.WriteU32(Meta, SelfVtable);
    memory.WriteU32(SelfVtable + 124u, ReadyMethod | 3u);
}

bool CompareOne(const Entry& entry, Mode mode, unsigned ordinal,
    Window& original, Window& recovered)
{
    Initialize(original.bytes, entry, mode);
    Initialize(recovered.bytes, entry, mode);
    Services expected(original.bytes, entry, mode, true);
    Services actual(recovered.bytes, entry, mode, false);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x82200000u;
    context.r3.u64 = InitialR3;
    context.r28.u64 = 0x1111222233334444ull;
    context.r29.u64 = 0x2222333344445555ull;
    context.r30.u64 = 0x3333444455556666ull;
    context.r31.u64 = 0x4444555566667777ull;
    saved_gprs.clear();
    active = &expected;
    entry.original(context, original.bytes);
    active = &actual;
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!callback::LinkRegisteredObject(entry.address, actual.memory,
            actual, actual, InitialR3, Stack, result))
        throw std::runtime_error("missing callback mapping");
    bool same = result == context.r3.u64 && context.r1.u64 == Stack &&
        context.lr == 0x82200000u && saved_gprs.empty() &&
        context.r28.u64 == 0x1111222233334444ull &&
        context.r29.u64 == 0x2222333344445555ull &&
        context.r30.u64 == 0x3333444455556666ull &&
        context.r31.u64 == 0x4444555566667777ull &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000u) == 0 &&
        std::memcmp(original.bytes + 0x83315000u,
            recovered.bytes + 0x83315000u, 0x5000) == 0 &&
        std::memcmp(original.bytes + 0x83247000u,
            recovered.bytes + 0x83247000u, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x8330b000u,
            recovered.bytes + 0x8330b000u, 0x1000) == 0;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL callback %08x case %u mode %u r3 %llx/%llx events %zu/%zu fields +60 %08x/%08x +196 %08x/%08x +52 %08x/%08x\n",
            entry.address, ordinal, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size(),
            expected.memory.ReadU32(Self + 60), actual.memory.ReadU32(Self + 60),
            expected.memory.ReadU32(Self + 196), actual.memory.ReadU32(Self + 196),
            expected.memory.ReadU32(Self + 52), actual.memory.ReadU32(Self + 52));
        return false;
    }
    return true;
}

bool CompareShared(Mode mode, Window& original, Window& recovered)
{
    const Entry& entry = Entries[0];
    Initialize(original.bytes, entry, mode);
    Initialize(recovered.bytes, entry, mode);
    Services expected(original.bytes, entry, mode, true);
    Services actual(recovered.bytes, entry, mode, false);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x82200000u;
    context.r3.u64 = InitialR3;
    context.r29.u64 = 0x2222333344445555ull;
    context.r30.u64 = 0x3333444455556666ull;
    context.r31.u64 = 0x4444555566667777ull;
    saved_gprs.clear();
    active = &expected;
    __imp__sub_82403200(context, original.bytes);
    active = &actual;
    const auto result = callback::RegisterSharedMetadataObject(actual.memory,
        actual, actual, InitialR3, Stack);
    const bool same = result == context.r3.u64 && context.r1.u64 == Stack &&
        context.lr == 0x82200000u && saved_gprs.empty() &&
        context.r29.u64 == 0x2222333344445555ull &&
        context.r30.u64 == 0x3333444455556666ull &&
        context.r31.u64 == 0x4444555566667777ull &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000u) == 0 &&
        std::memcmp(original.bytes + 0x83315000u,
            recovered.bytes + 0x83315000u, 0x5000) == 0;
    if (!same)
        std::fprintf(stderr, "FAIL shared metadata mode %u r3 %llx/%llx events %zu/%zu\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size());
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_28) { (void)base; SaveGprs(ctx, 28); }
PPC_FUNC(__restgprlr_28) { (void)base; RestoreGprs(ctx, 28); }
PPC_FUNC(__savegprlr_29) { (void)base; SaveGprs(ctx, 29); }
PPC_FUNC(__restgprlr_29) { (void)base; RestoreGprs(ctx, 29); }

void OriginalDirectCall(PPCContext& ctx, std::uint8_t* base,
    std::uint32_t target)
{
    (void)base;
    std::uint64_t result = 0;
    if (registered_constructor_family::Apply(target, active->memory,
            *active, *active, ctx.r3.u64, ctx.r1.u32, result))
        ctx.r3.u64 = result;
    else if (registered_getter_family::Apply(target, active->memory,
            *active, *active, ctx.r3.u64, ctx.r1.u32, result))
        ctx.r3.u64 = result;
    else if (target == 0x82410b90u)
        ctx.r3.u64 = ConstructRegisteredObject(active->memory, *active,
            ctx.r3.u64, ctx.r1.u32);
    else if (target == 0x82403200u)
        ctx.r3.u64 = callback::RegisterSharedMetadataObject(active->memory,
            *active, *active, ctx.r3.u64, ctx.r1.u32);
    else if (target == 0x82406b00u)
        ctx.r3.u64 = GetPrimaryRegisteredObject(active->memory, *active,
            *active, ctx.r1.u32);
    else if (target == 0x82410c48u)
        ctx.r3.u64 = RegisterObjectGraph(active->memory, *active);
    else if (target == active->entry.parent_registration)
        ctx.r3.u64 = active->Register(target, ctx.r3.u64,
            ctx.r1.u32);
    else
        ctx.r3.u64 = active->CallExternalGetter(target, ctx.r3.u64,
            ctx.r1.u32);
}

void OriginalIndirectCall(PPCContext& ctx, std::uint8_t* base,
    std::uint32_t method)
{
    (void)base;
    ctx.r3.u64 = active->CallReadyMethod(method, ctx.r3.u64, ctx.r1.u32);
}

int main()
{
    try
    {
        Window original, recovered;
        unsigned cases = 0;
        std::array<bool, 11> ready_shapes{};
        const Entry* lazy_parent = nullptr;
        const Entry* lazy_meta = nullptr;
        for (const Entry& entry : Entries)
        {
            if (!CompareOne(entry, Mode::Equal, cases++, original, recovered) ||
                !CompareOne(entry, Mode::Different, cases++, original, recovered))
                return 1;
            if (!ready_shapes[entry.shape] || entry.address == 0x8240f7d0u)
            {
                ready_shapes[entry.shape] = true;
                if (!CompareOne(entry, Mode::Ready, cases++, original, recovered))
                    return 1;
            }
            if (entry.address == 0x8249c688u) lazy_parent = &entry;
            if (entry.address == 0x824071e8u) lazy_meta = &entry;
        }
        if (std::size(Entries) == 708)
        {
            if (!lazy_parent || !lazy_meta ||
                !CompareOne(*lazy_parent, Mode::ParentMissing, cases++, original, recovered) ||
                !CompareOne(*lazy_meta, Mode::MetaMissing, cases++, original, recovered) ||
                !CompareShared(Mode::Equal, original, recovered) ||
                !CompareShared(Mode::Ready, original, recovered))
                return 1;
            cases += 2;
        }
        if (std::size(Entries) == 51 &&
            !CompareOne(Entries[0], Mode::GateAlias, cases++, original, recovered))
            return 1;
        if (std::size(Entries) == 4 &&
            (!lazy_parent || !CompareOne(*lazy_parent,
                Mode::ParentMissing, cases++, original, recovered)))
            return 1;
        std::printf("PASS registered-callback %zu entries %u cases\n",
            std::size(Entries), cases);
        std::puts("LIMIT original PPC callback bodies; recovered constructor/get-primary stand-ins; external getter/registration and ABI scratch are explicit boundaries");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
