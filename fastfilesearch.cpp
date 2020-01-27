#include "fastfilesearch.h"

int main() {
    ffs::IUsnRecordDao* dao;
    DWORD lastError = ffs::CreateUsnRecordDao(TEXT("fastfilesearch"), &dao);
    if (lastError != ERROR_SUCCESS) {
        OutputDebugString(ffs::GetLastErrorMessage(lastError).c_str());
        return -1;
    }
    ffs::Strings logicalDriveStrings = ffs::GetLogicalDrives();
    for (ffs::String drive : logicalDriveStrings) {
        if (ffs::IsDriveSupportUsnJournal(drive)) {
            HANDLE handle = ffs::OpenDrive(drive);
            ffs::IUsnRecordReader* reader;
            lastError = ffs::CreateUsnRecordReader(handle, &reader);
            if (lastError != ERROR_SUCCESS) {
                OutputDebugString(ffs::GetLastErrorMessage(lastError).c_str());
                return -1;
            }
            PUSN_RECORD_V2 record;
            reader->Initialize();
            reader->GetNext();
            dao->SetAutoCommit(false);
            for (;;) {
                record = reader->GetNext();
                if (record == nullptr) {
                    OutputDebugString(reader->GetLastErrorMessage().c_str());
                    break;
                }
                else {
                    dao->Save(record);
                }
            }
            dao->Commit();
        }
    }
    return 0;
}