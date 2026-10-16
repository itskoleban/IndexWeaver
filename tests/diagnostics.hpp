#pragma once
#include <cstdio>
#include <cstdlib>
#include <exception>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

// Checked-STL/CRT failures must fail unattended CI with a useful diagnostic,
// rather than wait for a desktop assertion dialog or Windows Error Reporting.
inline void configureTestDiagnostics() noexcept {
    std::set_terminate([] {
        std::fputs("Unexpected std::terminate in test executable", stderr);
        if (auto exception = std::current_exception()) {
            try { std::rethrow_exception(exception); }
            catch (const std::exception& error) { std::fprintf(stderr, ": %s", error.what()); }
            catch (...) { std::fputs(": non-standard exception", stderr); }
        }
        std::fputc('\n', stderr);
        std::fflush(stderr);
        std::abort();
    });
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(_WRITE_ABORT_MSG, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
}
