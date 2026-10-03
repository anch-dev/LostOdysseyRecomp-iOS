# Floating-point initializer boundary, 2026-10-03

The instance component family and three metadata initializers only load single
words and store those loaded values without arithmetic. Their format movement
now uses [LoadedSingle](include/lo_semantics/loaded_single.h). It is a narrow model
for that instruction sequence, not a general implementation of `stfs` or floating
arithmetic.

The [PowerPC Programming Environments manual, Rev. 1](https://www.nxp.com/docs/en/user-guide/MPCFPE.pdf),
Appendix D.6 and D.7, printed pages D-18 through D-20 (PDF pages 766–768), gives
explicit bit mapping for load and store conversions. It preserves the signaling
NaN bit in format movement and normalizes single subnormals when loading an FPR.
The helper uses integer exponent/fraction operations; its store retains the word
captured at load time, including when later writes alias the constant address.
This is architectural evidence, without a measurement on Xenon hardware.

The generated C++ is a different comparison boundary. XenonRecomp's `lfs` emitter
uses a volatile big-endian word read followed by `double(temp.f32)`; `stfs` uses
`float(fpr)` before a volatile word store. Volatile memory access does not define
the intervening NaN format conversion. The existing `/O2` and `/Od` comparison
uses the same source word `0x7F800001` at `0x82000E50`, with object `0x00010000`:

| Implementation | FPR f0 bits | Word stored at object+64 |
| --- | --- | --- |
| Original generated C++, clang-cl /O2 | `7FF8000020000000` | `7F800001` |
| Original generated C++, clang-cl /Od | `7FF8000020000000` | `7FC00001` |
| Earlier semantic host casts, /O2 | `7FF8000020000000` | `7FC00001` |
| Current documented format model, /O2 | `7FF0000020000000` | `7F800001` |

Compiler folding of the load/store cast pair is a strong inference from these
observations; no disassembly diagnosis is claimed. The original receipts remain
in external scratch `semantic-metadata-initializer-tests`, including
`registered-metadata-initializers-snan-result.json` and
`snan-optimization-diagnostic.json`. The current FPR discrepancy is retained in
`registered-metadata-initializers-snan-isa-result.json` as
`known_mismatch_reproduced`, with `semantic_equivalence=false`.

The metadata API now calls `FpServices::DisableFlushMode()` at its first `lfs`,
after preceding integer reads/writes; an unknown entry makes no call. The component
API has the corresponding callback only on its non-null path. Five selected
metadata PPC comparisons and seven selected component cases passed; seven fixed
format vectors and the signaling-NaN unchanged store were also checked. These
checks do not resolve the generated-C++ signaling-NaN FPR difference.

No runtime wrapper was added for these initializers. The existing game scene
does not cover them. Changing the global emitter or regenerated PPC remains a
separate task because its effect would reach every floating load/store and the
current runtime comparison baseline.
