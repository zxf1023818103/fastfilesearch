#include "usnrecordreader.h"
#include "usnrecorddao.h"

int main() {
    ffs::UsnRecordDao* dao = new ffs::UsnRecordDao(TEXT("dsn=fastfilesearch"));
    ffs::Strings logicalDriveStrings = ffs::GetLogicalDrives();
    for (ffs::String drive : logicalDriveStrings) {
        if (ffs::IsDriveSupportUsnJournal(drive)) {
            HANDLE handle = ffs::OpenDrive(drive);
            ffs::UsnRecordReader* reader = new ffs::UsnRecordReader(handle);
            PUSN_RECORD_V2 record;
            reader->Initialize();
            reader->GetNext();
            int i;
            for (i = 0; ; i++) {
                record = reader->GetNext();
                //printf("%.*S\r\n", record->FileNameLength / 2, record->FileName);
                dao->Save(record);
            }
            _tprintf(TEXT("%s: %d record(s) written to database.\n"), drive.c_str(), i);
        }
    }
    return 0;
}