#ifndef PCSC_CENXFS_BRIDGE_PCSC_Context_H
#define PCSC_CENXFS_BRIDGE_PCSC_Context_H

#pragma once

#include <boost/noncopyable.hpp>

// PC/SC API
#include <winscard.h>

namespace PCSC {
    /** Контекст представляет собой соединение к подсистеме PC/SC. Выделен в отдельный класс
        главным образом по соображениям того, чтобы его деструктор закрывал контекст PC/SC
        в самый последний момент, когда все остальные объекты, зависящие от контекста, уже
        будут разрушены. Таким образом, предназначен для наследования от него класса менеджера.
    */
    class Context : private boost::noncopyable {
        /// Контекст подсистемы PC/SC.
        SCARDCONTEXT hContext;
    public:
        /// Открывает соединение к подсистеме PC/SC.
        Context();
        /// Закрывает соединение к подсистеме PC/SC.
        ~Context();
    public:// Доступ к внутренностям
        inline SCARDCONTEXT context() const { return hContext; }
    };
} // namespace PCSC
#endif PCSC_CENXFS_BRIDGE_PCSC_Context_H