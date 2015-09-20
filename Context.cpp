#include "PCSC/Context.h"

#include "PCSC/Status.h"

#include "XFS/Logger.h"

namespace PCSC {
    Context::Context() {
        // Создаем контекст.
        Status st = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &hContext);
        XFS::Logger() << "SCardEstablishContext: " << st;
    }
    Context::~Context() {
        Status st = SCardReleaseContext(hContext);
        XFS::Logger() << "SCardReleaseContext: " << st;
    }
}