#include "lo_semantics/memory_fill.h"
#include "lo_semantics/detail/memory_fill_impl.h"

namespace lo::semantic::gpu
{

GuestAddress FillGuestMemory(GuestMemory& memory, GuestAddress destination,
    std::uint32_t value, std::uint32_t bytes)
{
    detail::FillMemory(memory, destination, value, bytes);
    return destination;
}

} // namespace lo::semantic::gpu
