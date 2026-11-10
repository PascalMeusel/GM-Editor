#pragma once

#include "Models/Map/MapPackageServiceInternal.hpp"
#include <cctype>
#include <cfenv>
#include <stdexcept>
#if defined(_MSC_VER)
#include <float.h>
#endif
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <xmmintrin.h>
#endif

namespace gm::map_render::detail
{
inline void encodeCompileAmbientState(MapPackageServiceInternal::Writer &writer)
{
    // Label projection uses the C byte classification functions. Capture the
    // actual table, not just a locale name (Windows supports thread locales).
    // No locale is installed or changed, and no string/facet is borrowed.
    for (int byte = 0; byte < 256; ++byte)
    {
        writer.i32(std::tolower(byte)); writer.i32(std::toupper(byte));
        writer.boolean(std::isspace(byte) != 0); writer.boolean(std::isdigit(byte) != 0);
        writer.boolean(std::isalpha(byte) != 0); writer.boolean(std::isalnum(byte) != 0);
        writer.boolean(std::iscntrl(byte) != 0); writer.boolean(std::isgraph(byte) != 0);
        writer.boolean(std::isprint(byte) != 0); writer.boolean(std::ispunct(byte) != 0);
        writer.boolean(std::islower(byte) != 0); writer.boolean(std::isupper(byte) != 0);
        writer.boolean(std::isxdigit(byte) != 0); writer.boolean(std::isblank(byte) != 0);
    }
    const int rounding = std::fegetround();
    if (rounding < 0) throw std::runtime_error("Floating-point mode unavailable.");
    writer.i32(rounding);
    // Geometry uses ordinary floating arithmetic and libm. Bind rounding,
    // precision and denormal controls. Unmasked traps bypass memoization;
    // sticky exception flags do not affect arithmetic results and are omitted.
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    unsigned int control = 0;
    if (_controlfp_s(&control, 0, 0) != 0 || (control & _MCW_EM) != _MCW_EM)
        throw std::runtime_error("Unsupported floating-point control.");
#if defined(_EM_AMBIGUOUS)
    if ((control & _EM_AMBIGUOUS) != 0)
        throw std::runtime_error("Mixed floating-point control is ambiguous.");
#endif
    writer.u32(control);
#elif defined(__i386__) || defined(__x86_64__)
    unsigned short control = 0;
    __asm__ __volatile__("fnstcw %0" : "=m"(control));
    if ((control & 0x3fu) != 0x3fu)
        throw std::runtime_error("Floating-point traps are enabled.");
    writer.u32(control);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    std::uint64_t control = 0;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(control));
    if ((control & (std::uint64_t{0x9f} << 8)) != 0)
        throw std::runtime_error("Floating-point traps are enabled.");
    writer.u64(control);
#else
    // A new architecture needs an audited control-register guard first.
    throw std::runtime_error("Compile memo unavailable on this architecture.");
#endif
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
    const auto sse = _mm_getcsr();
    if ((sse & 0x1f80u) != 0x1f80u)
        throw std::runtime_error("SIMD floating-point traps are enabled.");
    writer.u32(sse & ~0x3fu);
#endif
}
} // namespace gm::map_render::detail
