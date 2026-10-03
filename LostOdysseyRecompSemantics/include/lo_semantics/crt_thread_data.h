#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

struct CrtThreadDataCall
{
    std::uint64_t thread_environment = 0; // Live PPC r13, including upper bits.
};

class CrtThreadDataServices
{
public:
    virtual ~CrtThreadDataServices() = default;
    virtual std::uint64_t GetTlsValue(std::uint32_t index) = 0;
    virtual void SetTlsValue(std::uint32_t index, std::uint64_t value) = 0;
    virtual std::uint64_t CallThreadDataGetter(GuestAddress function,
                                               std::uint64_t context) = 0;
    virtual std::uint64_t CallThreadDataGetterWithState(GuestAddress function,
        std::uint64_t context, CrtThreadDataCall& call)
    {
        (void)call;
        return CallThreadDataGetter(function, context);
    }
    virtual std::uint64_t AllocateThreadData(std::uint32_t count,
                                             std::uint32_t bytes_each) = 0;
    virtual std::uint64_t BindThreadData(GuestAddress function,
                                         std::uint64_t context,
                                         std::uint64_t data) = 0;
    virtual std::uint64_t BindThreadDataWithState(GuestAddress function,
        std::uint64_t context, std::uint64_t data, CrtThreadDataCall& call)
    {
        (void)call;
        return BindThreadData(function, context, data);
    }
    virtual void FreeThreadData(std::uint64_t data) = 0;
};

// 822CA048 gets or creates the current CRT thread record. The r13-based
// environment and five observed process globals remain guest-memory inputs.
// External TLS, allocator, callback and release operations are explicit.
[[nodiscard]] std::uint64_t GetCrtThreadData(GuestMemory& memory,
    CrtThreadDataServices& services, GuestAddress thread_environment);

// Stateful entry for an ABI adapter. Indirect guest callbacks can change r13;
// subsequent thread-state loads and error restoration use its live value.
[[nodiscard]] std::uint64_t GetCrtThreadData(GuestMemory& memory,
    CrtThreadDataServices& services, CrtThreadDataCall& call);

class CrtErrorOutputServices
{
public:
    virtual ~CrtErrorOutputServices() = default;
    virtual void InitAnsiString(GuestMemory& memory, GuestAddress descriptor,
                                GuestAddress message) = 0;
    virtual std::uint64_t WriteAnsi(GuestAddress buffer, std::uint16_t length) = 0;
};

// 823ADD70 uses an eight-byte ANSI_STRING descriptor at caller_sp - 16.
// The imported initializer supplies the descriptor; the output sink receives
// its resulting buffer and length. ABI save/backchain words are outside scope.
[[nodiscard]] std::uint64_t OutputCrtErrorMessage(GuestMemory& memory,
    CrtErrorOutputServices& services, GuestAddress message,
    GuestAddress caller_sp);

} // namespace lo::semantic::gpu
