#pragma once
#include "lo_semantics/crt_stream_close_shared_lower.h"
namespace lo::semantic::gpu::crt_narrow_formatter61 {
using Registers=crt_async_status_transfer::Registers;
class GuestServices {
public:
 virtual ~GuestServices()=default;
 // Actual mutable formatter slots and ADD70 output retain the complete
 // selected context. This boundary does not claim native import semantics.
 virtual void CallIndirect(GuestAddress,GuestMemory&,Registers&)=0;
 virtual void CallOutput(GuestMemory&,Registers&)=0;
};
struct Dependencies {crt_stream_close_shared_lower::Dependencies accepted;GuestServices& guest;};
// Complete narrow engine, repeated-character output and recursive buffer
// wrapper. Typed accepted lowers retain their selected ABI limits.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
