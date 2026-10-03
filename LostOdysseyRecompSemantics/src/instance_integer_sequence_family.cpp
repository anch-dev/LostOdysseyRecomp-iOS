#include "lo_semantics/instance_integer_sequence_family.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::instance_integer_sequence_family
{
namespace
{

enum class Layout : std::uint8_t
{
    TextBufferFactory, Commandlet, XeAudioDevice, ShaderCache,
    GameViewportClient, Exporter, Factory, ShadowMap1D,
    DrawString, DrawStringLate, StaticMesh, AudioDevice,
};

struct Spec
{
    GuestAddress address;
    Layout layout;
    GuestAddress vtable;
    GuestAddress first_word;
    GuestAddress second_word;
    GuestAddress third_word;
    GuestAddress fourth_word;
    std::uint64_t doubleword;
};

// Every row is pinned to one complete cached PPC body by the generator.
constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE INTEGER SEQUENCE PARAMETERS
    {0x8240af30u, Layout::TextBufferFactory, 0x821911c0u, 0u, 0u, 0u, 0u, 0ull},
    {0x8245c678u, Layout::Commandlet, 0x821a75a0u, 0u, 0u, 0u, 0u, 0ull},
    {0x8245c710u, Layout::Commandlet, 0x821a7490u, 0u, 0u, 0u, 0u, 0ull},
    {0x824772f8u, Layout::XeAudioDevice, 0x82005508u, 0x82062cc0u, 0x82000d04u, 0u, 0u, 0ull},
    {0x824c1328u, Layout::ShaderCache, 0x821ad350u, 0u, 0u, 0u, 0u, 0ull},
    {0x825e2a98u, Layout::GameViewportClient, 0x82003d38u, 0x821de5f0u, 0x82062cc0u, 0x82002748u, 0x82000d20u, 0x04023bc200003362ull},
    {0x825f2860u, Layout::Exporter, 0x821e0160u, 0u, 0u, 0u, 0u, 0ull},
    {0x825f4450u, Layout::Factory, 0x82190b30u, 0u, 0u, 0u, 0u, 0ull},
    {0x826980d0u, Layout::ShadowMap1D, 0x821fc2e8u, 0x8218d6f8u, 0x821cb43cu, 0x821fc3f0u, 0x82203160u, 0ull},
    {0x826a0d28u, Layout::DrawString, 0x821fded8u, 0x821590c4u, 0x821fe010u, 0u, 0u, 0ull},
    {0x826a0ea0u, Layout::DrawStringLate, 0x821fe020u, 0x821590c4u, 0x821fe010u, 0u, 0u, 0ull},
    {0x826a0fb8u, Layout::DrawStringLate, 0x821fe158u, 0x821590c4u, 0x821fe010u, 0u, 0u, 0ull},
    {0x826e0918u, Layout::StaticMesh, 0x822031f8u, 0u, 0u, 0u, 0u, 0ull},
    {0x826f0c38u, Layout::AudioDevice, 0x821a8150u, 0x82062cc0u, 0x821a828cu, 0u, 0u, 0ull},
    // END GENERATED INSTANCE INTEGER SEQUENCE PARAMETERS
};

void ClearWords(GuestMemory& memory, GuestAddress object,
    GuestAddress first, GuestAddress last)
{
    for (GuestAddress offset = first; offset <= last; offset += 4u)
        memory.WriteU32(object + offset, 0u);
}

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

void DrawStringBytes(GuestMemory& memory, GuestAddress object)
{
    memory.WriteU8(object + 160u, 3u);
    memory.WriteU8(object + 161u, 3u);
    memory.WriteU8(object + 188u, 3u);
    memory.WriteU8(object + 189u, 3u);
}

void Initialize(const Spec& spec, GuestMemory& memory,
    GuestAddress object, GuestAddress caller_sp)
{
    switch (spec.layout)
    {
    case Layout::TextBufferFactory:
    {
        ClearWords(memory, object, 68u, 76u);
        ClearWords(memory, object, 100u, 108u);
        const auto flags = memory.ReadU32(object + 92u);
        memory.WriteU32(object, spec.vtable);
        memory.WriteU32(object + 92u, flags & ~0x40000000u);
        break;
    }
    case Layout::Factory:
    {
        memory.WriteU32(object, spec.vtable);
        ClearWords(memory, object, 68u, 76u);
        ClearWords(memory, object, 100u, 108u);
        const auto flags = memory.ReadU32(object + 92u);
        memory.WriteU32(object + 92u, flags & ~0x40000000u);
        break;
    }
    case Layout::Exporter:
    {
        const auto flags = memory.ReadU32(object + 96u);
        memory.WriteU32(object, spec.vtable);
        memory.WriteU32(object + 96u, flags & ~0x40000000u);
        break;
    }
    case Layout::Commandlet:
    {
        const auto flags = ReadU64(memory, object + 8u);
        memory.WriteU32(object, spec.vtable);
        if ((static_cast<std::uint32_t>(flags) & 0x200u) == 0)
            WriteU64(memory, object + 8u, flags | 0x4000u);
        break;
    }
    case Layout::DrawString:
        memory.WriteU32(object + 108u, spec.first_word);
        memory.WriteU32(object, spec.vtable);
        memory.WriteU32(object + 108u, spec.second_word);
        memory.WriteU32(object + 112u, 0u);
        memory.WriteU32(object + 116u, 0u);
        DrawStringBytes(memory, object);
        break;
    case Layout::DrawStringLate:
        memory.WriteU32(object + 108u, spec.first_word);
        memory.WriteU32(object + 112u, 0u);
        memory.WriteU32(object + 116u, 0u);
        DrawStringBytes(memory, object);
        memory.WriteU32(object, spec.vtable);
        memory.WriteU32(object + 108u, spec.second_word);
        break;
    case Layout::ShaderCache:
        memory.WriteU32(object, spec.vtable);
        ClearWords(memory, object, 60u, 72u);
        memory.WriteU32(object + 76u, 8u);
        ClearWords(memory, object, 80u, 92u);
        memory.WriteU32(object + 96u, 8u);
        ClearWords(memory, object, 100u, 124u);
        break;
    case Layout::GameViewportClient:
        memory.WriteU32(object + 60u, spec.first_word);
        memory.WriteU32(object + 64u, spec.second_word);
        memory.WriteU32(object, spec.vtable);
        memory.WriteU32(object + 60u, spec.third_word);
        memory.WriteU32(object + 64u, spec.fourth_word);
        ClearWords(memory, object, 76u, 84u);
        WriteU64(memory, object + 100u, spec.doubleword);
        break;
    case Layout::ShadowMap1D:
    {
        memory.WriteU32(object, spec.first_word);
        memory.WriteU32(object + 68u, 0u);
        memory.WriteU32(object + 72u, 0u);
        memory.WriteU32(object + 60u, spec.second_word);
        const auto flags = memory.ReadU32(object + 76u);
        memory.WriteU32(object + 76u, flags & ~0x80000000u);
        memory.WriteU32(object + 80u, 0u);
        memory.WriteU32(object, spec.vtable);
        memory.WriteU32(object + 60u, spec.third_word);
        ClearWords(memory, object, 88u, 96u);
        memory.WriteU32(object + 84u, spec.fourth_word);
        memory.WriteU32(object + 116u, 0u);
        break;
    }
    case Layout::StaticMesh:
        memory.WriteU32(object, spec.vtable);
        ClearWords(memory, object, 60u, 80u);
        ClearWords(memory, object, 116u, 136u);
        ClearWords(memory, object, 172u, 204u);
        memory.WriteU32(object + 232u, 0u);
        break;
    case Layout::AudioDevice:
        memory.WriteU32(object + 60u, spec.first_word);
        memory.WriteU32(object, spec.vtable);
        memory.WriteU32(object + 60u, spec.second_word);
        ClearWords(memory, object, 108u, 132u);
        ClearWords(memory, object, 156u, 168u);
        memory.WriteU32(object + 172u, 8u);
        ClearWords(memory, object, 176u, 188u);
        memory.WriteU32(object + 192u, 8u);
        break;
    case Layout::XeAudioDevice:
    {
        // These staged words are live guest stack memory, not local C++ temps.
        const GuestAddress tail = object + 228u;
        const GuestAddress audio = object + 108u;
        memory.WriteU32(caller_sp - 4u, tail);
        memory.WriteU32(object + 60u, spec.first_word);
        memory.WriteU32(audio, 0u);
        memory.WriteU32(audio + 8u, 0u);
        memory.WriteU32(audio + 4u, 0u);
        ClearWords(memory, audio, 12u, 20u);
        memory.WriteU32(caller_sp - 12u, spec.vtable);
        memory.WriteU32(audio + 24u, 0u);
        memory.WriteU32(caller_sp - 16u, 0u);
        memory.WriteU32(caller_sp - 8u, spec.second_word);
        const GuestAddress staged_object = memory.ReadU32(caller_sp + 20u);
        memory.WriteU32(staged_object + 172u, 8u);
        ClearWords(memory, staged_object, 156u, 168u);
        memory.WriteU32(staged_object + 192u, 8u);
        ClearWords(memory, staged_object, 176u, 188u);
        memory.WriteU32(staged_object, memory.ReadU32(caller_sp - 12u));
        memory.WriteU32(staged_object + 60u, memory.ReadU32(caller_sp - 8u));
        ClearWords(memory, staged_object, 216u, 224u);
        const GuestAddress staged_tail = memory.ReadU32(caller_sp - 4u);
        ClearWords(memory, staged_tail, 0u, 24u);
        break;
    }
    }
}

} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, GuestAddress caller_sp, std::uint64_t& result)
{
    const Spec* spec = std::lower_bound(std::begin(kSpecs), std::end(kSpecs),
        address, [](const Spec& entry, GuestAddress target)
        { return entry.address < target; });
    if (spec == std::end(kSpecs) || spec->address != address)
        return false;

    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    if (spec->layout == Layout::XeAudioDevice)
        memory.WriteU32(caller_sp + 20u, object);
    if (object != 0)
        Initialize(*spec, memory, object, caller_sp);
    result = incoming_r3;
    return true;
}

} // namespace lo::semantic::gpu::instance_integer_sequence_family
