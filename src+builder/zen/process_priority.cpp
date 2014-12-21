// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#include "process_priority.h"
//#include "sys_error.h"
#include "i18n.h"

#ifdef ZEN_WIN
    #include "win.h" //includes "windows.h"
#endif

using namespace zen;


#ifdef ZEN_WIN
struct PreventStandby::Pimpl {};

PreventStandby::PreventStandby()
{
    if (::SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED) == 0)
        throw FileError(_("Unable to suspend system sleep mode.")); //no GetLastError() support?
}


PreventStandby::~PreventStandby()
{
    ::SetThreadExecutionState(ES_CONTINUOUS);
}


#ifndef PROCESS_MODE_BACKGROUND_BEGIN
    #define PROCESS_MODE_BACKGROUND_BEGIN     0x00100000 // Windows Server 2003 and Windows XP/2000:  This value is not supported!
    #define PROCESS_MODE_BACKGROUND_END       0x00200000 //
#endif

struct ScheduleForBackgroundProcessing::Pimpl {};


ScheduleForBackgroundProcessing::ScheduleForBackgroundProcessing()
{
    if (!::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN)) //this call lowers CPU priority, too!!
        throwFileError(_("Cannot change process I/O priorities."), L"SetPriorityClass", getLastError());
}


ScheduleForBackgroundProcessing::~ScheduleForBackgroundProcessing()
{
    ::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END);
}

#elif defined ZEN_LINUX
struct PreventStandby::Pimpl {};
PreventStandby::PreventStandby() {}
PreventStandby::~PreventStandby() {}

//solution for GNOME?: http://people.gnome.org/~mccann/gnome-session/docs/gnome-session.html#org.gnome.SessionManager.Inhibit

struct ScheduleForBackgroundProcessing::Pimpl {};
ScheduleForBackgroundProcessing::ScheduleForBackgroundProcessing() {};
ScheduleForBackgroundProcessing::~ScheduleForBackgroundProcessing() {};

/*
struct ScheduleForBackgroundProcessing
{
	- required functions ioprio_get/ioprio_set are not part of glibc: http://linux.die.net/man/2/ioprio_set
	- and probably never will: http://sourceware.org/bugzilla/show_bug.cgi?id=4464
	- /usr/include/linux/ioprio.h not available on Ubuntu, so we can't use it instead

	ScheduleForBackgroundProcessing() : oldIoPrio(getIoPriority(IOPRIO_WHO_PROCESS, ::getpid()))
	{
		if (oldIoPrio != -1)
			setIoPriority(::getpid(), IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0));
	}
	~ScheduleForBackgroundProcessing()
	{
		if (oldIoPrio != -1)
			setIoPriority(::getpid(), oldIoPrio);
	}

private:
	static int getIoPriority(pid_t pid)
	{
		return ::syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, pid);
	}
	static int setIoPriority(pid_t pid, int ioprio)
	{
		return ::syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, pid, ioprio);
	}

	const int oldIoPrio;
};
*/
#endif
