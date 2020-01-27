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

    /// 根据 GetLastError() 返回值获取错误信息
    String GetLastErrorMessage(ErrnoT lastError);

    /// 获取所有磁盘路径
    Strings GetLogicalDrives();

    /// 根据磁盘路径获取文件系统名称
    String GetFileSystemName(String driveString);

    /// 打开驱动器
    HANDLE OpenDrive(String driveString);

    /// 根据磁盘路径检查磁盘文件系统是否支持 USN 日志
    bool IsDriveSupportUsnJournal(String driveString);

    /// 获取内存映射分配粒度
    DWORD GetAllocationGranularity();

    /// 创建缓冲区
    ErrnoT CreateBuffer(BufferType, IBuffer**);

    /// 创建 USN 日志数据库访问模块
    ErrnoT CreateUsnRecordDao(String connectionString, IUsnRecordDao**);

    /// 创建 USN 日志读取模块
    ErrnoT CreateUsnRecordReader(HANDLE, IUsnRecordReader**);

} // namespace ffs