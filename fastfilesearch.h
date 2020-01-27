#pragma once

#include <tchar.h>
#include <string>
#include <vector>
#include <Windows.h>
#include <sqltypes.h>

namespace ffs {

    using String = std::basic_string<TCHAR>;

    using Strings = std::vector<String>;

    using ErrnoT = DWORD;

    enum BufferType {
        FileBasedBuffer
    };

    class IFfsObject {
    public:
        virtual ~IFfsObject() {}

        virtual ErrnoT GetLastError() = 0;

        virtual String GetLastErrorMessage() = 0;

        virtual bool Initialize() = 0;
    };

    class IBuffer : public IFfsObject {
    public:
        virtual ~IBuffer() {}

        virtual size_t GetAllocationGranularity() = 0;

        virtual void* GetBuffer() = 0;

        virtual size_t GetCapacityBytes() = 0;

        virtual size_t GetSizeBytes() = 0;

        virtual bool IsBaseAddressChangeable() = 0;

        virtual bool SetBaseAddressChangeable(bool) = 0;

        virtual bool SetSizeBytes(size_t) = 0;

        virtual bool SetCapacityBytes(size_t) = 0;

        virtual bool Trim() = 0;
    };

    class IUsnRecordDao : public IFfsObject {
    public:
        virtual ~IUsnRecordDao() {}

        virtual bool Commit() = 0;

        virtual Strings GetOdbcErrorMessages() = 0;

        virtual std::vector<SQLINTEGER> GetOdbcNativeErrors() = 0;

        virtual Strings GetOdbcSqlStates() = 0;

        virtual bool IsAutoCommit() = 0;

        virtual bool Save(PUSN_RECORD_V2) = 0;

        virtual bool SetAutoCommit(bool) = 0;
    };

    class IUsnRecordReader : public IFfsObject {
    public:
        virtual ~IUsnRecordReader() {};

        virtual PUSN_RECORD_V2 GetNext() = 0;
    };

    /// ���� GetLastError() ����ֵ��ȡ������Ϣ
    String GetLastErrorMessage(ErrnoT lastError);

    /// ��ȡ���д���·��
    Strings GetLogicalDrives();

    /// ���ݴ���·����ȡ�ļ�ϵͳ����
    String GetFileSystemName(String driveString);

    /// ��������
    HANDLE OpenDrive(String driveString);

    /// ���ݴ���·���������ļ�ϵͳ�Ƿ�֧�� USN ��־
    bool IsDriveSupportUsnJournal(String driveString);

    /// ��ȡ�ڴ�ӳ���������
    DWORD GetAllocationGranularity();

    /// ����������
    ErrnoT CreateBuffer(BufferType, IBuffer**);

    /// ���� USN ��־���ݿ����ģ��
    ErrnoT CreateUsnRecordDao(String connectionString, IUsnRecordDao**);

    /// ���� USN ��־��ȡģ��
    ErrnoT CreateUsnRecordReader(HANDLE, IUsnRecordReader**);

} // namespace ffs