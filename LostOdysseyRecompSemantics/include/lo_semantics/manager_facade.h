#pragma once

#include "lo_semantics/allocation_array.h"
#include "lo_semantics/manager_init.h"

namespace lo::semantic::gpu
{

// The manager's vtable methods remain guest callbacks. The common manager
// initializer and array resize/remove operations use recovered implementations.
class ManagerFacadeServices : public ManagerInitServices
{
public:
    virtual std::uint64_t ReleaseStorage(GuestAddress method,
        std::uint64_t manager_register, std::uint64_t buffer_register) = 0;
    virtual std::uint64_t AllocateStorage(GuestAddress method,
        std::uint64_t manager_register, std::uint64_t bytes_register,
        std::uint64_t alignment_register) = 0;
    virtual GuestAddress ResizeStorage(GuestAddress method,
        GuestAddress manager, GuestAddress old_storage,
        std::uint32_t bytes, std::uint32_t argument) = 0;
};

// 823F3340 and 82486C88. Both lazily initialize the global manager, reload
// it after initialization, and preserve the full r3 returned by its vtable.
[[nodiscard]] std::uint64_t ReleaseManagerBuffer(GuestMemory& memory,
    ManagerFacadeServices& services, std::uint64_t buffer_register,
    GuestAddress caller_sp);
[[nodiscard]] std::uint64_t AllocateManagerBuffer(GuestMemory& memory,
    ManagerFacadeServices& services, std::uint64_t bytes_register,
    GuestAddress caller_sp);

// 823F3548: release a non-null data pointer, then clear header words 0,8,4.
[[nodiscard]] std::uint64_t ClearBufferHeader(GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress header,
    GuestAddress caller_sp);

// 82298A98: remove every two-byte element, release any remaining storage,
// then clear the same three header words. Its nested remove frame is 128 bytes.
[[nodiscard]] std::uint64_t ReleaseTwoByteArray(GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress array,
    GuestAddress caller_sp);

// Array-release family: remove the current element range with its original
// element size and resize argument, reload storage after the nested call,
// release non-null storage, then clear header words in 0,8,4 order.
[[nodiscard]] std::uint64_t ReleaseArrayElements(GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress array,
    std::uint32_t element_size, std::uint32_t resize_argument,
    GuestAddress caller_sp);

// 82298938: clear count, shrink a nonzero capacity, then release the array.
[[nodiscard]] std::uint64_t ResetTwoByteArray(GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress array,
    GuestAddress caller_sp);

} // namespace lo::semantic::gpu
