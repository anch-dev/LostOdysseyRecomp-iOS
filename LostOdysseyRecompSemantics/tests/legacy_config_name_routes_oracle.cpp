// Appended after both complete 124-instruction routes and their reachable
// accepted name parsing, hashing, comparison and token lookup PPC bodies.
#include "lo_semantics/legacy_config_name_routes.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_config_name_routes;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Text = 0x30000u;
constexpr GuestAddress Object = 0x50000u;
constexpr GuestAddress Record = 0x60000u;
constexpr GuestAddress BucketArray = 0x65000u;
constexpr GuestAddress Entries = 0x66000u;
constexpr GuestAddress Result = 0x70000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress GlobalReady = 0x83246260u;
constexpr GuestAddress HashBucketZero = 0x832ee568u;
constexpr std::array<Region, 3> Regions{{{0u, 0x90000u},
    {0x83246000u, 0x1000u}, {0x832ee000u, 0x1000u}}};

enum class Mode { EmptyFloat, MissingFloat, HitFloat, EmptyButton,
    MissingButton, HitButton };
constexpr std::array Cases{Mode::EmptyFloat, Mode::MissingFloat,
    Mode::HitFloat, Mode::EmptyButton, Mode::MissingButton,
    Mode::HitButton};

bool FloatRoute(Mode mode)
{ return mode == Mode::EmptyFloat || mode == Mode::MissingFloat ||
    mode == Mode::HitFloat; }
bool Hit(Mode mode)
{ return mode == Mode::HitFloat || mode == Mode::HitButton; }
bool Empty(Mode mode)
{ return mode == Mode::EmptyFloat || mode == Mode::EmptyButton; }

PPCRegister* Gpr(PPCContext& context, unsigned index)
{
    PPCRegister* const fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16,
        &context.r17, &context.r18, &context.r19, &context.r20,
        &context.r21, &context.r22, &context.r23, &context.r24,
        &context.r25, &context.r26, &context.r27, &context.r28,
        &context.r29, &context.r30, &context.r31};
    return fields[index];
}

family::Registers FromPpc(PPCContext& context)
{
    family::Registers state{};
    for (unsigned i = 0; i < 32; ++i)
        state.r[i] = Gpr(context, i)->u64;
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

void WriteText(GuestMemory& memory, GuestAddress address,
    const char* text)
{
    do
    {
        memory.WriteU16(address, static_cast<unsigned char>(*text));
        address += 2u;
    } while (*text++);
}

void Seed(GuestWindow& window, Mode mode)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(GlobalReady, 1u); // Skips the accepted 446-record init.
    WriteText(memory, Text, Empty(mode) ? "" : "A");
    if (!Hit(mode)) return;
    memory.WriteU32(HashBucketZero, Record);
    memory.WriteU32(Record, 7u);
    memory.WriteU32(Record + 12u, 0u);
    WriteText(memory, Record + 16u, "A");
    memory.WriteU32(Object + 108u, Entries);
    memory.WriteU32(Object + 112u, 1u);
    memory.WriteU32(Object + 120u, BucketArray);
    memory.WriteU32(Object + 124u, 1u);
    memory.WriteU32(BucketArray, 0u);
    memory.WriteU32(Entries, 0xffffffffu);
    memory.WriteU32(Entries + 4u, 7u);
    memory.WriteU32(Entries + 8u, 0u);
    memory.WriteU32(Entries + 12u, Result);
}

PPCContext Initial()
{
    PPCContext context{};
    for (unsigned i = 0; i < 32; ++i)
        Gpr(context, i)->u64 = 0x1122334400000000ull + i;
    context.r1.u64 = 0x1234567800000000ull | Stack;
    context.r3.u64 = 0x9988776600000000ull | Object;
    context.r4.u64 = 0x8877665500000000ull | Text;
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    return context;
}

struct Services final : CrtThreadDataServices, InvalidParameterServices,
    metadata_name_record::Services, ManagerFacadeServices,
    ArrayResizeServices,
    registered_constructor_family::RegistrationServices,
    manager_index_operations::FpServices
{
    std::uint64_t GetTlsValue(std::uint32_t) override
    { throw std::runtime_error("unselected TLS read"); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override
    { throw std::runtime_error("unselected TLS write"); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unselected thread getter"); }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unselected thread allocation"); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unselected thread binding"); }
    void FreeThreadData(std::uint64_t) override
    { throw std::runtime_error("unselected thread free"); }
    void CallHandler(GuestMemory&, GuestAddress,
        InvalidParameterCall&) override
    { throw std::runtime_error("unselected invalid handler"); }
    void Trap(const InvalidParameterCall&) override
    { throw std::runtime_error("unselected invalid trap"); }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unselected allocator"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unselected primary constructor"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unselected fallback constructor"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unselected manager method"); }
    std::uint64_t AllocateRecord(GuestAddress, GuestMemory&,
        std::uint64_t, std::uint64_t, std::uint64_t,
        std::uint64_t, metadata_name_record::FrameRegisters&) override
    { throw std::runtime_error("unselected record allocation"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unselected storage release"); }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unselected storage allocation"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unselected storage resize"); }
    void InitializeManager() override
    { throw std::runtime_error("unselected array manager"); }
    std::uint64_t Register(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unselected registration"); }
    void DisableFlushMode() override
    { throw std::runtime_error("unselected FP mode change"); }
};

bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr &&
        a.ctr == b.ctr && a.xer_so == b.xer_so &&
        a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so;
}

void Expected(Mode mode, const GuestWindow& window,
    const PPCContext& context)
{
    const auto expected = Hit(mode) ? Result : 0u;
    if (context.r3.u64 != expected)
    {
        std::fprintf(stderr, "EXPECT name-route case=%u r3=%llX/%X "
            "bucket=%X key=%X\n", static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64), expected,
            window.Memory().ReadU32(HashBucketZero),
            window.Memory().ReadU32(Text));
        throw std::runtime_error("independent name-route result");
    }
}

void Check(Mode mode)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, mode);
    Seed(recovered, mode);
    auto context = Initial();
    auto state = FromPpc(context);
    const auto entry = FloatRoute(mode) ? 0x82296b00u : 0x82713828u;
    if (FloatRoute(mode)) __imp__sub_82296B00(context, original.Bytes());
    else __imp__sub_82713828(context, original.Bytes());
    Expected(mode, original, context);
    Services services;
    auto memory = recovered.Memory();
    family::Dependencies dependencies{services, services, services,
        services, services, services, services};
    if (!family::Apply(entry, memory, dependencies, state))
        throw std::runtime_error("name-route entry missing");
    const auto observed = FromPpc(context);
    const bool state_equal = Same(observed, state);
    const bool ram_equal = original.EqualCommitted(recovered);
    if (!state_equal || !ram_equal)
    {
        std::fprintf(stderr, "FAIL name-route case=%u state=%u RAM=%u "
            "r3=%llX/%llX r11=%llX/%llX "
            "SP=%llX/%llX LR=%llX/%llX CTR=%llX/%llX "
            "CR0=%u%u%u%u/%u%u%u%u CR6=%u%u%u%u/%u%u%u%u "
            "SO=%u/%u CA=%u/%u\n",
            static_cast<unsigned>(mode), state_equal, ram_equal,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.r[11]),
            static_cast<unsigned long long>(state.r[11]),
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(observed.ctr),
            static_cast<unsigned long long>(state.ctr),
            observed.cr0.lt, observed.cr0.gt, observed.cr0.eq,
            observed.cr0.so, state.cr0.lt, state.cr0.gt, state.cr0.eq,
            state.cr0.so, observed.cr6.lt, observed.cr6.gt,
            observed.cr6.eq, observed.cr6.so, state.cr6.lt,
            state.cr6.gt, state.cr6.eq, state.cr6.so,
            observed.xer_so, state.xer_so, observed.xer_ca, state.xer_ca);
        for (unsigned i = 0; i < 32; ++i)
            if (observed.r[i] != state.r[i])
                std::fprintf(stderr, " r%u=%llX/%llX", i,
                    static_cast<unsigned long long>(observed.r[i]),
                    static_cast<unsigned long long>(state.r[i]));
        std::fputc('\n', stderr);
        throw std::runtime_error("name-route PPC mismatch");
    }
}
} // namespace

void OriginalSaveName(PPCContext& context, std::uint8_t* base,
    unsigned first)
{
    for (unsigned i = first; i <= 31; ++i)
        PPC_STORE_U64(context.r1.u32 - 8u * (33u - i),
            Gpr(context, i)->u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}
void OriginalRestoreName(PPCContext& context, std::uint8_t* base,
    unsigned first)
{
    for (unsigned i = first; i <= 31; ++i)
        Gpr(context, i)->u64 = PPC_LOAD_U64(
            context.r1.u32 - 8u * (33u - i));
    context.r12.u64 = PPC_LOAD_U32(context.r1.u32 - 8u);
    context.lr = context.r12.u64;
}

void OriginalUnselectedName(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unselected original name-route lower"); }

int main()
{
    try
    {
        for (const auto mode : Cases) Check(mode);
        std::printf("PASS legacy-config-name-routes %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT allocator/manager callbacks and unselected registry/constructor/index paths");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
