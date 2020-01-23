#pragma once

#include <Windows.h>
#include <sql.h>
#include <sqlext.h>
#include "utils.h"
#include "filebasedbuffer.h"

#define TRYODBC(_handle, _handleType, _statement) { \
            int _line = __LINE__; \
            SQLRETURN _ret = _statement; \
            if (_ret != SQL_SUCCESS) { \
                HandleDiagnosticRecord(TEXT(__FILE__), _line, _handle, _handleType, _ret); \
            } \
            if (!SQL_SUCCEEDED(_ret) && _ret != SQL_NEED_DATA) { \
                return false; \
            } \
        }

namespace ffs {

    struct FileNameRecord {

        SQLLEN fileNameLength;

        WCHAR fileName[1];
    };

    struct UsnRecord {

        SQLUBIGINT fileReferenceNumber;

        SQLUBIGINT parentFileReferenceNumber;

        SQLBIGINT usn;

        SQLUBIGINT timestamp;

        SQLINTEGER reason;

        SQLINTEGER sourceInfo;

        SQLINTEGER securityId;

        SQLINTEGER fileAttributes;

        SQLLEN fileNameLengthOrIndicator;

        SQLLEN bytesWritten;

        size_t fileNameRecordOffset;
    };

    /// USN 日志 DAO 类
    class UsnRecordDao {
    private:
        bool init = false;

        bool autoCommit = true;

        size_t uncommits;

        SQLHENV environment;

        SQLHDBC connection;

        SQLHSTMT statement;

        SQLHSTMT bulkStatement;

        Strings errorMessages;

        Strings sqlStates;

        std::vector<SQLINTEGER> nativeErrors;

        String connectionString;

        SQLTCHAR* sqlInsertUsnRecord = (SQLTCHAR*)TEXT("insert into usn_record values(?, ?, ?, ?, ?, ?, ?, ?, ?)");

        SQLTCHAR* sqlQueryUsnRecord = (SQLTCHAR*)TEXT("select top 0 * from usn_record");

        FileBasedBuffer fileNameRecordBuffer, usnRecordBuffer;

        FileNameRecord* lastFileNameRecord;

        UsnRecord* lastUsnRecord;

        size_t commitLoop, commitNumber;

        // 获取 ODBC 错误、警告或信息
        void HandleDiagnosticRecord(const SQLTCHAR* filename, int line, SQLHANDLE handle, SQLSMALLINT type, SQLRETURN ret) {
            errorMessages.clear();
            sqlStates.clear();
            nativeErrors.clear();
            SQLSMALLINT currentRecord = 0;
            for (;;) {
                SQLRETURN _ret;
                SQLTCHAR* message;
                SQLSMALLINT messageLength;
                SQLTCHAR sqlState[6];
                SQLINTEGER nativeError;

                currentRecord++;

                if (!SQL_SUCCEEDED(::SQLGetDiagField(type, handle, currentRecord, SQL_DIAG_MESSAGE_TEXT, nullptr, 0, &messageLength))) {
                    break;
                }
                message = new SQLTCHAR[messageLength / sizeof(SQLTCHAR) + 3];
                _ret = ::SQLGetDiagField(type, handle, currentRecord, SQL_DIAG_MESSAGE_TEXT, message, messageLength, &messageLength);
                ::SQLGetDiagField(type, handle, currentRecord, SQL_DIAG_SQLSTATE, sqlState, sizeof sqlState, nullptr);
                ::SQLGetDiagField(type, handle, currentRecord, SQL_DIAG_NATIVE, &nativeError, sizeof nativeError, nullptr);
                if (_ret != SQL_NO_DATA) {
                    sqlStates.emplace_back(sqlState);
                    nativeErrors.emplace_back(nativeError);
                    messageLength /= sizeof(SQLTCHAR);
                    errorMessages.emplace_back(message);
                    SQLLEN len = lstrlen(message);
                    message[messageLength - 1] = TEXT('\r');
                    message[messageLength] = TEXT('\n');
                    message[messageLength + 1] = 0;
                    TCHAR buffer[20];
                    if (filename != nullptr) {
                        OutputDebugString(filename);
                        _stprintf_s(buffer, TEXT("(%d,%d): "), line, 1);
                        OutputDebugString(buffer);
                    }
                    _stprintf_s(buffer, TEXT("Loop %zu, commit %zu: "), commitLoop, commitNumber - uncommits);
                    OutputDebugString(buffer);
                    OutputDebugString(sqlState);
                    OutputDebugString(message);
                    delete[] message;
                }
                else {
                    delete[] message;
                    break;
                }
            }
        }

        /// 将 USN 记录直接写入数据库
        bool SaveDirect(PUSN_RECORD_V2 record) {

            SQLLEN bytesWritten[10] = { 0 };

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLFreeStmt(statement, SQL_RESET_PARAMS));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLBindParameter(statement, 1, SQL_PARAM_INPUT, SQL_C_UBIGINT, SQL_BIGINT, 0, 0, &record->FileReferenceNumber, 0, bytesWritten + 1));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLBindParameter(statement, 2, SQL_PARAM_INPUT, SQL_C_UBIGINT, SQL_BIGINT, 0, 0, &record->ParentFileReferenceNumber, 0, bytesWritten + 2));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLBindParameter(statement, 3, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0, 0, &record->Usn, 0, bytesWritten + 3));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLBindParameter(statement, 4, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &record->Reason, 0, bytesWritten + 4));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLBindParameter(statement, 5, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &record->SourceInfo, 0, bytesWritten + 5));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLBindParameter(statement, 6, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &record->SecurityId, 0, bytesWritten + 6));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLBindParameter(statement, 7, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &record->FileAttributes, 0, bytesWritten + 7));

            SQLLEN len = record->FileNameLength / sizeof(SQLTCHAR);

            if (len == 0) {
                bytesWritten[8] = SQL_NULL_DATA;
                len = 1;
            }
            else {
                bytesWritten[8] = record->FileNameLength;
            }

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLBindParameter(statement, 8, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WCHAR, len, 0, record->FileName, record->FileNameLength, bytesWritten + 8));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLBindParameter(statement, 9, SQL_PARAM_INPUT, SQL_C_UBIGINT, SQL_BIGINT, 0, 0, &record->TimeStamp.QuadPart, 0, bytesWritten + 9));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLExecute(statement));

            return true;
        }

        /// 将 USN 记录提交任务附加到提交队列
        bool SaveBulk(PUSN_RECORD_V2 record) {

            size_t oldFileNameRecordBufferSize = fileNameRecordBuffer.GetBufferSize();
            size_t oldUsnRecordBufferSize = usnRecordBuffer.GetBufferSize();

            size_t appendFileNameRecordBufferSize = sizeof(SQLLEN) + record->FileNameLength + sizeof(WCHAR);
            size_t appendUsnRecordBufferSize = sizeof(UsnRecord);

            size_t newFileNameRecordBufferSize = oldFileNameRecordBufferSize + appendFileNameRecordBufferSize;
            size_t newUsnRecordBufferSize = oldUsnRecordBufferSize + appendUsnRecordBufferSize;

            if (!fileNameRecordBuffer.SetBufferSize(newFileNameRecordBufferSize) || !usnRecordBuffer.SetBufferSize(newUsnRecordBufferSize)) {
                if (!Commit()) {
                    return false;
                }
                fileNameRecordBuffer.SetBufferSize(appendFileNameRecordBufferSize);
                usnRecordBuffer.SetBufferSize(appendUsnRecordBufferSize);
                oldFileNameRecordBufferSize = 0;
                oldUsnRecordBufferSize = 0;
            }

            lastFileNameRecord = (FileNameRecord*)((PBYTE)fileNameRecordBuffer.GetBuffer() + oldFileNameRecordBufferSize);
            SQLLEN fileNameLength = record->FileNameLength;
            lastFileNameRecord->fileNameLength = record->FileNameLength;
            ::CopyMemory(lastFileNameRecord->fileName, record->FileName, record->FileNameLength);
            lastFileNameRecord->fileName[lastFileNameRecord->fileNameLength / sizeof(WCHAR)] = 0;
            //::OutputDebugString(lastFileNameRecord->fileName);
            //::OutputDebugString(TEXT("\r\n"));

            lastUsnRecord = (UsnRecord*)((PBYTE)usnRecordBuffer.GetBuffer() + oldUsnRecordBufferSize);
            lastUsnRecord->fileNameLengthOrIndicator = SQL_LEN_DATA_AT_EXEC(record->FileNameLength);
            lastUsnRecord->fileNameRecordOffset = oldFileNameRecordBufferSize;
            lastUsnRecord->bytesWritten = 0;
            ::CopyMemory(lastUsnRecord, &record->FileReferenceNumber, ((size_t) & (((UsnRecord*)0)->fileAttributes) + sizeof(UsnRecord::fileAttributes)));

            uncommits++;
            return true;
        }

    public:

        UsnRecordDao(String connectionString) {
            this->connectionString = connectionString;
        }

        bool Initialize() {
            if (init) {
                return true;
            }

            fileNameRecordBuffer.SetBaseAddressChangeable(true);

            usnRecordBuffer.SetBaseAddressChangeable(true);

            SQLRETURN ret;
            ret = ::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HENV, &environment);
            if (ret == SQL_ERROR) {
                errorMessages.emplace_back(TEXT("Unable to allocate an environment handle."));
                return false;
            }

            TRYODBC(environment,
                SQL_HANDLE_ENV,
                ::SQLSetEnvAttr(environment,
                    SQL_ATTR_ODBC_VERSION,
                    (SQLPOINTER)SQL_OV_ODBC3,
                    0));

            TRYODBC(environment,
                SQL_HANDLE_ENV,
                ::SQLAllocHandle(SQL_HANDLE_DBC, environment, &connection));

            TRYODBC(connection,
                SQL_HANDLE_DBC,
                ::SQLConnect(connection,
                (SQLTCHAR*)connectionString.c_str(),
                    SQL_NTS,
                    nullptr,
                    SQL_NULL_DATA,
                    nullptr,
                    SQL_NULL_DATA));

            TRYODBC(connection,
                SQL_HANDLE_DBC,
                ::SQLAllocHandle(SQL_HANDLE_STMT, connection, &statement));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLPrepare(statement, sqlInsertUsnRecord, SQL_NTS));

            TRYODBC(connection,
                SQL_HANDLE_DBC,
                ::SQLAllocHandle(SQL_HANDLE_STMT, connection, &bulkStatement));

            TRYODBC(bulkStatement,
                SQL_HANDLE_STMT,
                ::SQLSetStmtAttr(bulkStatement, SQL_ATTR_ROW_BIND_TYPE, (SQLPOINTER)sizeof(UsnRecord), 0));

            TRYODBC(bulkStatement,
                SQL_HANDLE_STMT,
                ::SQLSetStmtAttr(bulkStatement, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_KEYSET_DRIVEN, 0));

            TRYODBC(bulkStatement,
                SQL_HANDLE_STMT,
                ::SQLSetStmtAttr(bulkStatement, SQL_ATTR_CONCURRENCY, (SQLPOINTER)SQL_CONCUR_ROWVER, 0));

            TRYODBC(bulkStatement,
                SQL_HANDLE_STMT,
                ::SQLExecDirect(bulkStatement, sqlQueryUsnRecord, SQL_NTS));

            init = true;

            return true;
        }

        bool Save(PUSN_RECORD_V2 record) {
            if (!init) {
                if (!Initialize()) {
                    return false;
                }
            }

            if (autoCommit) {
                return SaveDirect(record);
            }
            else {
                return SaveBulk(record);
            }
        }

        /// 提交缓存的更改
        bool Commit() {
            if (!init) {
                if (!Initialize()) {
                    return false;
                }
            }

            if (uncommits != 0) {

                commitLoop++;
                commitNumber = uncommits;

                UsnRecord* usnRecords = (UsnRecord*)usnRecordBuffer.GetBuffer();

                TRYODBC(connection,
                    SQL_HANDLE_DBC,
                    ::SQLSetConnectAttr(connection, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0));

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ::SQLSetStmtAttr(bulkStatement, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)(usnRecordBuffer.GetBufferSize() / sizeof(UsnRecord)), 0));

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ::SQLBindCol(bulkStatement, 1, SQL_C_UBIGINT, &usnRecords[0].fileReferenceNumber, 0, &usnRecords[0].bytesWritten));

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ::SQLBindCol(bulkStatement, 2, SQL_C_UBIGINT, &usnRecords[0].parentFileReferenceNumber, 0, &usnRecords[0].bytesWritten));

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ::SQLBindCol(bulkStatement, 3, SQL_C_SBIGINT, &usnRecords[0].usn, 0, &usnRecords[0].bytesWritten));

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ::SQLBindCol(bulkStatement, 4, SQL_C_SLONG, &usnRecords[0].reason, 0, &usnRecords[0].bytesWritten));

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ::SQLBindCol(bulkStatement, 5, SQL_C_SLONG, &usnRecords[0].sourceInfo, 0, &usnRecords[0].bytesWritten));

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ::SQLBindCol(bulkStatement, 6, SQL_C_SLONG, &usnRecords[0].securityId, 0, &usnRecords[0].bytesWritten));

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ::SQLBindCol(bulkStatement, 7, SQL_C_SLONG, &usnRecords[0].fileAttributes, 0, &usnRecords[0].bytesWritten));

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ::SQLBindCol(bulkStatement, 8, SQL_C_WCHAR, &usnRecords[0].fileNameRecordOffset, fileNameRecordBuffer.GetBufferCapacity(), &usnRecords[0].fileNameLengthOrIndicator));

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ::SQLBindCol(bulkStatement, 9, SQL_C_UBIGINT, &usnRecords[0].timestamp, 0, &usnRecords[0].bytesWritten));

                SQLRETURN ret;

                TRYODBC(bulkStatement,
                    SQL_HANDLE_STMT,
                    ret = ::SQLBulkOperations(bulkStatement, SQL_ADD));

                while (true) {
                    uncommits--;

                    size_t* fileNameRecordOffset;

                    TRYODBC(bulkStatement,
                        SQL_HANDLE_STMT,
                        ret = ::SQLParamData(bulkStatement, (SQLPOINTER*)&fileNameRecordOffset));
                    if (ret != SQL_NEED_DATA) {
                        break;
                    }

                    FileNameRecord* fileNameRecord = (FileNameRecord*)((PBYTE)fileNameRecordBuffer.GetBuffer() + *fileNameRecordOffset);

                    TRYODBC(bulkStatement,
                        SQL_HANDLE_STMT,
                        ::SQLPutData(bulkStatement, fileNameRecord->fileName, fileNameRecord->fileNameLength));
                }

                TRYODBC(connection,
                    SQL_HANDLE_DBC,
                    ::SQLEndTran(SQL_HANDLE_DBC, connection, SQL_COMMIT));

                TRYODBC(connection,
                    SQL_HANDLE_DBC,
                    ::SQLSetConnectAttr(connection, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0));
            }

            return true;
        }

        bool IsAutoCommit() {
            if (!init) {
                Initialize();
            }

            return autoCommit;
        }

        bool SetAutoCommit(bool autoCommit) {
            this->autoCommit = autoCommit;
            if (autoCommit) {
                if (!Commit()) {
                    return false;
                }
            }
            return true;
        }

        Strings GetErrorMessages() {
            return errorMessages;
        }

        Strings GetSqlStates() {
            return sqlStates;
        }

        std::vector<SQLINTEGER> GetNativeErrors() {
            return nativeErrors;
        }

        ~UsnRecordDao() {
            ::SQLFreeHandle(SQL_HANDLE_STMT, statement);
            ::SQLFreeHandle(SQL_HANDLE_STMT, bulkStatement);
            ::SQLDisconnect(connection);
            ::SQLFreeHandle(SQL_HANDLE_DBC, connection);
            ::SQLFreeHandle(SQL_HANDLE_ENV, environment);
        }
    };

} // namespace ffs

#undef TRYODBC