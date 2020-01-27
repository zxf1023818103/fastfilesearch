#include "fastfilesearch.h"
#include <Windows.h>

namespace ffs {

    String GetLastErrorMessage(ErrnoT lastError) {
        LPTSTR msg;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            lastError,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&msg,
            0,
            NULL
        );
        String s = msg;
        ::LocalFree(msg);
        return s;
    }

}