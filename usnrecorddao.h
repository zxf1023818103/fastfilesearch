#pragma once

#include <Windows.h>
#include <sql.h>
#include <sqlext.h>
#include "types.h"

#define TRYODBC(handle, handleType, statement) { \
            SQLRETURN ret = statement; \
            if (ret != SQL_SUCCESS) { \
                HandleDiagnosticRecord(handle, handleType, ret); \
            } \
            if (!SQL_SUCCEEDED(ret)) { \
                return false; \
            } \
        }

namespace ffs {

    class UsnRecordDao {
    private:
        bool init = false;

        SQLHENV environment;

        SQLHDBC connection;

        SQLHSTMT statement;

        Strings errorMessages;

        String connectionString;

        SQLTCHAR* sqlInsertUsnRecord = (SQLTCHAR*)TEXT("insert into usn_record values(?, ?, ?, ?, ?, ?, ?, ?, ?)");

        SQLTCHAR* sqlCreateTestTable = (SQLTCHAR*)TEXT("create table test (id int primary key not null, name text)");

        SQLTCHAR* sqlInsertIdIntoTest = (SQLTCHAR*)TEXT("insert into test (id) values (?)");

        void HandleDiagnosticRecord(SQLHANDLE handle, SQLSMALLINT type, SQLRETURN ret) {
            errorMessages.clear();
            SQLSMALLINT currentRecord = 0;
            for (;;) {
                SQLRETURN _ret;
                SQLTCHAR* message;
                SQLSMALLINT messageLength;

                currentRecord++;

                if (!SQL_SUCCEEDED(::SQLGetDiagField(type, handle, currentRecord, SQL_DIAG_MESSAGE_TEXT, nullptr, 0, &messageLength))) {
                    break;
                }
                message = new SQLTCHAR[messageLength / sizeof(SQLTCHAR) + 3];
                _ret = ::SQLGetDiagField(type, handle, currentRecord, SQL_DIAG_MESSAGE_TEXT, message, messageLength, &messageLength);
                if (_ret != SQL_NO_DATA) {
                    messageLength /= sizeof(SQLTCHAR);
                    errorMessages.emplace_back(message);
                    SQLLEN len = lstrlen(message);
                    message[messageLength - 1] = TEXT('\r');
                    message[messageLength] = TEXT('\n');
                    message[messageLength + 1] = 0;
                    OutputDebugString(message);
                    delete[] message;
                }
                else {
                    delete[] message;
                    break;
                }
            }
        }

    public:

        UsnRecordDao(String connectionString) {
            this->connectionString = connectionString;
        }

        bool Initialize() {
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
                ::SQLDriverConnect(connection,
                    ::GetDesktopWindow(),
                    (SQLTCHAR*)connectionString.c_str(),
                    SQL_NTS,
                    nullptr,
                    0,
                    nullptr,
                    SQL_DRIVER_COMPLETE));

            TRYODBC(connection,
                SQL_HANDLE_DBC,
                ::SQLAllocHandle(SQL_HANDLE_STMT, connection, &statement));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLPrepare(statement, sqlInsertUsnRecord, SQL_NTS));

            init = true;
            return true;
        }

        bool Save(PUSN_RECORD_V2 record) {
            if (!init) {
                if (!Initialize()) {
                    return false;
                }
            }

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
                ::SQLBindParameter(statement, 8, SQL_PARAM_INPUT, SQL_C_TCHAR, SQL_WCHAR, len, 0, record->FileName, record->FileNameLength, bytesWritten + 8));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLBindParameter(statement, 9, SQL_PARAM_INPUT, SQL_C_UBIGINT, SQL_BIGINT, 0, 0, &record->TimeStamp.QuadPart, 0, bytesWritten + 9));

            TRYODBC(statement,
                SQL_HANDLE_STMT,
                ::SQLExecute(statement));

            return true;
        }

        Strings GetErrorMessages() {
            return errorMessages;
        }

        ~UsnRecordDao() {
            ::SQLFreeHandle(SQL_HANDLE_STMT, statement);
            ::SQLFreeHandle(SQL_HANDLE_DBC, connection);
            ::SQLFreeHandle(SQL_HANDLE_ENV, environment);
        }
    };
} // namespace ffs

#undef TRYODBC