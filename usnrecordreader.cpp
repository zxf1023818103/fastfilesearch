#include <Windows.h>
#include "fastfilesearch.h"

namespace ffs {

    Strings GetLogicalDrives() {
        DWORD size = ::GetLogicalDriveStringsW(0, nullptr);
        TCHAR* buffer = new TCHAR[size];
        ::GetLogicalDriveStrings(size, buffer);
        Strings result;
        String string;
        for (DWORD i = 0; i < size; i++) {
            if (buffer[i] != 0) {
                string += buffer[i];
            }
            else if (!string.empty()) {
                result.emplace_back(string);
                string.clear();
            }
        }

        delete[] buffer;
        return result;
    }

    String GetFileSystemName(String driveString) {
        TCHAR buffer[MAX_PATH + 1];
        ::GetVolumeInformation(driveString.c_str(), nullptr, 0, nullptr, nullptr, nullptr, buffer, sizeof buffer / sizeof(TCHAR));
        return String(buffer);
    }

    bool IsDriveSupportUsnJournal(String driveString) {
        String fs = GetFileSystemName(driveString);
        return fs == TEXT("NTFS") || fs == TEXT("ReFS");
    }

    /// 根据磁盘路径打开磁盘返回句柄
    HANDLE OpenDrive(String driveString) {
        TCHAR path[] = TEXT("\\\\.\\X:");
        path[4] = driveString[0];
        return CreateFile(
            path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
    }

    /// USN 日志读取类
    class UsnRecordReader : public IUsnRecordReader {
    private:
        static const size_t BUFFER_SIZE = 65536;

    private:
        HANDLE handle;

        USN_JOURNAL_DATA journalData;

        DWORD lastError = ERROR_SUCCESS;

        READ_USN_JOURNAL_DATA readData = { 0, 0xFFFFFFFF, FALSE, 0, 0 };

        PUSN_RECORD_V2 record = nullptr;

        DWORD bytesLeft;

        CHAR* buffer;

        IBuffer* fileBasedBuffer;

        bool init = false;

    public:
        UsnRecordReader(HANDLE handle) {
            this->handle = handle;
        }

        /// 获取上一次操作的错误信息
        String GetLastErrorMessage() {
            Initialize();
            return ffs::GetLastErrorMessage(lastError);
        }

        /// 获取上一次操作的错误代码
        ErrnoT GetLastError() {
            Initialize();
            return lastError;
        }

        /// 进行初始化操作
        bool Initialize() {
            if (init) {
                return true;
            }

            lastError = CreateBuffer(FileBasedBuffer, &fileBasedBuffer);
            if (lastError != ERROR_SUCCESS) {
                return false;
            }

            DWORD returnedBytes;

            BOOL result = DeviceIoControl(
                handle,
                FSCTL_QUERY_USN_JOURNAL,
                NULL,
                0,
                &journalData,
                sizeof journalData,
                &returnedBytes,
                NULL);

            fileBasedBuffer->SetSizeBytes(BUFFER_SIZE);
            buffer = (PCHAR)fileBasedBuffer->GetBuffer();

            if (result == TRUE) {
                init = true;
                readData.UsnJournalID = journalData.UsnJournalID;
                readData.MaxMajorVersion = 2;
                readData.MinMajorVersion = 2;
                return true;
            }
            else {
                lastError = ::GetLastError();
                return false;
            }
        }

        /// 获取下一条 USN 记录，读取失败返回空指针
        PUSN_RECORD_V2 GetNext() {

            if (init == false) {
                if (!Initialize()) {
                    return nullptr;
                }
            }

            if (bytesLeft > 0) {
                PUSN_RECORD_V2 currentRecord = record;
                bytesLeft -= record->RecordLength;
                record = (PUSN_RECORD_V2)(((PCHAR)record) + record->RecordLength);
                return currentRecord;
            }
            else {
                memset(buffer, 0, BUFFER_SIZE);

                if (!DeviceIoControl(
                    handle,
                    FSCTL_READ_USN_JOURNAL,
                    &readData,
                    sizeof(readData),
                    buffer,
                    BUFFER_SIZE,
                    &bytesLeft,
                    NULL)) {
                    lastError = ::GetLastError();
                    return nullptr;
                }

                bytesLeft -= sizeof(USN);

                record = (PUSN_RECORD_V2)(((PUCHAR)buffer) + sizeof(USN));

                readData.StartUsn = *(USN*)buffer;

                if (bytesLeft != 0) {
                    return GetNext();
                }
                else {
                    return nullptr;
                }
            }
        }

        ~UsnRecordReader() {
            ::CloseHandle(handle);
        }
    };

    ErrnoT CreateUsnRecordReader(HANDLE handle, IUsnRecordReader** ppReader) {
        UsnRecordReader* reader = new UsnRecordReader(handle);
        bool success = reader->Initialize();
        ErrnoT lastError = reader->GetLastError();
        if (success) {
            *ppReader = reader;
        }
        else {
            *ppReader = nullptr;
            delete reader;
        }
        return lastError;
    }

} // namespace ffs