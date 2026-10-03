#include "lo_semantics/crt_stream_state.h"

#include "lo_semantics/allocation_failure.h"

#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_state
{
namespace
{
constexpr GuestAddress kReadStreamField = 0x82B81648u;
constexpr GuestAddress kInitializeBuffer = 0x82B85C40u;
constexpr GuestAddress kNativeWrapper = 0x82B82180u;
constexpr GuestAddress kInitializeOnce = 0x82B821B0u;
constexpr GuestAddress kBufferCounter = 0x832D3AB8u;
constexpr GuestAddress kOnceTarget = 0x832D3CA8u;

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

void Enter(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, GuestAddress size, bool save_r31)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    if (save_r31)
        WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - size, sp);
    frame.sp = caller_sp - size;
}

void Leave(GuestMemory& memory, FrameRegisters& frame,
    GuestAddress size, bool restore_r31)
{
    frame.sp += size;
    const GuestAddress sp = static_cast<GuestAddress>(frame.sp);
    frame.lr = memory.ReadU32(sp - 8u);
    if (restore_r31)
        frame.r31 = ReadU64(memory, sp - 16u);
}

class ErrorAddressServices final : public AllocationFailureServices
{
public:
    ErrorAddressServices(GuestMemory& memory, CrtThreadDataServices& services,
        InvalidParameterCall& call)
        : memory_(memory), services_(services), call_(call) {}

    std::uint64_t GetThreadData() override
    {
        CrtThreadDataCall state{call_.thread_environment};
        const auto data = GetCrtThreadData(memory_, services_, state);
        call_.thread_environment = state.thread_environment;
        return data;
    }
    std::uint64_t OutputErrorMessage(GuestAddress) override
    { throw std::logic_error("unexpected error output"); }
    std::uint64_t BugCheck(std::uint32_t) override
    { throw std::logic_error("unexpected bug-check"); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { throw std::logic_error("unexpected new handler"); }
private:
    GuestMemory& memory_;
    CrtThreadDataServices& services_;
    InvalidParameterCall& call_;
};

std::uint64_t ReadStreamField(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    InvalidParameterCall& call, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, 96u, false);
    std::uint64_t value;
    if (static_cast<GuestAddress>(call.arguments[0]) == 0)
    {
        frame.lr = 0x82B81660u;
        ErrorAddressServices errors(memory, thread_services, call);
        const GuestAddress error_address = static_cast<GuestAddress>(
            GetAllocationErrorAddress(errors));
        call.arguments[7] = 22;
        for (unsigned index = 0; index != 5; ++index)
            call.arguments[index] = 0;
        memory.WriteU32(error_address, 22);
        frame.lr = 0x82B81684u;
        (void)ReportInvalidParameter(memory, invalid_services, call);
        value = UINT64_MAX;
    }
    else
    {
        value = memory.ReadU32(static_cast<GuestAddress>(call.arguments[0]) + 16u);
    }
    call.arguments[0] = value;
    Leave(memory, frame, 96u, false);
    return value;
}

std::uint64_t InitializeBuffer(GuestMemory& memory,
    RawAllocationServices& allocation_services,
    InvalidParameterCall& call, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, 96u, true);
    frame.r31 = call.arguments[0];
    const GuestAddress buffer = static_cast<GuestAddress>(frame.r31);
    const auto count = memory.ReadU32(kBufferCounter);
    memory.WriteU32(kBufferCounter, count + 1u);
    call.arguments[0] = 4096;
    frame.lr = 0x82B85C6Cu;
    const auto allocated = AllocateRawMemory(memory, allocation_services, 4096);
    call.arguments[0] = allocated;
    const auto flags = memory.ReadU32(buffer + 12u);
    memory.WriteU32(buffer + 8u, static_cast<GuestAddress>(allocated));
    if (static_cast<GuestAddress>(allocated) != 0)
    {
        memory.WriteU32(buffer + 24u, 4096);
        memory.WriteU32(buffer + 12u, flags | 8u);
    }
    else
    {
        memory.WriteU32(buffer + 8u, buffer + 20u);
        memory.WriteU32(buffer + 24u, 2);
        memory.WriteU32(buffer + 12u, flags | 4u);
    }
    memory.WriteU32(buffer + 4u, 0);
    memory.WriteU32(buffer, memory.ReadU32(buffer + 8u));
    Leave(memory, frame, 96u, true);
    return allocated;
}

std::uint64_t NativeWrapper(GuestMemory& memory,
    NativeServices& native_services, InvalidParameterCall& call,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, 96u, false);
    frame.lr = 0x82B82190u;
    call.arguments[0] = native_services.InitializeCriticalSection(
        memory, call, frame);
    call.arguments[0] = 1;
    Leave(memory, frame, 96u, false);
    return 1;
}

std::uint64_t InitializeOnce(GuestMemory& memory,
    NativeServices& native_services, InvalidParameterCall& call,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, 112u, true);
    frame.r31 = caller_sp - 112u;
    GuestAddress target = memory.ReadU32(kOnceTarget);
    if (target == 0)
    {
        target = kNativeWrapper;
        memory.WriteU32(kOnceTarget, target);
    }
    frame.lr = 0x82B821F4u;
    const auto value = (target & ~3u) == kNativeWrapper ?
        NativeWrapper(memory, native_services, call, frame.sp, frame) :
        native_services.CallIndirect(target & ~3u, memory, call, frame);
    call.arguments[0] = value;
    memory.WriteU32(static_cast<GuestAddress>(frame.r31 + 80u),
        static_cast<GuestAddress>(value));
    // The PPC epilogue uses live r31, not the incoming stack pointer.
    frame.sp = frame.r31 + 112u;
    const GuestAddress sp = static_cast<GuestAddress>(frame.sp);
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r31 = ReadU64(memory, sp - 16u);
    return value;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    RawAllocationServices& allocation_services,
    NativeServices& native_services, InvalidParameterCall& call,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result)
{
    switch (address)
    {
    case kReadStreamField:
        result = ReadStreamField(memory, thread_services, invalid_services,
            call, caller_sp, frame);
        return true;
    case kInitializeBuffer:
        result = InitializeBuffer(memory, allocation_services, call,
            caller_sp, frame);
        return true;
    case kNativeWrapper:
        result = NativeWrapper(memory, native_services, call, caller_sp, frame);
        return true;
    case kInitializeOnce:
        result = InitializeOnce(memory, native_services, call, caller_sp, frame);
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_state
