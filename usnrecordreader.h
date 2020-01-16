#pragma once

#include <Windows.h>
#include "types.h"

namespace ffs {

    /// ��ȡ���д���·��
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

    /// ���ݴ���·����ȡ�ļ�ϵͳ����
    String GetFileSystemName(String driveString) {
        TCHAR buffer[MAX_PATH + 1];
        ::GetVolumeInformation(driveString.c_str(), nullptr, 0, nullptr, nullptr, nullptr, buffer, sizeof buffer / sizeof(TCHAR));
        return String(buffer);
    }

    /// ���ݴ���·���������Ƿ�֧�� USN ��־
    bool IsDriveSupportUsnJournal(String driveString) {
        String fs = GetFileSystemName(driveString);
        return fs == TEXT("NTFS") || fs == TEXT("ReFS");
    }

    /// ���ݴ���·���򿪴��̷��ؾ��
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

    /// ���� GetLastError() ����ֵ��ȡ������Ϣ
    String GetErrorMessage(int lastError) {
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
        LocalFree(msg);
        return s;
    }

    /// USN ��־��ȡ��
    class UsnRecordReader {
    private:
        static const size_t BUFFER_SIZE = 65536;

    private:
        HANDLE handle;

        USN_JOURNAL_DATA journalData;

        DWORD lastError = ERROR_SUCCESS;

        READ_USN_JOURNAL_DATA readData = { 0, 0xFFFFFFFF, FALSE, 0, 0 };

        PUSN_RECORD_V2 record = nullptr;

        DWORD bytesLeft;

        CHAR buffer[BUFFER_SIZE];

        bool init = false;

    public:
        UsnRecordReader(HANDLE handle) {
            this->handle = handle;
        }
        
        /// ��ȡ��һ�β����Ĵ�����Ϣ
        String GetErrorMessage() {
            return ffs::GetErrorMessage(lastError);
        }

        /// ���г�ʼ������
        bool Initialize() {
            DWORD returnedBytes;
            BOOL result = DeviceIoControl(
                handle,
                FSCTL_QUERY_USN_JOURNAL,
                NULL,
                0,
                &journalData,
                sizeof journalData,
                &returnedBytes,
                NULL
            );
            if (result == TRUE) {
                init = true;
                readData.UsnJournalID = journalData.UsnJournalID;
                readData.MaxMajorVersion = 2;
                readData.MinMajorVersion = 2;
                return true;
            }
            else {
                lastError = GetLastError();
                return false;
            }
        }

        /// ��ȡ��һ�� USN ��¼����ȡʧ�ܷ��ؿ�ָ��
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
                    &buffer,
                    BUFFER_SIZE,
                    &bytesLeft,
                    NULL)) {
                    lastError = GetLastError();
                    return nullptr;
                }

                bytesLeft -= sizeof(USN);

                record = (PUSN_RECORD_V2)(((PUCHAR)buffer) + sizeof(USN));
                
                readData.StartUsn = *(USN*)buffer;

                return GetNext();
            }
        }

        ~UsnRecordReader() {
            CloseHandle(handle);
        }
    };
    
} // namespace ffs