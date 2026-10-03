#include "lo_semantics/metadata_option_match.h"

#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/registered_metadata_string.h"

#include <stdexcept>

namespace lo::semantic::gpu::metadata_option_match
{
namespace
{
constexpr GuestAddress kCompare = 0x82296858u;
constexpr GuestAddress kFind = 0x82297390u;
constexpr GuestAddress kScan = 0x8247C0C0u;

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

void EnterFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, unsigned first, GuestAddress frame_size)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    for (unsigned reg = first; reg <= 31u; ++reg)
        WriteU64(memory, sp - 8u * (33u - reg),
            frame.r25_through_r31[reg - 25u]);
    memory.WriteU32(sp - frame_size, sp);
}

void LeaveFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, unsigned first)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    frame.lr = memory.ReadU32(sp - 8u);
    for (unsigned reg = first; reg <= 31u; ++reg)
        frame.r25_through_r31[reg - 25u] =
            ReadU64(memory, sp - 8u * (33u - reg));
}

std::uint16_t FoldForComparison(std::uint16_t value)
{
    return value >= 65u && value <= 90u ? value + 32u : value;
}

std::uint16_t FoldForSearch(std::uint16_t value)
{
    return value >= 97u && value <= 122u ? value - 32u : value;
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

std::uint64_t ComparePrefix(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    InvalidParameterCall& call, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    memory.WriteU32(sp - 96u, sp);
    auto& r = call.arguments;
    std::uint64_t value = 0;
    if (static_cast<GuestAddress>(r[2]) != 0)
    {
        if (static_cast<GuestAddress>(r[0]) == 0 ||
            static_cast<GuestAddress>(r[1]) == 0)
        {
            frame.lr = 0x8229687Cu;
            ErrorAddressServices errors(memory, thread_services, call);
            const auto address = static_cast<GuestAddress>(
                GetAllocationErrorAddress(errors));
            r[7] = 22;
            for (unsigned i = 0; i < 5; ++i)
                r[i] = 0;
            memory.WriteU32(address, 22);
            frame.lr = 0x822968A0u;
            (void)ReportInvalidParameter(memory, invalid_services, call);
            value = 0x7FFFFFFFu;
        }
        else
        {
            for (;;)
            {
                const auto left = FoldForComparison(memory.ReadU16(
                    static_cast<GuestAddress>(r[0])));
                const auto right = FoldForComparison(memory.ReadU16(
                    static_cast<GuestAddress>(r[1])));
                r[6] = left; // r9 holds the folded left code unit.
                r[7] = right;
                r[2] -= 1u;
                r[0] += 2u;
                r[1] += 2u;
                const bool stop = static_cast<GuestAddress>(r[2]) == 0 ||
                    left == 0;
                if (!stop)
                    r[5] = right; // r8 is written only past the early exits.
                if (stop || left != right)
                {
                    r[7] = left;
                    value = static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(left) - right);
                    break;
                }
            }
        }
    }
    r[0] = value;
    frame.lr = memory.ReadU32(sp - 8u);
    return value;
}

std::uint64_t FindToken(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    InvalidParameterCall& call, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    EnterFrame(memory, caller_sp, frame, 25, 144u);
    auto& r = call.arguments;
    frame.r25_through_r31[5] = r[1]; // r30: token
    frame.r25_through_r31[4] = r[0]; // r29: candidate
    std::uint64_t found = 0;
    if (static_cast<GuestAddress>(r[0]) != 0 &&
        static_cast<GuestAddress>(r[1]) != 0)
    {
        const auto first = FoldForSearch(memory.ReadU16(
            static_cast<GuestAddress>(frame.r25_through_r31[5])));
        r[0] = frame.r25_through_r31[5];
        frame.lr = 0x822973DCu;
        const std::uint64_t length = registered_metadata_string::Utf16Length(
            memory, r[0]);
        frame.r25_through_r31[0] = first;
        frame.r25_through_r31[1] = length - 1u; // r26
        frame.r25_through_r31[2] = frame.r25_through_r31[5] + 2u; // r27
        frame.r25_through_r31[5] = frame.r25_through_r31[4] + 2u; // r30
        std::uint16_t current = memory.ReadU16(
            static_cast<GuestAddress>(frame.r25_through_r31[4]));
        bool inside_word = false;
        while (current != 0)
        {
            const auto folded = FoldForSearch(current);
            frame.r25_through_r31[6] = folded;
            if (!inside_word && folded == first)
            {
                r[0] = frame.r25_through_r31[5];
                r[1] = frame.r25_through_r31[2];
                r[2] = frame.r25_through_r31[1];
                frame.lr = 0x82297440u;
                const std::uint64_t equal = ComparePrefix(memory,
                    thread_services, invalid_services, call,
                    caller_sp - 144u, frame);
                if (static_cast<std::int32_t>(equal) == 0)
                {
                    found = frame.r25_through_r31[5] - 2u;
                    break;
                }
            }
            inside_word = (folded >= 65u && folded <= 90u) ||
                (folded >= 48u && folded <= 57u);
            current = memory.ReadU16(static_cast<GuestAddress>(
                frame.r25_through_r31[5]));
            frame.r25_through_r31[5] += 2u;
        }
    }
    r[0] = found;
    LeaveFrame(memory, caller_sp, frame, 25);
    return found;
}

std::uint64_t ScanOptions(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    InvalidParameterCall& call, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    EnterFrame(memory, caller_sp, frame, 29, 112u);
    auto& r = call.arguments;
    frame.r25_through_r31[4] = r[0]; // r29: option sequence
    frame.r25_through_r31[5] = r[1]; // r30: requested option
    std::uint64_t answer = 0;
    if (memory.ReadU16(static_cast<GuestAddress>(frame.r25_through_r31[4])) != 0)
    {
        r[0] = frame.r25_through_r31[4] + 2u;
        r[1] = frame.r25_through_r31[5];
        frame.lr = 0x8247C0E8u;
        frame.r25_through_r31[6] = FindToken(memory, thread_services,
            invalid_services, call, caller_sp - 112u, frame);
        while (static_cast<GuestAddress>(frame.r25_through_r31[6]) != 0)
        {
            const auto candidate = frame.r25_through_r31[6];
            if (static_cast<GuestAddress>(candidate) >
                static_cast<GuestAddress>(frame.r25_through_r31[4]))
            {
                const auto delimiter = memory.ReadU16(
                    static_cast<GuestAddress>(candidate - 2u));
                if (delimiter == 45u || delimiter == 47u)
                {
                    r[0] = frame.r25_through_r31[5];
                    frame.lr = 0x8247C118u;
                    const auto length = registered_metadata_string::Utf16Length(
                        memory, r[0]);
                    const auto bytes = static_cast<std::uint32_t>(length << 1u) &
                        0xFFFFFFFEu;
                    const auto after = candidate + bytes;
                    if (static_cast<GuestAddress>(after) == 0)
                    {
                        answer = 1;
                        break;
                    }
                    const auto suffix = memory.ReadU16(
                        static_cast<GuestAddress>(after));
                    if (suffix == 0 || suffix == 32u || suffix == 9u)
                    {
                        answer = 1;
                        break;
                    }
                }
            }
            r[0] = candidate + 2u;
            r[1] = frame.r25_through_r31[5];
            frame.lr = 0x8247C154u;
            frame.r25_through_r31[6] = FindToken(memory, thread_services,
                invalid_services, call, caller_sp - 112u, frame);
        }
    }
    r[0] = answer;
    LeaveFrame(memory, caller_sp, frame, 29);
    return answer;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    InvalidParameterCall& call, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case kCompare:
        result = ComparePrefix(memory, thread_services, invalid_services,
            call, caller_sp, frame);
        return true;
    case kFind:
        result = FindToken(memory, thread_services, invalid_services,
            call, caller_sp, frame);
        return true;
    case kScan:
        result = ScanOptions(memory, thread_services, invalid_services,
            call, caller_sp, frame);
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::metadata_option_match
