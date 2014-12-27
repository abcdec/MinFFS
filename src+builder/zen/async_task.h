// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************
// **************************************************************************
// * This file is modified from its original source file distributed by the *
// * FreeFileSync project: http://www.freefilesync.org/ version 6.12        *
// * Modifications made by abcdec @GitHub. https://github.com/abcdec/MinFFS *
// *                          --EXPERIMENTAL--                              *
// * This program is experimental and not recommended for general use.      *
// * Please consider using the original FreeFileSync program unless there   *
// * are specific needs to use this experimental MinFFS version.            *
// *                          --EXPERIMENTAL--                              *
// * This modified program is distributed in the hope that it will be       *
// * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
// * General Public License for more details.                               *
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
#ifdef TODO_MinFFS_UI
        tasks.push_back(zen::async([=]() -> std::function<void()>
        {
            auto result = doAsync();
            return [=]{ evalOnGui(result); };
        }));
#endif//TODO_MinFFS_UI
    }

    template <class Fun, class Fun2>
    void add2(Fun doAsync, Fun2 evalOnGui) //for evalOnGui taking no parameters
    {
#ifdef TODO_MinFFS_UI
        tasks.push_back(zen::async([=]() -> std::function<void()> { doAsync(); return [=]{ evalOnGui(); }; }));
#endif//TODO_MinFFS_UI
    }

    void evalResults() //call from gui thread repreatedly
    {
#ifdef TODO_MinFFS_UI
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
#endif//TODO_MinFFS_UI
    }

#ifdef TODO_MinFFS_UI
    bool empty() const { return tasks.empty(); }
#else//TODO_MinFFS_UI
    bool empty() const { return true; }
#endif//TODO_MinFFS_UI

private:
    bool inRecursion;
#ifdef TODO_MinFFS_UI
    std::list<boost::unique_future<std::function<void()>>> tasks;
#endif//TODO_MinFFS_UI
};
}

#endif //ASYNC_JOB_839147839170432143214321
