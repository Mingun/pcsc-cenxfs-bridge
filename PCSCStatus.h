#ifndef PCSC_CENXFS_BRIDGE_Status_H
#define PCSC_CENXFS_BRIDGE_Status_H

#pragma once

#include <iomanip>
// Для HRESULT и DWORD
#include <windef.h>

/** Результат выполнения PC/SC функций. */
struct Status {
    LONG value;
    Status(LONG rv) : value(rv) {}

    operator bool() const { return value != 0; }

    inline const char* name() {
        static const char* names[] = {
            "SCARD_S_SUCCESS"                  , // NO_ERROR
            "SCARD_F_INTERNAL_ERROR"           , // _HRESULT_TYPEDEF_(0x80100001L)
            "SCARD_E_CANCELLED"                , // _HRESULT_TYPEDEF_(0x80100002L)
            "SCARD_E_INVALID_HANDLE"           , // _HRESULT_TYPEDEF_(0x80100003L)
            "SCARD_E_INVALID_PARAMETER"        , // _HRESULT_TYPEDEF_(0x80100004L)
            "SCARD_E_INVALID_TARGET"           , // _HRESULT_TYPEDEF_(0x80100005L)
            "SCARD_E_NO_MEMORY"                , // _HRESULT_TYPEDEF_(0x80100006L)
            "SCARD_F_WAITED_TOO_LONG"          , // _HRESULT_TYPEDEF_(0x80100007L)
            "SCARD_E_INSUFFICIENT_BUFFER"      , // _HRESULT_TYPEDEF_(0x80100008L)
            "SCARD_E_UNKNOWN_READER"           , // _HRESULT_TYPEDEF_(0x80100009L)
            "SCARD_E_TIMEOUT"                  , // _HRESULT_TYPEDEF_(0x8010000AL)
            "SCARD_E_SHARING_VIOLATION"        , // _HRESULT_TYPEDEF_(0x8010000BL)
            "SCARD_E_NO_SMARTCARD"             , // _HRESULT_TYPEDEF_(0x8010000CL)
            "SCARD_E_UNKNOWN_CARD"             , // _HRESULT_TYPEDEF_(0x8010000DL)
            "SCARD_E_CANT_DISPOSE"             , // _HRESULT_TYPEDEF_(0x8010000EL)
            "SCARD_E_PROTO_MISMATCH"           , // _HRESULT_TYPEDEF_(0x8010000FL)
            "SCARD_E_NOT_READY"                , // _HRESULT_TYPEDEF_(0x80100010L)
            "SCARD_E_INVALID_VALUE"            , // _HRESULT_TYPEDEF_(0x80100011L)
            "SCARD_E_SYSTEM_CANCELLED"         , // _HRESULT_TYPEDEF_(0x80100012L)
            "SCARD_F_COMM_ERROR"               , // _HRESULT_TYPEDEF_(0x80100013L)
            "SCARD_F_UNKNOWN_ERROR"            , // _HRESULT_TYPEDEF_(0x80100014L)
            "SCARD_E_INVALID_ATR"              , // _HRESULT_TYPEDEF_(0x80100015L)
            "SCARD_E_NOT_TRANSACTED"           , // _HRESULT_TYPEDEF_(0x80100016L)
            "SCARD_E_READER_UNAVAILABLE"       , // _HRESULT_TYPEDEF_(0x80100017L)
            "SCARD_P_SHUTDOWN"                 , // _HRESULT_TYPEDEF_(0x80100018L)
            "SCARD_E_PCI_TOO_SMALL"            , // _HRESULT_TYPEDEF_(0x80100019L)
            "SCARD_E_READER_UNSUPPORTED"       , // _HRESULT_TYPEDEF_(0x8010001AL)
            "SCARD_E_DUPLICATE_READER"         , // _HRESULT_TYPEDEF_(0x8010001BL)
            "SCARD_E_CARD_UNSUPPORTED"         , // _HRESULT_TYPEDEF_(0x8010001CL)
            "SCARD_E_NO_SERVICE"               , // _HRESULT_TYPEDEF_(0x8010001DL)
            "SCARD_E_SERVICE_STOPPED"          , // _HRESULT_TYPEDEF_(0x8010001EL)
            "SCARD_E_UNEXPECTED"               , // _HRESULT_TYPEDEF_(0x8010001FL)
            "SCARD_E_ICC_INSTALLATION"         , // _HRESULT_TYPEDEF_(0x80100020L)
            "SCARD_E_ICC_CREATEORDER"          , // _HRESULT_TYPEDEF_(0x80100021L)
            "SCARD_E_UNSUPPORTED_FEATURE"      , // _HRESULT_TYPEDEF_(0x80100022L)
            "SCARD_E_DIR_NOT_FOUND"            , // _HRESULT_TYPEDEF_(0x80100023L)
            "SCARD_E_FILE_NOT_FOUND"           , // _HRESULT_TYPEDEF_(0x80100024L)
            "SCARD_E_NO_DIR"                   , // _HRESULT_TYPEDEF_(0x80100025L)
            "SCARD_E_NO_FILE"                  , // _HRESULT_TYPEDEF_(0x80100026L)
            "SCARD_E_NO_ACCESS"                , // _HRESULT_TYPEDEF_(0x80100027L)
            "SCARD_E_WRITE_TOO_MANY"           , // _HRESULT_TYPEDEF_(0x80100028L)
            "SCARD_E_BAD_SEEK"                 , // _HRESULT_TYPEDEF_(0x80100029L)
            "SCARD_E_INVALID_CHV"              , // _HRESULT_TYPEDEF_(0x8010002AL)
            "SCARD_E_UNKNOWN_RES_MNG"          , // _HRESULT_TYPEDEF_(0x8010002BL)
            "SCARD_E_NO_SUCH_CERTIFICATE"      , // _HRESULT_TYPEDEF_(0x8010002CL)
            "SCARD_E_CERTIFICATE_UNAVAILABLE"  , // _HRESULT_TYPEDEF_(0x8010002DL)
            "SCARD_E_NO_READERS_AVAILABLE"     , // _HRESULT_TYPEDEF_(0x8010002EL)
            "SCARD_E_COMM_DATA_LOST"           , // _HRESULT_TYPEDEF_(0x8010002FL)
            "SCARD_E_NO_KEY_CONTAINER"         , // _HRESULT_TYPEDEF_(0x80100030L)
            "SCARD_E_SERVER_TOO_BUSY"          , // _HRESULT_TYPEDEF_(0x80100031L)
            NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,// xxx3x
            NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,// xxx4x
            NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,// xxx5x
            NULL,NULL,NULL,NULL,NULL,// xxx6x
            "SCARD_W_UNSUPPORTED_CARD"         , // _HRESULT_TYPEDEF_(0x80100065L)
            "SCARD_W_UNRESPONSIVE_CARD"        , // _HRESULT_TYPEDEF_(0x80100066L)
            "SCARD_W_UNPOWERED_CARD"           , // _HRESULT_TYPEDEF_(0x80100067L)
            "SCARD_W_RESET_CARD"               , // _HRESULT_TYPEDEF_(0x80100068L)
            "SCARD_W_REMOVED_CARD"             , // _HRESULT_TYPEDEF_(0x80100069L)
            "SCARD_W_SECURITY_VIOLATION"       , // _HRESULT_TYPEDEF_(0x8010006AL)
            "SCARD_W_WRONG_CHV"                , // _HRESULT_TYPEDEF_(0x8010006BL)
            "SCARD_W_CHV_BLOCKED"              , // _HRESULT_TYPEDEF_(0x8010006CL)
            "SCARD_W_EOF"                      , // _HRESULT_TYPEDEF_(0x8010006DL)
            "SCARD_W_CANCELLED_BY_USER"        , // _HRESULT_TYPEDEF_(0x8010006EL)
            "SCARD_W_CARD_NOT_AUTHENTICATED"   , // _HRESULT_TYPEDEF_(0x8010006FL)
            "SCARD_W_CACHE_ITEM_NOT_FOUND"     , // _HRESULT_TYPEDEF_(0x80100070L)
            "SCARD_W_CACHE_ITEM_STALE"         , // _HRESULT_TYPEDEF_(0x80100071L)
            "SCARD_W_CACHE_ITEM_TOO_BIG"       , // _HRESULT_TYPEDEF_(0x80100072L)
        };
        //  Values are 32 bit values laid out as follows:
        //
        //   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
        //   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
        //  +---+-+-+-----------------------+-------------------------------+
        //  |Sev|C|R|     Facility          |               Code            |
        //  +---+-+-+-----------------------+-------------------------------+
        //
        //  where
        //
        //      Sev - is the severity code
        //
        //          00 - Success
        //          01 - Informational
        //          10 - Warning
        //          11 - Error
        //
        //      C - is the Customer code flag
        //
        //      R - is a reserved bit
        //
        //      Facility - is the facility code
        //
        //      Code - is the facility's status code
        LONG i = (value & 0x0000FFFF);
        if (i < 0 || i > sizeof(names) / sizeof(names[0])) {
            return "<unknown>";
        }
        return names[i];
    }
    /// Конвертирует код возврата функций PC/SC в код возврата функций XFS.
    inline HRESULT translate() {
        switch (value) {/*
            case SCARD_S_SUCCESS:                 return WFS_SUCCESS;               // NO_ERROR
            case SCARD_F_INTERNAL_ERROR:          return WFS_ERR_INTERNAL_ERROR;    // _HRESULT_TYPEDEF_(0x80100001L)
            case SCARD_E_CANCELLED:               return ; // _HRESULT_TYPEDEF_(0x80100002L)
            case SCARD_E_INVALID_HANDLE:          return WFS_ERR_INVALID_HSERVICE;  // _HRESULT_TYPEDEF_(0x80100003L)
            case SCARD_E_INVALID_PARAMETER:       return ; // _HRESULT_TYPEDEF_(0x80100004L)
            case SCARD_E_INVALID_TARGET:          return ; // _HRESULT_TYPEDEF_(0x80100005L)
            case SCARD_E_NO_MEMORY:               return ; // _HRESULT_TYPEDEF_(0x80100006L)
            case SCARD_F_WAITED_TOO_LONG:         return ; // _HRESULT_TYPEDEF_(0x80100007L)
            case SCARD_E_INSUFFICIENT_BUFFER:     return ; // _HRESULT_TYPEDEF_(0x80100008L)
            case SCARD_E_UNKNOWN_READER:          return ; // _HRESULT_TYPEDEF_(0x80100009L)
            case SCARD_E_TIMEOUT:                 return WFS_ERR_TIMEOUT;           // _HRESULT_TYPEDEF_(0x8010000AL)
            case SCARD_E_SHARING_VIOLATION:       return WFS_ERR_LOCKED;            // _HRESULT_TYPEDEF_(0x8010000BL)
            case SCARD_E_NO_SMARTCARD:            return ; // _HRESULT_TYPEDEF_(0x8010000CL)
            case SCARD_E_UNKNOWN_CARD:            return ; // _HRESULT_TYPEDEF_(0x8010000DL)
            case SCARD_E_CANT_DISPOSE:            return ; // _HRESULT_TYPEDEF_(0x8010000EL)
            case SCARD_E_PROTO_MISMATCH:          return ; // _HRESULT_TYPEDEF_(0x8010000FL)
            case SCARD_E_NOT_READY:               return ; // _HRESULT_TYPEDEF_(0x80100010L)
            case SCARD_E_INVALID_VALUE:           return ; // _HRESULT_TYPEDEF_(0x80100011L)
            case SCARD_E_SYSTEM_CANCELLED:        return ; // _HRESULT_TYPEDEF_(0x80100012L)
            case SCARD_F_COMM_ERROR:              return WFS_ERR_HARDWARE_ERROR;    // _HRESULT_TYPEDEF_(0x80100013L)
            case SCARD_F_UNKNOWN_ERROR:           return ; // _HRESULT_TYPEDEF_(0x80100014L)
            case SCARD_E_INVALID_ATR:             return ; // _HRESULT_TYPEDEF_(0x80100015L)
            case SCARD_E_NOT_TRANSACTED:          return ; // _HRESULT_TYPEDEF_(0x80100016L)
            case SCARD_E_READER_UNAVAILABLE:      return WFS_ERR_CONNECTION_LOST;   // _HRESULT_TYPEDEF_(0x80100017L)
            case SCARD_P_SHUTDOWN:                return ; // _HRESULT_TYPEDEF_(0x80100018L)
            case SCARD_E_PCI_TOO_SMALL:           return ; // _HRESULT_TYPEDEF_(0x80100019L)
            case SCARD_E_READER_UNSUPPORTED:      return ; // _HRESULT_TYPEDEF_(0x8010001AL)
            case SCARD_E_DUPLICATE_READER:        return ; // _HRESULT_TYPEDEF_(0x8010001BL)
            case SCARD_E_CARD_UNSUPPORTED:        return ; // _HRESULT_TYPEDEF_(0x8010001CL)
            case SCARD_E_NO_SERVICE:              return WFS_ERR_INTERNAL_ERROR;    // _HRESULT_TYPEDEF_(0x8010001DL)
            case SCARD_E_SERVICE_STOPPED:         return ; // _HRESULT_TYPEDEF_(0x8010001EL)
            case SCARD_E_UNEXPECTED:              return ; // _HRESULT_TYPEDEF_(0x8010001FL)
            case SCARD_E_ICC_INSTALLATION:        return ; // _HRESULT_TYPEDEF_(0x80100020L)
            case SCARD_E_ICC_CREATEORDER:         return ; // _HRESULT_TYPEDEF_(0x80100021L)
            case SCARD_E_UNSUPPORTED_FEATURE:     return ; // _HRESULT_TYPEDEF_(0x80100022L)
            case SCARD_E_DIR_NOT_FOUND:           return ; // _HRESULT_TYPEDEF_(0x80100023L)
            case SCARD_E_FILE_NOT_FOUND:          return ; // _HRESULT_TYPEDEF_(0x80100024L)
            case SCARD_E_NO_DIR:                  return ; // _HRESULT_TYPEDEF_(0x80100025L)
            case SCARD_E_NO_FILE:                 return ; // _HRESULT_TYPEDEF_(0x80100026L)
            case SCARD_E_NO_ACCESS:               return ; // _HRESULT_TYPEDEF_(0x80100027L)
            case SCARD_E_WRITE_TOO_MANY:          return ; // _HRESULT_TYPEDEF_(0x80100028L)
            case SCARD_E_BAD_SEEK:                return ; // _HRESULT_TYPEDEF_(0x80100029L)
            case SCARD_E_INVALID_CHV:             return ; // _HRESULT_TYPEDEF_(0x8010002AL)
            case SCARD_E_UNKNOWN_RES_MNG:         return ; // _HRESULT_TYPEDEF_(0x8010002BL)
            case SCARD_E_NO_SUCH_CERTIFICATE:     return ; // _HRESULT_TYPEDEF_(0x8010002CL)
            case SCARD_E_CERTIFICATE_UNAVAILABLE: return ; // _HRESULT_TYPEDEF_(0x8010002DL)
            case SCARD_E_NO_READERS_AVAILABLE:    return ; // _HRESULT_TYPEDEF_(0x8010002EL)
            case SCARD_E_COMM_DATA_LOST:          return ; // _HRESULT_TYPEDEF_(0x8010002FL)
            case SCARD_E_NO_KEY_CONTAINER:        return ; // _HRESULT_TYPEDEF_(0x80100030L)
            case SCARD_E_SERVER_TOO_BUSY:         return ; // _HRESULT_TYPEDEF_(0x80100031L)
            case SCARD_W_UNSUPPORTED_CARD:        return ; // _HRESULT_TYPEDEF_(0x80100065L)
            case SCARD_W_UNRESPONSIVE_CARD:       return ; // _HRESULT_TYPEDEF_(0x80100066L)
            case SCARD_W_UNPOWERED_CARD:          return ; // _HRESULT_TYPEDEF_(0x80100067L)
            case SCARD_W_RESET_CARD:              return ; // _HRESULT_TYPEDEF_(0x80100068L)
            case SCARD_W_REMOVED_CARD:            return ; // _HRESULT_TYPEDEF_(0x80100069L)
            case SCARD_W_SECURITY_VIOLATION:      return ; // _HRESULT_TYPEDEF_(0x8010006AL)
            case SCARD_W_WRONG_CHV:               return ; // _HRESULT_TYPEDEF_(0x8010006BL)
            case SCARD_W_CHV_BLOCKED:             return ; // _HRESULT_TYPEDEF_(0x8010006CL)
            case SCARD_W_EOF:                     return ; // _HRESULT_TYPEDEF_(0x8010006DL)
            case SCARD_W_CANCELLED_BY_USER:       return ; // _HRESULT_TYPEDEF_(0x8010006EL)
            case SCARD_W_CARD_NOT_AUTHENTICATED:  return ; // _HRESULT_TYPEDEF_(0x8010006FL)
            case SCARD_W_CACHE_ITEM_NOT_FOUND:    return ; // _HRESULT_TYPEDEF_(0x80100070L)
            case SCARD_W_CACHE_ITEM_STALE:        return ; // _HRESULT_TYPEDEF_(0x80100071L)
            case SCARD_W_CACHE_ITEM_TOO_BIG:      return ; // _HRESULT_TYPEDEF_(0x80100072L)
            */
            // Успешно
            case SCARD_S_SUCCESS:           return WFS_SUCCESS;
            // Некорректный хендл карты (hCard)
            case SCARD_E_INVALID_HANDLE:    return WFS_ERR_INVALID_HSERVICE;
            // Ресурсный менеджер подсистемы PC/SC не запущен
            case SCARD_E_NO_SERVICE:        return WFS_ERR_INTERNAL_ERROR;
            // Считыватель был отключен
            case SCARD_E_READER_UNAVAILABLE:return WFS_ERR_CONNECTION_LOST;
            // Кто-то эксклюзивно владеет картой
            case SCARD_E_SHARING_VIOLATION: return WFS_ERR_LOCKED;
            // Внутренняя ошибка взаимодействия с устройством
            case SCARD_F_COMM_ERROR:        return WFS_ERR_HARDWARE_ERROR;
        }
        // TODO: Уточнить тип ошибки, возвращаемой в случае, если трансляция кодов ошибок не удалась.
        return WFS_ERR_INTERNAL_ERROR;
    }
};

template<class OS>
inline OS& operator<<(OS& os, Status s) {
    return os << "status=" << s.name() << " (0x"
              // На каждый байт требуется 2 символа.
              << std::hex << std::setw(2*sizeof(s.value)) << std::setfill('0')
              << s.value << ")";
}
#endif // PCSC_CENXFS_BRIDGE_Status_H