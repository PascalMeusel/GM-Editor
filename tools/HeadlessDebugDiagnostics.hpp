#pragma once

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#include <cstdio>
#include <cstdlib>
#endif

// Runtime assertions must fail a service test with a diagnostic rather than
// opening an inaccessible desktop dialog or disappearing from its report.
inline void enableHeadlessDebugDiagnostics()
{
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportHook([](int, char *message, int *) -> int {
        std::fputs(message ? message : "MSVC runtime assertion", stderr);
        std::fflush(stderr);
        std::_Exit(EXIT_FAILURE);
    });
#endif
}
