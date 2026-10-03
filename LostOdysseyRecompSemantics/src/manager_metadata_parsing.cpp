#include "lo_semantics/manager_metadata_parsing.h"

#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/manager_object_registration.h"
#include "lo_semantics/registered_metadata_string.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::manager_metadata_parsing
{
namespace
{
constexpr GuestAddress kCharacterClasses = 0x83215b40u;
constexpr GuestAddress kDecimal = 0x82376fa8u;
constexpr GuestAddress kClass = 0x822974b0u;
constexpr GuestAddress kParseCore = 0x82b7d3e0u;
constexpr GuestAddress kParseWrapper = 0x82b7d688u;
constexpr GuestAddress kParseTen = 0x82376f98u;
constexpr GuestAddress kMetadataNumber = 0x82296e80u;

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

void SaveFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, unsigned first, std::uint32_t size)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    for (unsigned reg = first; reg <= 31; ++reg)
        WriteU64(memory, sp - 8u * (33u - reg),
            frame.r23_through_r31[reg - 23u]);
    memory.WriteU32(sp - size, sp);
}

void RestoreFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, unsigned first)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    frame.lr = memory.ReadU32(sp - 8u);
    for (unsigned reg = first; reg <= 31; ++reg)
        frame.r23_through_r31[reg - 23u] =
            ReadU64(memory, sp - 8u * (33u - reg));
}

class ThreadAdapter final : public AllocationFailureServices
{
public:
    ThreadAdapter(GuestMemory& memory, CrtThreadDataServices& services,
        FrameRegisters& frame)
        : memory_(memory), services_(services), frame_(frame) {}

    std::uint64_t OutputErrorMessage(GuestAddress) override
    { throw std::logic_error("unexpected error output"); }
    std::uint64_t BugCheck(std::uint32_t) override
    { throw std::logic_error("unexpected bugcheck"); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { throw std::logic_error("unexpected new handler"); }
    std::uint64_t GetThreadData() override
    {
        CrtThreadDataCall call{frame_.r13};
        const std::uint64_t result = GetCrtThreadData(memory_, services_, call);
        frame_.r13 = call.thread_environment;
        return result;
    }

private:
    GuestMemory& memory_;
    CrtThreadDataServices& services_;
    FrameRegisters& frame_;
};

void WriteError(GuestMemory& memory, CrtThreadDataServices& thread_services,
    FrameRegisters& frame, std::uint32_t code)
{
    ThreadAdapter adapter(memory, thread_services, frame);
    const GuestAddress error = static_cast<GuestAddress>(
        GetAllocationErrorAddress(adapter));
    memory.WriteU32(error, code);
}

std::uint64_t InvalidArguments(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, FrameRegisters& frame)
{
    WriteError(memory, thread_services, frame, 22);
    InvalidParameterCall call{};
    call.arguments[5] = frame.r8;
    call.arguments[6] = frame.r9;
    call.arguments[7] = 22;
    call.thread_environment = frame.r13;
    (void)ReportInvalidParameter(memory, invalid_services, call);
    frame.r13 = call.thread_environment;
    frame.r8 = call.arguments[5];
    frame.r9 = call.arguments[6];
    return 0;
}

std::uint64_t ParseInteger(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, std::uint64_t source,
    std::uint64_t end_pointer, std::uint64_t base_register,
    std::uint64_t flags, std::uint64_t caller_sp, FrameRegisters& frame)
{
    SaveFrame(memory, caller_sp, frame, 23u, 160u);
    const GuestAddress source_address = static_cast<GuestAddress>(source);
    const GuestAddress end_address = static_cast<GuestAddress>(end_pointer);
    std::uint32_t base = static_cast<std::uint32_t>(base_register);
    if (end_address != 0)
        memory.WriteU32(end_address, source_address);
    if (source_address == 0 ||
        (base != 0 && (static_cast<std::int32_t>(base) < 2 ||
                       static_cast<std::int32_t>(base) > 36)))
    {
        const auto invalid = InvalidArguments(memory, thread_services,
            invalid_services, frame);
        RestoreFrame(memory, caller_sp, frame, 23u);
        return invalid;
    }

    std::uint64_t cursor = source + 2u;
    std::uint16_t current = memory.ReadU16(source_address);
    while (CharacterClass(memory, current, 8u) != 0)
    {
        current = memory.ReadU16(static_cast<GuestAddress>(cursor));
        cursor += 2u;
    }
    if (current == '-' || current == '+')
    {
        if (current == '-') flags |= 2u;
        current = memory.ReadU16(static_cast<GuestAddress>(cursor));
        cursor += 2u;
    }

    if (base == 0)
    {
        if (DecimalDigit(current) != 0)
            base = 10;
        else
        {
            const std::uint16_t following =
                memory.ReadU16(static_cast<GuestAddress>(cursor));
            base = following == 'x' || following == 'X' ? 16u : 8u;
        }
    }
    if (base == 16 && DecimalDigit(current) == 0)
    {
        const std::uint16_t following =
            memory.ReadU16(static_cast<GuestAddress>(cursor));
        if (following == 'x' || following == 'X')
        {
            const std::uint64_t after_prefix = cursor + 2u;
            cursor = after_prefix + 2u;
            current = memory.ReadU16(static_cast<GuestAddress>(after_prefix));
        }
    }

    const std::uint32_t cutoff = 0xffffffffu / base;
    std::uint64_t value = 0;
    for (;;)
    {
        std::uint64_t digit = DecimalDigit(current);
        if (static_cast<std::int32_t>(digit) == -1)
        {
            std::uint32_t letter = current;
            if (letter >= 'a' && letter <= 'z') letter -= 32u;
            digit = letter >= 'A' && letter <= 'Z' ? letter - 55u : digit;
        }
        if (static_cast<std::uint32_t>(digit) >= base)
            break;
        flags |= 8u;
        const std::uint32_t low = static_cast<std::uint32_t>(value);
        const std::uint32_t remainder =
            0xffffffffu - (0xffffffffu / base) * base;
        if (low > cutoff || (low == cutoff &&
                static_cast<std::uint32_t>(digit) > remainder))
        {
            flags |= 4u;
            if (end_address == 0)
                break;
        }
        else
        {
            const std::int64_t product =
                std::int64_t{std::bit_cast<std::int32_t>(low)} *
                std::int64_t{std::bit_cast<std::int32_t>(base)};
            value = static_cast<std::uint64_t>(product) + digit;
        }
        current = memory.ReadU16(static_cast<GuestAddress>(cursor));
        cursor += 2u;
    }

    cursor -= 2u;
    if ((flags & 8u) == 0)
    {
        if (end_address != 0)
            cursor = source;
        value = 0;
    }
    else
    {
        const bool negative = (flags & 2u) != 0;
        const bool unsigned_parse = (flags & 1u) != 0;
        if ((flags & 4u) != 0 ||
            (!unsigned_parse && static_cast<std::uint32_t>(value) >
                (negative ? 0x80000000u : 0x7fffffffu)))
        {
            WriteError(memory, thread_services, frame, 34);
            value = unsigned_parse ? 0xffffffffffffffffull :
                negative ? 0xffffffff80000000ull : 0x7fffffffull;
        }
    }
    if (end_address != 0)
        memory.WriteU32(end_address, static_cast<GuestAddress>(cursor));
    if ((flags & 2u) != 0)
        value = 0u - value;
    RestoreFrame(memory, caller_sp, frame, 23u);
    return value;
}

std::uint64_t ParseMetadataNumber(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    std::uint64_t source, std::uint64_t destination,
    std::uint64_t output_word, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    SaveFrame(memory, caller_sp, frame, 27u, 128u);
    const std::uint64_t length = registered_metadata_string::Utf16Length(
        memory, source);
    const std::uint64_t last = source +
        ((static_cast<std::uint32_t>(length) << 1u) & 0xfffffffeu) - 2u;
    std::uint64_t marker = last;
    std::uint16_t unit = memory.ReadU16(static_cast<GuestAddress>(marker));
    if (unit < '0' || unit > '9')
    {
        RestoreFrame(memory, caller_sp, frame, 27u);
        return 0;
    }
    for (;;)
    {
        if (unit > '9' || static_cast<GuestAddress>(marker) <=
                static_cast<GuestAddress>(source))
            break;
        marker -= 2u;
        unit = memory.ReadU16(static_cast<GuestAddress>(marker));
        if (unit < '0') break;
    }
    if (memory.ReadU16(static_cast<GuestAddress>(marker)) != '_')
    {
        RestoreFrame(memory, caller_sp, frame, 27u);
        return 0;
    }
    const std::uint16_t first_digit =
        memory.ReadU16(static_cast<GuestAddress>(marker + 2u));
    if (first_digit == '0' &&
        static_cast<std::int32_t>(static_cast<std::uint32_t>(last - marker) &
            0xfffffffeu) != 2)
    {
        RestoreFrame(memory, caller_sp, frame, 27u);
        return 0;
    }

    frame.r23_through_r31[27u - 23u] = destination;
    frame.r23_through_r31[28u - 23u] = 0;
    frame.r23_through_r31[29u - 23u] = output_word;
    frame.r23_through_r31[30u - 23u] = source;
    frame.r23_through_r31[31u - 23u] = marker;
    frame.lr = 0x82296f14u;
    const std::uint64_t parsed = ParseInteger(memory, thread_services,
        invalid_services, marker + 2u, 0, 10, 0,
        caller_sp - 128u, frame);
    memory.WriteU32(static_cast<GuestAddress>(frame.r23_through_r31[29u - 23u]),
        static_cast<GuestAddress>(parsed));
    const std::int32_t distance = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(frame.r23_through_r31[31u - 23u] -
            frame.r23_through_r31[30u - 23u]));
    std::int64_t copy_units = (distance >> 1) + 1;
    if (copy_units > 128) copy_units = 128;
    const auto copy = manager_object_registration::CopyUtf16Padded(memory,
        frame.r23_through_r31[27u - 23u],
        frame.r23_through_r31[30u - 23u],
        static_cast<std::uint64_t>(copy_units));
    (void)copy;
    memory.WriteU16(static_cast<GuestAddress>(
        frame.r23_through_r31[27u - 23u] +
        ((static_cast<std::uint32_t>(copy_units) << 1u) & 0xfffffffeu) - 2u),
        static_cast<std::uint16_t>(frame.r23_through_r31[28u - 23u]));
    RestoreFrame(memory, caller_sp, frame, 27u);
    return 1;
}
} // namespace

std::uint64_t DecimalDigit(std::uint64_t code_unit)
{
    constexpr std::uint16_t starts[] = {48, 1632, 1776, 2406, 2534,
        2662, 2790, 2918, 3174, 3302, 3430, 3664, 3792, 3872,
        4160, 6112, 6160, 65296};
    const std::uint16_t unit = static_cast<std::uint16_t>(code_unit);
    for (std::uint16_t start : starts)
        if (unit >= start && unit < start + 10u)
            return unit - start;
    return 0xffffffffffffffffull;
}

std::uint64_t CharacterClass(GuestMemory& memory,
    std::uint64_t code_unit, std::uint64_t mask)
{
    const std::uint16_t unit = static_cast<std::uint16_t>(code_unit);
    const std::uint16_t requested = static_cast<std::uint16_t>(mask);
    if (unit >= 256)
        return 0;
    const GuestAddress table = memory.ReadU32(kCharacterClasses);
    return memory.ReadU16(table + 2u * unit) & requested;
}

bool Apply(GuestAddress address, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t incoming_r7, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case kDecimal:
        result = DecimalDigit(incoming_r3);
        return true;
    case kClass:
        result = CharacterClass(memory, incoming_r3, incoming_r4);
        return true;
    case kParseCore:
        result = ParseInteger(memory, thread_services, invalid_services,
            incoming_r4, incoming_r5, incoming_r6, incoming_r7,
            caller_sp, frame);
        return true;
    case kParseWrapper:
        result = ParseInteger(memory, thread_services, invalid_services,
            incoming_r3, incoming_r4, incoming_r5, 0, caller_sp, frame);
        return true;
    case kParseTen:
        result = ParseInteger(memory, thread_services, invalid_services,
            incoming_r3, 0, 10, 0, caller_sp, frame);
        return true;
    case kMetadataNumber:
        result = ParseMetadataNumber(memory, thread_services, invalid_services,
            incoming_r3, incoming_r4, incoming_r6, caller_sp, frame);
        return true;
    default:
        return false;
    }
}

} // namespace lo::semantic::gpu::manager_metadata_parsing
