// Appended after kAccessorEntries; compare runtime dispatch with exact PPC.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <windows.h>

namespace
{

static_assert(std::is_trivially_copyable_v<PPCContext>);
constexpr std::size_t kAddressSpace = std::size_t{1} << 32;
constexpr std::uint32_t kTargets[] = {
    0x00001021u, 0x80001023u, 0xFFFFF02Du, 0x00001039u,
};

class SparseGuestSpace
{
public:
    SparseGuestSpace()
    {
        base_ = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, kAddressSpace, MEM_RESERVE, PAGE_NOACCESS));
        if (!base_) throw std::runtime_error("reserve guest address space");
        for (const std::uint32_t page : {0x00001000u, 0x80001000u, 0xFFFFF000u})
        {
            if (!VirtualAlloc(base_ + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit guest test page");
        }
    }

    ~SparseGuestSpace() { VirtualFree(base_, 0, MEM_RELEASE); }
    SparseGuestSpace(const SparseGuestSpace&) = delete;
    SparseGuestSpace& operator=(const SparseGuestSpace&) = delete;

    [[nodiscard]] std::uint8_t* data() const { return base_; }

private:
    std::uint8_t* base_ = nullptr;
};

void SetRegister(PPCContext& context, unsigned index, std::uint64_t value)
{
    switch (index)
    {
    case 3: context.r3.u64 = value; return;
    case 4: context.r4.u64 = value; return;
    case 5: context.r5.u64 = value; return;
    case 6: context.r6.u64 = value; return;
    case 13: context.r13.u64 = value; return;
    }
    throw std::invalid_argument("unsupported accessor register");
}

bool Test(const AccessorEntry& entry, std::size_t index, unsigned path,
          bool enabled, SparseGuestSpace& space)
{
    const std::uint32_t target = kTargets[(index + path) % std::size(kTargets)];
    const std::uint32_t register_base = target -
        static_cast<std::uint32_t>(entry.displacement);
    PPCContext actual{};
    auto* bytes = reinterpret_cast<std::uint8_t*>(&actual);
    for (std::size_t i = 0; i < sizeof(actual); ++i)
        bytes[i] = static_cast<std::uint8_t>(i * 37u + entry.address + path);
    SetRegister(actual, entry.base_register,
                0xFEDCBA9800000000ull | register_base);
    if (!entry.getter)
        SetRegister(actual, entry.value_register,
                    0x89ABCDEF00000000ull | (0xA5C33C5Au + index));
    PPCContext expected{};
    std::memcpy(&expected, &actual, sizeof(actual));

    std::array<std::uint8_t, 64> before{};
    for (std::size_t i = 0; i < before.size(); ++i)
        before[i] = static_cast<std::uint8_t>(i * 19u + entry.address + path);
    auto* window = space.data() + target - 32;
    std::memcpy(window, before.data(), before.size());
    const unsigned calls_before = g_original_calls[index];
    entry.original(expected, space.data());
    std::array<std::uint8_t, 64> expected_memory{};
    std::memcpy(expected_memory.data(), window, expected_memory.size());

    std::memcpy(window, before.data(), before.size());
    if (PPCFuncMappings[index].guest != entry.address) return false;
    (path == 0 ? DirectCalls[index] : PPCFuncMappings[index].host)(actual,
                                                                   space.data());
    const unsigned expected_calls = calls_before + (enabled ? 1u : 2u);
    if (g_original_calls[index] != expected_calls ||
        std::memcmp(&actual, &expected, sizeof(actual)) != 0 ||
        std::memcmp(window, expected_memory.data(), before.size()) != 0)
    {
        std::fprintf(stderr, "FAIL accessor runtime %08X path %u %s calls %u/%u\n",
                     entry.address, path, enabled ? "enabled" : "disabled",
                     g_original_calls[index], expected_calls);
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    const bool enabled = argc == 2 && std::strcmp(argv[1], "enabled") == 0;
    SparseGuestSpace space;
    for (unsigned path = 0; path != 2; ++path)
        for (std::size_t i = 0; i < std::size(kAccessorEntries); ++i)
            if (!Test(kAccessorEntries[i], i, path, enabled, space)) return 1;
    std::printf("PASS %zu accessor wrappers, direct + mapping, %s\n",
                std::size(kAccessorEntries), enabled ? "enabled" : "disabled");
}
