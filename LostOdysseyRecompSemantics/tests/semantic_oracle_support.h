#pragma once

#ifndef _WIN32
#error "Semantic oracle guest windows require Windows VirtualAlloc"
#endif

#include "lo_semantics/guest_memory.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace lo::semantic::gpu::test
{

struct Region
{
    GuestAddress base;
    std::size_t size;
    bool operator==(const Region&) const = default;
};

// An addressable 4 GiB window with storage only for the listed regions.
class GuestWindow
{
public:
    static_assert(sizeof(void*) >= 8, "guest window needs a 64-bit host");
    static constexpr std::size_t Space = std::size_t{1} << 32;

    explicit GuestWindow(std::span<const Region> regions)
        : regions_(regions.begin(), regions.end())
    {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const auto page = static_cast<std::size_t>(info.dwPageSize);
        bytes_ = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
        if (!bytes_)
            throw std::runtime_error("reserve guest address window");

        try
        {
            for (const auto region : regions_)
            {
                if (!region.size || region.size > Space - region.base ||
                    region.base % page || region.size % page)
                    throw std::invalid_argument("invalid guest region");
                if (!VirtualAlloc(bytes_ + region.base, region.size,
                        MEM_COMMIT, PAGE_READWRITE))
                    throw std::runtime_error("commit guest region");
            }
        }
        catch (...)
        {
            VirtualFree(bytes_, 0, MEM_RELEASE);
            bytes_ = nullptr;
            throw;
        }
    }

    ~GuestWindow() { if (bytes_) VirtualFree(bytes_, 0, MEM_RELEASE); }
    GuestWindow(const GuestWindow&) = delete;
    GuestWindow& operator=(const GuestWindow&) = delete;

    [[nodiscard]] std::uint8_t* Bytes() const { return bytes_; }
    // Like the original oracle windows, only committed regions may be touched.
    [[nodiscard]] GuestMemory Memory() const
    { return GuestMemory(0, std::span<std::uint8_t>(bytes_, Space)); }

    void Fill(std::uint8_t value)
    {
        for (const auto region : regions_)
            std::memset(bytes_ + region.base, value, region.size);
    }

    [[nodiscard]] bool EqualCommitted(const GuestWindow& other) const
    {
        if (regions_ != other.regions_)
            return false;
        for (const auto region : regions_)
            if (std::memcmp(bytes_ + region.base,
                    other.bytes_ + region.base, region.size) != 0)
                return false;
        return true;
    }

private:
    std::vector<Region> regions_;
    std::uint8_t* bytes_ = nullptr;
};

} // namespace lo::semantic::gpu::test
