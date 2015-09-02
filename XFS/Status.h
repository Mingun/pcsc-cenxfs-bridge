#ifndef PCSC_CENXFS_BRIDGE_XFS_Status_H
#define PCSC_CENXFS_BRIDGE_XFS_Status_H

#pragma once

#include <string>
#include <sstream>
#include <cassert>

// Для REQUESTID, HSERVICE, HRESULT, LONG и DWORD
#include <windef.h>
// Для PostMessage
#include <winuser.h>
// Для GetSystemTime
#include <WinBase.h>
// Коды ошибок
#include <xfsapi.h>

#include "Utils/CTString.h"
#include "Utils/Enum.h"
#include "PCSC/Status.h"
#include "XFS/Memory.h"

namespace XFS {
    class Status : public Enum<DWORD, Status> {
        typedef Enum<DWORD, Status> _Base;
    public:
        Status(DWORD value) : _Base(value) {}

        CTString name() const {
            static CTString names[] = {
                CTString("WFS_SUCCESS"                    ),// (0)
                CTString("WFS_ERR_ALREADY_STARTED"        ),// (-1)
                CTString("WFS_ERR_API_VER_TOO_HIGH"       ),// (-2)
                CTString("WFS_ERR_API_VER_TOO_LOW"        ),// (-3)
                CTString("WFS_ERR_CANCELED"               ),// (-4)
                CTString("WFS_ERR_CFG_INVALID_HKEY"       ),// (-5)
                CTString("WFS_ERR_CFG_INVALID_NAME"       ),// (-6)
                CTString("WFS_ERR_CFG_INVALID_SUBKEY"     ),// (-7)
                CTString("WFS_ERR_CFG_INVALID_VALUE"      ),// (-8)
                CTString("WFS_ERR_CFG_KEY_NOT_EMPTY"      ),// (-9)
                CTString("WFS_ERR_CFG_NAME_TOO_LONG"      ),// (-10)
                CTString("WFS_ERR_CFG_NO_MORE_ITEMS"      ),// (-11)
                CTString("WFS_ERR_CFG_VALUE_TOO_LONG"     ),// (-12)
                CTString("WFS_ERR_DEV_NOT_READY"          ),// (-13)
                CTString("WFS_ERR_HARDWARE_ERROR"         ),// (-14)
                CTString("WFS_ERR_INTERNAL_ERROR"         ),// (-15)
                CTString("WFS_ERR_INVALID_ADDRESS"        ),// (-16)
                CTString("WFS_ERR_INVALID_APP_HANDLE"     ),// (-17)
                CTString("WFS_ERR_INVALID_BUFFER"         ),// (-18)
                CTString("WFS_ERR_INVALID_CATEGORY"       ),// (-19)
                CTString("WFS_ERR_INVALID_COMMAND"        ),// (-20)
                CTString("WFS_ERR_INVALID_EVENT_CLASS"    ),// (-21)
                CTString("WFS_ERR_INVALID_HSERVICE"       ),// (-22)
                CTString("WFS_ERR_INVALID_HPROVIDER"      ),// (-23)
                CTString("WFS_ERR_INVALID_HWND"           ),// (-24)
                CTString("WFS_ERR_INVALID_HWNDREG"        ),// (-25)
                CTString("WFS_ERR_INVALID_POINTER"        ),// (-26)
                CTString("WFS_ERR_INVALID_REQ_ID"         ),// (-27)
                CTString("WFS_ERR_INVALID_RESULT"         ),// (-28)
                CTString("WFS_ERR_INVALID_SERVPROV"       ),// (-29)
                CTString("WFS_ERR_INVALID_TIMER"          ),// (-30)
                CTString("WFS_ERR_INVALID_TRACELEVEL"     ),// (-31)
                CTString("WFS_ERR_LOCKED"                 ),// (-32)
                CTString("WFS_ERR_NO_BLOCKING_CALL"       ),// (-33)
                CTString("WFS_ERR_NO_SERVPROV"            ),// (-34)
                CTString("WFS_ERR_NO_SUCH_THREAD"         ),// (-35)
                CTString("WFS_ERR_NO_TIMER"               ),// (-36)
                CTString("WFS_ERR_NOT_LOCKED"             ),// (-37)
                CTString("WFS_ERR_NOT_OK_TO_UNLOAD"       ),// (-38)
                CTString("WFS_ERR_NOT_STARTED"            ),// (-39)
                CTString("WFS_ERR_NOT_REGISTERED"         ),// (-40)
                CTString("WFS_ERR_OP_IN_PROGRESS"         ),// (-41)
                CTString("WFS_ERR_OUT_OF_MEMORY"          ),// (-42)
                CTString("WFS_ERR_SERVICE_NOT_FOUND"      ),// (-43)
                CTString("WFS_ERR_SPI_VER_TOO_HIGH"       ),// (-44)
                CTString("WFS_ERR_SPI_VER_TOO_LOW"        ),// (-45)
                CTString("WFS_ERR_SRVC_VER_TOO_HIGH"      ),// (-46)
                CTString("WFS_ERR_SRVC_VER_TOO_LOW"       ),// (-47)
                CTString("WFS_ERR_TIMEOUT"                ),// (-48)
                CTString("WFS_ERR_UNSUPP_CATEGORY"        ),// (-49)
                CTString("WFS_ERR_UNSUPP_COMMAND"         ),// (-50)
                CTString("WFS_ERR_VERSION_ERROR_IN_SRVC"  ),// (-51)
                CTString("WFS_ERR_INVALID_DATA"           ),// (-52)
                CTString("WFS_ERR_SOFTWARE_ERROR"         ),// (-53)
                CTString("WFS_ERR_CONNECTION_LOST"        ),// (-54)
                CTString("WFS_ERR_USER_ERROR"             ),// (-55)
                CTString("WFS_ERR_UNSUPP_DATA"            ),// (-56)
            };
            return _Base::name(names, (~((DWORD)0)) - mValue + 1);
        }
    };
} // namespace XFS
#endif PCSC_CENXFS_BRIDGE_XFS_Status_H