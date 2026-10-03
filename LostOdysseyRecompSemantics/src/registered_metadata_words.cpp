#include "lo_semantics/registered_metadata_words.h"

#include <algorithm>
#include <array>
#include <bit>
#include <iterator>

namespace lo::semantic::gpu::registered_metadata_words
{
namespace
{
struct Sequence
{
    GuestAddress address;
    std::uint32_t frame_size;
    std::uint32_t word_count;
    std::int32_t patch_previous_index;
    std::uint32_t clear_object_offset;
    std::array<std::uint32_t, 10> words;
};

constexpr Sequence kSequences[] = {
    // BEGIN GENERATED METADATA WORD PARAMETERS
    {0x82407B80u, 96, 1, -1, 0, {0x00010098u}},
    {0x8240C9B8u, 112, 2, -1, 0, {0x0001003Cu, 0x00010040u}},
    {0x8240CD38u, 112, 9, -1, 0, {0x00010044u, 0x00010048u, 0x0001004Cu, 0x0001006Cu, 0x00010070u, 0x00010074u, 0x00010078u, 0x0001007Cu, 0x00030080u}},
    {0x825AA760u, 96, 1, -1, 0, {0x0003003Cu}},
    {0x825AABD8u, 112, 8, -1, 0, {0x00010090u, 0x00030094u, 0x000300A0u, 0x00010144u, 0x00010148u, 0x0001014Cu, 0x00010150u, 0x00030154u}},
    {0x825B3338u, 112, 10, -1, 508, {0x00010050u, 0x000100CCu, 0x00030044u, 0x00010054u, 0x00010058u, 0x000100D0u, 0x00030124u, 0x00010148u, 0x0001014Cu, 0x0001016Cu}},
    {0x825E7698u, 112, 7, -1, 0, {0x00010060u, 0x00010068u, 0x0001006Cu, 0x00010070u, 0x00010074u, 0x00010078u, 0x0001007Cu}},
    {0x825E8728u, 112, 2, -1, 0, {0x00010080u, 0x00010084u}},
    {0x825EA998u, 96, 1, -1, 0, {0x00010084u}},
    {0x825EC9B0u, 96, 1, -1, 0, {0x00010080u}},
    {0x825F1B28u, 96, 1, -1, 0, {0x0001003Cu}},
    {0x8267A438u, 112, 7, 5, 0, {0x000100B0u, 0x00050EB8u, 0x00000004u, 0x000003FFu, 0x00010EB8u, 0x00014EECu, 0x00034F28u}},
    {0x8267A528u, 96, 1, -1, 0, {0x00014F60u}},
    {0x826D6F90u, 96, 1, -1, 0, {0x00010094u}},
    {0x826FA288u, 96, 1, -1, 0, {0x00010070u}},
    {0x82723D80u, 96, 1, -1, 0, {0x00010468u}},
    // END GENERATED METADATA WORD PARAMETERS
};

std::int32_t Signed(std::uint32_t value)
{
    return std::bit_cast<std::int32_t>(value);
}
} // namespace

std::uint64_t AddArrayElements(GuestMemory& memory,
    ArrayResizeServices& services, GuestAddress array, std::uint32_t count,
    std::uint32_t element_size, std::uint32_t argument)
{
    const std::uint32_t old_count = memory.ReadU32(array + 4u);
    const std::uint32_t capacity = memory.ReadU32(array + 8u);
    const std::uint32_t new_count = old_count + count;
    memory.WriteU32(array + 4u, new_count);
    if (Signed(new_count) > Signed(capacity))
    {
        // PPC wraps the doubled sum to a word, then srawi/addze divide the
        // signed result by eight with truncation toward zero.
        const std::uint32_t tripled = new_count + (new_count << 1u);
        const std::uint32_t slack = static_cast<std::uint32_t>(Signed(tripled) / 8);
        memory.WriteU32(array + 8u, new_count + slack + 32u);
        ResizeArray(memory, services, array, element_size, argument);
    }
    return old_count;
}

std::uint64_t AppendMetadataWord(GuestMemory& memory,
    ArrayResizeServices& services, GuestAddress array, GuestAddress value)
{
    const std::uint32_t index = static_cast<std::uint32_t>(
        AddArrayElements(memory, services, array, 1, 4, 8));
    const GuestAddress storage = memory.ReadU32(array);
    const GuestAddress destination = storage + (index << 2u);
    if (destination != 0)
        memory.WriteU32(destination, memory.ReadU32(value));
    return std::uint64_t{memory.ReadU32(array + 4u)} - 1u;
}

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& services, std::uint64_t object_register,
    GuestAddress caller_sp, std::uint64_t& result)
{
    const Sequence* sequence = std::lower_bound(std::begin(kSequences),
        std::end(kSequences), address,
        [](const Sequence& entry, GuestAddress target)
        { return entry.address < target; });
    if (sequence == std::end(kSequences) || sequence->address != address)
        return false;

    const GuestAddress object = static_cast<GuestAddress>(object_register);
    const GuestAddress array = memory.ReadU32(object + 52u) + 364u;
    const GuestAddress outgoing_word = caller_sp - sequence->frame_size + 80u;
    for (std::uint32_t index = 0; index < sequence->word_count; ++index)
    {
        GuestAddress captured_storage = 0;
        if (static_cast<std::int32_t>(index) == sequence->patch_previous_index)
            captured_storage = memory.ReadU32(array);
        memory.WriteU32(outgoing_word, sequence->words[index]);
        if (static_cast<std::int32_t>(index) == sequence->patch_previous_index)
        {
            const GuestAddress previous = captured_storage +
                (memory.ReadU32(array + 4u) << 2u) - 4u;
            memory.WriteU32(previous, memory.ReadU32(previous) + 0x01000000u);
        }
        result = AppendMetadataWord(memory, services, array, outgoing_word);
    }
    if (sequence->clear_object_offset != 0)
        memory.WriteU32(object + sequence->clear_object_offset, 0);
    return true;
}

} // namespace lo::semantic::gpu::registered_metadata_words
