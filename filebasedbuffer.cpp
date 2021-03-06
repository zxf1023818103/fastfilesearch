#include <Windows.h>
#include "fastfilesearch.h"

#define TRYWINAPI(result) { \
            if (!(result)) { \
                lastError = ::GetLastError(); \
                return false; \
            } \
        }

#define INIT_ELSE(value) { \
            if (!init) { \
                if (!Initialize()) { \
                    ::DebugBreak(); \
                    return (value); \
                } \
            }\
        }

#define INIT() INIT_ELSE(false)

#define INIT_ELSE_NULL() INIT_ELSE(nullptr)

namespace ffs {

    DWORD GetAllocationGranularity() {
        SYSTEM_INFO info;
        ::GetSystemInfo(&info);
        return info.dwAllocationGranularity;
    }

    /// 基于临时文件的缓冲区类
    class FileBasedBuffer : public IBuffer {
    private:
        bool init = false;

        bool baseAddressChangeable = false;

        HANDLE temporalFileHandle = INVALID_HANDLE_VALUE;

        HANDLE temporalFileMapping = INVALID_HANDLE_VALUE;

        size_t bytesSize;

        size_t bytesCapacity;

        void* buffer;

        DWORD lastError = ERROR_SUCCESS;

        DWORD allocationGranularity = ffs::GetAllocationGranularity();

    private:
        bool SetCapacityBytes(size_t newBytesCapacity) {
            if (buffer != nullptr) {
                TRYWINAPI(::UnmapViewOfFile(buffer));
            }

            if (temporalFileMapping != 0) {
                TRYWINAPI(::CloseHandle(temporalFileMapping));
                temporalFileMapping = 0;
            }

            PLARGE_INTEGER largeInteger = (PLARGE_INTEGER)&newBytesCapacity;

            TRYWINAPI(temporalFileMapping = ::CreateFileMapping(temporalFileHandle, nullptr, PAGE_READWRITE, largeInteger->HighPart, largeInteger->LowPart, nullptr));

            void* newBuffer;

            if (baseAddressChangeable || buffer == nullptr) {
                TRYWINAPI(newBuffer = ::MapViewOfFile(temporalFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, newBytesCapacity));
            }
            else {
                newBuffer = ::MapViewOfFileEx(temporalFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, newBytesCapacity, buffer);
                if (newBuffer == nullptr) {
                    TRYWINAPI(newBuffer = ::MapViewOfFileEx(temporalFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, bytesCapacity, buffer));
                    buffer = newBuffer;
                    return false;
                }
            }

            buffer = newBuffer;
            bytesCapacity = newBytesCapacity;
            return true;
        }

    public:
        FileBasedBuffer() {
        }

        String GetLastErrorMessage() {
            return ffs::GetLastErrorMessage(lastError);
        }

        ErrnoT GetLastError() {
            return lastError;
        }

        bool Initialize() {
            if (init) {
                return true;
            }

            TCHAR temporalFilePath[MAX_PATH + 1];

            TCHAR temporalFilePathName[MAX_PATH + 1];

            TRYWINAPI(::GetTempPath(sizeof temporalFilePath / sizeof(TCHAR), temporalFilePath));

            TRYWINAPI(::GetTempFileName(temporalFilePath, TEXT("FFS"), 0, temporalFilePathName));

            TRYWINAPI((temporalFileHandle = ::CreateFile(
                temporalFilePathName,
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY,
                nullptr)) != INVALID_HANDLE_VALUE);

            init = true;

            return true;
        }

        size_t GetAllocationGranularity() {
            Initialize();

            return allocationGranularity;
        }

        void* GetBuffer() {
            INIT_ELSE_NULL();

            return buffer;
        }

        bool SetSizeBytes(size_t bytesSize) {
            INIT();

            size_t newBytesCapacity = bytesSize / allocationGranularity * allocationGranularity;

            if (newBytesCapacity < bytesSize || newBytesCapacity == 0) {
                newBytesCapacity += allocationGranularity;
            }

            if (newBytesCapacity > bytesCapacity) {
                if (!SetCapacityBytes(newBytesCapacity)) {
                    return false;
                }
            }

            this->bytesSize = bytesSize;

            return true;
        }

        size_t GetSizeBytes() {
            Initialize();

            return bytesSize;
        }

        size_t GetCapacityBytes() {
            Initialize();

            return bytesCapacity;
        }

        bool IsBaseAddressChangeable() {
            Initialize();

            return baseAddressChangeable;
        }

        bool SetBaseAddressChangeable(bool baseAddressChangeable) {
            if (!Initialize()) {
                return false;
            }

            this->baseAddressChangeable = baseAddressChangeable;
            return true;
        }

        bool Trim() {
            INIT();

            /// 尝试获取调整文件大小的权限
            LUID luid;
            if (!::LookupPrivilegeValue(nullptr, SE_MANAGE_VOLUME_NAME, &luid)) {
                lastError = ::GetLastError();
                return false;
            }

            HANDLE token;
            if (!::OpenProcessToken(GetCurrentProcess(), GENERIC_ALL, &token)) {
                lastError = ::GetLastError();
                return false;
            }

            DWORD privilegesLength;
            ::GetTokenInformation(token, TokenPrivileges, nullptr, 0, &privilegesLength);

            PTOKEN_PRIVILEGES privileges = (PTOKEN_PRIVILEGES)LocalAlloc(LMEM_FIXED, privilegesLength);
            if (!::GetTokenInformation(token, TokenPrivileges, privileges, privilegesLength, &privilegesLength)) {
                lastError = ::GetLastError();
                return false;
            }

            bool privilegeEnabled = false;
            for (DWORD i = 0; i < privileges->PrivilegeCount; i++) {
                PLUID_AND_ATTRIBUTES luidAndAttributes = privileges->Privileges + i;
                PLUID currentLuid = &luidAndAttributes->Luid;
                DWORD attributes = luidAndAttributes->Attributes;
                if (luid.HighPart == currentLuid->HighPart && luid.LowPart == currentLuid->LowPart) {
                    if ((attributes & SE_PRIVILEGE_ENABLED) != 0) {
                        privilegeEnabled = true;
                    }
                }
            }
            ::LocalFree(privileges);

            if (!privilegeEnabled) {
                TOKEN_PRIVILEGES newPrivileges;
                newPrivileges.PrivilegeCount = 1;
                newPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                newPrivileges.Privileges[0].Luid = luid;
                if (!::AdjustTokenPrivileges(token, FALSE, &newPrivileges, sizeof newPrivileges, nullptr, nullptr)) {
                    lastError = ::GetLastError();
                    return false;
                }
            }

            size_t newBytesCapacity = bytesSize / allocationGranularity * allocationGranularity;
            if (newBytesCapacity == 0) {
                newBytesCapacity += allocationGranularity;
            }

            if (buffer != nullptr) {
                ::UnmapViewOfFile(buffer);
                buffer = nullptr;
            }

            if (temporalFileMapping != INVALID_HANDLE_VALUE) {
                ::CloseHandle(temporalFileMapping);
                temporalFileMapping = INVALID_HANDLE_VALUE;
            }

            if (::SetFileValidData(temporalFileHandle, newBytesCapacity)) {
                lastError = ::GetLastError();
                return false;
            }

            if (!::SetEndOfFile(temporalFileHandle)) {
                lastError = ::GetLastError();
                return false;
            }

            return SetCapacityBytes(newBytesCapacity);
        }

        ~FileBasedBuffer() {
            if (temporalFileMapping != INVALID_HANDLE_VALUE) {
                ::CloseHandle(temporalFileMapping);
                temporalFileMapping = INVALID_HANDLE_VALUE;
            }

            if (temporalFileHandle != INVALID_HANDLE_VALUE) {
                ::CloseHandle(temporalFileHandle);
                temporalFileHandle = INVALID_HANDLE_VALUE;
            }
        }
    };

    ErrnoT CreateBuffer(BufferType type, IBuffer** ppBuffer) {
        IBuffer* buffer;
        switch (type) {
        case FileBasedBuffer:
            buffer = new class FileBasedBuffer();
            break;
        default:
            return ERROR_UNSUPPORTED_TYPE;
            break;
        }

        bool success = buffer->Initialize();
        ErrnoT lastError = buffer->GetLastError();
        if (success) {
            *ppBuffer = buffer;
        }
        else {
            *ppBuffer = nullptr;
            delete buffer;
        }
        return lastError;
    }

} // namespace ffs