#pragma once

#include <tchar.h>
#include <string>
#include <vector>

namespace ffs {

    using String = std::basic_string<TCHAR>;

    using Strings = std::vector<String>;

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

    /// ���ݴ���·���������ļ�ϵͳ�Ƿ�֧�� USN ��־
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
        ::LocalFree(msg);
        return s;
    }

    /// ��ȡ�ڴ�ӳ���������
    DWORD GetAllocationGranularity() {
        SYSTEM_INFO info;
        ::GetSystemInfo(&info);
        return info.dwAllocationGranularity;
    }

} // namespace ffs