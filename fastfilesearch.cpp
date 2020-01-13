#include "usnrecordreader.h"

int main() {
    ffs::Strings logicalDriveStrings = ffs::GetLogicalDrives();
    for (ffs::String drive : logicalDriveStrings) {
        if (ffs::IsDriveSupportUsnJournal(drive)) {
            HANDLE handle = ffs::OpenDrive(drive);
            ffs::UsnRecordReader* reader = new ffs::UsnRecordReader(handle);
            PUSN_RECORD_V2 record;
            reader->Initialize();
            while ((record = reader->GetNext())) {
                printf("%.*S\r\n", record->FileNameLength / 2, record->FileName);
            }
        }
    }
    return 0;
}