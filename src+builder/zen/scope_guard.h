// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef SCOPE_GUARD_H_8971632487321434
#define SCOPE_GUARD_H_8971632487321434

#include <cassert>
#include <exception>
#include <type_traits> //std::decay

//best of Zen, Loki and C++17


#ifdef ZEN_WIN
inline int getUncaughtExceptionCount() { return std::uncaught_exceptions(); }

#elif defined ZEN_LINUX || defined ZEN_MAC
//std::uncaught_exceptions() currently unsupported on GCC and Clang => clean up ASAP
#ifdef ZEN_LINUX
    static_assert(__GNUC__ < 5 || (__GNUC__ == 5 && (__GNUC_MINOR__ < 2 || (__GNUC_MINOR__ == 2 && __GNUC_PATCHLEVEL__ <= 1))), "check std::uncaught_exceptions support");
#else
    static_assert(__clang_major__ < 7 || (__clang_major__ == 7 && __clang_minor__ <= 0), "check std::uncaught_exceptions support");
#endif

namespace __cxxabiv1
{
struct __cxa_eh_globals;
extern "C" __cxa_eh_globals* __cxa_get_globals() noexcept;
}

inline int getUncaughtExceptionCount()
{
    return *(reinterpret_cast<unsigned int*>(static_cast<char*>(static_cast<void*>(__cxxabiv1::__cxa_get_globals())) + sizeof(void*)));
}
#endif


namespace zen
{
//Scope Guard
/*
    auto guardAio = zen::makeGuard<ScopeGuardRunMode::ON_EXIT>([&] { ::CloseHandle(hDir); });
        ...
    guardAio.dismiss();

Scope Exit:
    ZEN_ON_SCOPE_EXIT(::CloseHandle(hDir));
    ZEN_ON_SCOPE_FAIL(UndoPreviousWork());
    ZEN_ON_SCOPE_SUCCESS(NotifySuccess());
*/

enum class ScopeGuardRunMode
{
    ON_EXIT,
    ON_SUCCESS,
    ON_FAIL
};


template <ScopeGuardRunMode runMode, typename F>
class ScopeGuard
{
public:
    explicit ScopeGuard(const F&  fun) : fun_(fun) {}
    explicit ScopeGuard(      F&& fun) : fun_(std::move(fun)) {}

    ScopeGuard(ScopeGuard&& other) : fun_(std::move(other.fun_)),
        exeptionCount(other.exeptionCount),
        dismissed(other.dismissed) { other.dismissed = true; }

    ~ScopeGuard() noexcept(runMode != ScopeGuardRunMode::ON_SUCCESS)
    {
#ifdef _MSC_VER
		#pragma warning(suppress: 4127) //"conditional expression is constant"
#endif
        if (!dismissed)
        {
            if (runMode != ScopeGuardRunMode::ON_EXIT)
            {
                const bool failed = getUncaughtExceptionCount() > exeptionCount;
                if ((runMode == ScopeGuardRunMode::ON_FAIL) != failed)
                    return;
            }

            if (runMode == ScopeGuardRunMode::ON_SUCCESS)
                fun_(); //throw X
            else
                try { fun_(); }
                catch (...) { assert(false); } //consistency: don't expect exceptions for ON_EXIT even if "!failed"!
        }
    }

    void dismiss() { dismissed = true; }

private:
    ScopeGuard           (const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    F fun_;
    const int exeptionCount = getUncaughtExceptionCount();
    bool dismissed = false;
};


template <ScopeGuardRunMode runMode, class F> inline
auto makeGuard(F&& fun) { return ScopeGuard<runMode, std::decay_t<F>>(std::forward<F>(fun)); }
}

#define ZEN_CONCAT_SUB(X, Y) X ## Y
#define ZEN_CONCAT(X, Y) ZEN_CONCAT_SUB(X, Y)

#define ZEN_ON_SCOPE_EXIT(X)    auto ZEN_CONCAT(dummy, __LINE__) = zen::makeGuard<zen::ScopeGuardRunMode::ON_EXIT   >([&]{ X; }); (void)ZEN_CONCAT(dummy, __LINE__);
#define ZEN_ON_SCOPE_FAIL(X)    auto ZEN_CONCAT(dummy, __LINE__) = zen::makeGuard<zen::ScopeGuardRunMode::ON_FAIL   >([&]{ X; }); (void)ZEN_CONCAT(dummy, __LINE__);
#define ZEN_ON_SCOPE_SUCCESS(X) auto ZEN_CONCAT(dummy, __LINE__) = zen::makeGuard<zen::ScopeGuardRunMode::ON_SUCCESS>([&]{ X; }); (void)ZEN_CONCAT(dummy, __LINE__);

#endif //SCOPE_GUARD_H_8971632487321434
