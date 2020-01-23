#pragma once

#include <Windows.h>
#include "utils.h"
#include "filebasedbuffer.h"

namespace ffs {

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

        CHAR *buffer;

        FileBasedBuffer fileBasedBuffer;

        bool init = false;

    public:
        UsnRecordReader(HANDLE handle) {
            this->handle = handle;
        }
        
        /// ��ȡ��һ�β����Ĵ�����Ϣ
        String GetErrorMessage() {
            return ffs::GetErrorMessage(lastError);
        }

        /// ��ȡ��һ�β����Ĵ������
        DWORD GetLastError() {
            return lastError;
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
                NULL);

            fileBasedBuffer.SetBufferSize(BUFFER_SIZE);
            buffer = (PCHAR)fileBasedBuffer.GetBuffer();

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
    
} // namespace ffs