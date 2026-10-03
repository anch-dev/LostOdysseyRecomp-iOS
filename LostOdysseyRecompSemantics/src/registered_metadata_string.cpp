#include "lo_semantics/registered_metadata_string.h"

#include "lo_semantics/memory_move.h"
#include "lo_semantics/registered_metadata_words.h"

#include <bit>
#include <stdexcept>

namespace lo::semantic::gpu::registered_metadata_string
{
namespace
{
constexpr GuestAddress kInitialize = 0x8229C8B0u;
constexpr GuestAddress kAssign = 0x8229F5E0u;
constexpr GuestAddress kLength = 0x82296830u;
constexpr GuestAddress kRegistration = 0x824070C8u;
constexpr GuestAddress kMetadata = 0x82722D18u;
constexpr GuestAddress kTailAssign = 0x82723DB8u;

// addi r11,r3,1 after the length call takes the full signed result; the
// header stores only its low word.
std::uint32_t ElementCount(GuestMemory& memory, std::uint64_t source)
{
    if (memory.ReadU16(static_cast<GuestAddress>(source)) == 0)
        return 0;
    return static_cast<std::uint32_t>(Utf16Length(memory, source) + 1u);
}

void CopyLiveString(GuestMemory& memory, std::uint64_t destination,
    std::uint64_t source, GuestAddress frame)
{
    const GuestAddress header = static_cast<GuestAddress>(destination);
    const std::uint32_t count = memory.ReadU32(header + 4u);
    // cmpwi only skips zero. A negative signed count enters CopyGuestMemory
    // with its doubled low word, as in the PPC body.
    if (count != 0)
    {
        const std::uint64_t bytes = (count << 1u) & 0xFFFFFFFEu;
        const std::uint64_t storage = memory.ReadU32(header);
        (void)CopyGuestMemory(memory, storage,
            static_cast<GuestAddress>(source), bytes, frame);
    }
}
} // namespace

std::uint64_t Utf16Length(GuestMemory& memory, std::uint64_t source_register)
{
    std::uint64_t cursor = source_register;
    do
    {
        const std::uint16_t code_unit = memory.ReadU16(
            static_cast<GuestAddress>(cursor));
        cursor += 2u;
        if (code_unit == 0)
            break;
    } while (true);

    const std::uint32_t distance = static_cast<std::uint32_t>(
        cursor - source_register);
    const std::int64_t half = std::bit_cast<std::int32_t>(distance) >> 1;
    return static_cast<std::uint64_t>(half - 1);
}

std::uint64_t InitializeString(GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t destination_register,
    std::uint64_t source_register, GuestAddress caller_sp)
{
    const GuestAddress header = static_cast<GuestAddress>(destination_register);
    const std::uint32_t count = ElementCount(memory, source_register);
    memory.WriteU32(header + 4u, count);
    memory.WriteU32(header + 8u, count);
    memory.WriteU32(header, 0);
    ResizeArray(memory, resize_services, header, 2, 8);
    CopyLiveString(memory, destination_register, source_register, caller_sp - 112u);
    return destination_register;
}

std::uint64_t AssignString(GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t destination_register,
    std::uint64_t source_register, GuestAddress caller_sp)
{
    const GuestAddress header = static_cast<GuestAddress>(destination_register);
    if (memory.ReadU32(header) == static_cast<GuestAddress>(source_register))
        return destination_register;

    const std::uint32_t count = ElementCount(memory, source_register);
    memory.WriteU32(header + 8u, count);
    memory.WriteU32(header + 4u, count);
    ResizeArray(memory, resize_services, header, 2, 8);
    CopyLiveString(memory, destination_register, source_register, caller_sp - 112u);
    return destination_register;
}

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services, ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    GuestAddress caller_sp, std::uint64_t& result)
{
    // Dispatch precedes all reads so an unknown target is effect-free.
    switch (address)
    {
    case kLength:
        result = Utf16Length(memory, incoming_r3);
        return true;
    case kInitialize:
        result = InitializeString(memory, resize_services,
            incoming_r3, incoming_r4, caller_sp);
        return true;
    case kAssign:
        result = AssignString(memory, resize_services,
            incoming_r3, incoming_r4, caller_sp);
        return true;
    case kTailAssign:
        result = AssignString(memory, resize_services,
            incoming_r3 + 72u, 0xFFFFFFFF82201354ull, caller_sp);
        return true;
    case kMetadata:
    {
        const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
        const GuestAddress frame = caller_sp - 112u;
        (void)AssignString(memory, resize_services,
            incoming_r3 + 72u, 0xFFFFFFFF821A83D0ull, frame);
        const GuestAddress array = memory.ReadU32(object + 52u) + 364u;
        memory.WriteU32(object + 84u, 0);
        memory.WriteU32(frame + 80u, 0x0001003Cu);
        result = registered_metadata_words::AppendMetadataWord(memory,
            resize_services, array, frame + 80u);
        return true;
    }
    case kRegistration:
    {
        const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
        const GuestAddress frame = caller_sp - 112u;
        const GuestAddress array = object + 80u;
        const std::uint64_t index = registered_metadata_words::AddArrayElements(
            memory, resize_services, array, 1, 12, 8);
        const GuestAddress offset = static_cast<GuestAddress>(
            (static_cast<std::uint32_t>(index + (index << 1u)) << 2u) & 0xFFFFFFFCu);
        std::uint64_t live_r3 = std::uint64_t{memory.ReadU32(array)} + offset;
        if (static_cast<GuestAddress>(live_r3) != 0)
            live_r3 = InitializeString(memory, resize_services, live_r3,
                0xFFFFFFFF821909C8ull, frame);
        if (!registered_constructor_family::Apply(0x8240B1B8u, memory,
                manager_services, registration_services, live_r3, frame, result))
            throw std::logic_error("registered singleton dependency missing");
        memory.WriteU32(object + 60u, static_cast<GuestAddress>(result));
        memory.WriteU32(object + 92u,
            (memory.ReadU32(object + 92u) & 0x7FFFFFFFu) | 0x20000000u);
        return true;
    }
    default:
        return false;
    }
}

} // namespace lo::semantic::gpu::registered_metadata_string
