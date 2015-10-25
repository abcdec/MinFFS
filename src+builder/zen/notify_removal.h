// **************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under *
// * GNU General Public License: http://www.gnu.org/licenses/gpl-3.0        *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved        *
// **************************************************************************

#ifndef NOTIFY_H_INCLUDED_257804267562
#define NOTIFY_H_INCLUDED_257804267562

#include <memory>
#include "win.h" //includes "windows.h"
#include "file_error.h"

//handle (user-) request for device removal via template method pattern
//evaluate directly after processing window messages
class NotifyRequestDeviceRemoval
{
public:
    NotifyRequestDeviceRemoval(HANDLE hDir); //throw FileError
    virtual ~NotifyRequestDeviceRemoval();

private:
    virtual void onRequestRemoval(HANDLE hnd) = 0; //throw()!
    //NOTE: onRemovalFinished is NOT guaranteed to execute after onRequestRemoval()! but most likely will
    virtual void onRemovalFinished(HANDLE hnd, bool successful) = 0; //throw()!

    NotifyRequestDeviceRemoval           (NotifyRequestDeviceRemoval&) = delete;
    NotifyRequestDeviceRemoval& operator=(NotifyRequestDeviceRemoval&) = delete;

    class Pimpl;
    std::unique_ptr<Pimpl> pimpl;
};

#endif //NOTIFY_H_INCLUDED_257804267562
