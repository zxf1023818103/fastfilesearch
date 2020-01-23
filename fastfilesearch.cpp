#include "usnrecordreader.h"
#include "usnrecorddao.h"

int main() {
    ffs::UsnRecordDao* dao = new ffs::UsnRecordDao(TEXT("fastfilesearch"));
    ffs::Strings logicalDriveStrings = ffs::GetLogicalDrives();
    for (ffs::String drive : logicalDriveStrings) {
        if (ffs::IsDriveSupportUsnJournal(drive)) {
            HANDLE handle = ffs::OpenDrive(drive);
            ffs::UsnRecordReader* reader = new ffs::UsnRecordReader(handle);
            PUSN_RECORD_V2 record;
            reader->Initialize();
            reader->GetNext();
            dao->SetAutoCommit(false);
            for (;;) {
                record = reader->GetNext();
                if (record == nullptr) {
                    OutputDebugString(reader->GetErrorMessage().c_str());
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