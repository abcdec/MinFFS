// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef ASYNC_JOB_839147839170432143214321
#define ASYNC_JOB_839147839170432143214321

#include <list>
#include <functional>
#include "thread.h"
#include "scope_guard.h"
//#include "type_tools.h"

namespace zen
{
//run a job in an async thread, but process result on GUI event loop
class AsyncTasks
{
public:
    AsyncTasks() : inRecursion(false) {}

    template <class Fun, class Fun2>
    void add(Fun doAsync, Fun2 evalOnGui)
    //equivalent to "evalOnGui(doAsync())"
    //	-> doAsync: the usual thread-safety requirements apply!
    //	-> evalOnGui: no thread-safety concerns, but must only reference variables with greater-equal lifetime than the AsyncTask instance!
    {
        tasks.push_back(zen::async([=]() -> std::function<void()>
        {
            auto result = doAsync();
            return [=]{ evalOnGui(result); };
        }));
    }

    template <class Fun, class Fun2>
    void add2(Fun doAsync, Fun2 evalOnGui) //for evalOnGui taking no parameters
    {
        tasks.push_back(zen::async([=]() -> std::function<void()> { doAsync(); return [=]{ evalOnGui(); }; }));
    }

    void evalResults() //call from gui thread repreatedly
    {
        if (!inRecursion) //prevent implicit recursion, e.g. if we're called from an idle event and spawn another one via the callback below
        {
            inRecursion = true;
            ZEN_ON_SCOPE_EXIT(inRecursion = false);

            tasks.remove_if([](boost::unique_future<std::function<void()>>& ft) -> bool
            {
                if (ft.is_ready())
                {
                    (ft.get())();
                    return true;
                }
                return false;
            });
        }
    }

    bool empty() const { return tasks.empty(); }

private:
    bool inRecursion;
    std::list<boost::unique_future<std::function<void()>>> tasks;
};
}

#endif //ASYNC_JOB_839147839170432143214321
