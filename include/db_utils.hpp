// -----------------------------------------------------------------
// Dbfs - Cache in RAM the content of DB tables and mount the cache like a file system.
// Copyright (C) 2017  Gabriele Bonacini
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------

#ifndef  PQ_DB__FS
#define  PQ_DB__FS

#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <exception> 
#include <iostream> 
#include <fstream> 
#include <cstring>
#include <functional>
#include <memory>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

extern "C"{
#include <postgresql/libpq-fe.h>
}

#include <syslog.hpp>
#include <Types.hpp>

namespace dbfsutils{

typedef  struct stat                               Stat;
typedef  std::string                               TableName;
typedef  size_t                                    RowNum;
typedef  std::vector<char>                         TableData;
typedef  std::tuple<RowNum, TableData, Stat>       TableAttr;
typedef  std::map<TableName, TableAttr>            TableList;

enum ATTRIB { RNUM, DATA, SSTAT };

class DbConnExc final {
      public:
         explicit    DbConnExc(int errNum);
         explicit    DbConnExc(std::string&  errString);
         explicit    DbConnExc(std::string&& errString);
                     DbConnExc(int errNum, std::string errString);
         std::string what(void)                                                           const noexcept(true);
      private:
         int         errorCode;
         std::string errorMessage;
   };

class DbConnection{
        public:
                explicit         DbConnection(syslogwrp::Syslog *slog);
                virtual          ~DbConnection();
                virtual void     connect(std::string dbname, std::string user, 
                                 std::string hostAddr, std::string port, 
                                 std::string pwd)                                        = 0;
                virtual void     connect(std::string dbname, std::string user, 
                                 std::string hostAddr)                                   = 0; 
                virtual void     loadDbByOwner(TableList& table, const std::string owner)
                                                                                         = 0;
                virtual void     loadDbByList(TableList& table, const std::string owner)
                                                                                         = 0;
                virtual void     printDebug(const TableList& db)                         = 0;
                virtual void     reset(void)                                             = 0;
    
        protected:
                syslogwrp::Syslog            *syslog;

                virtual void     loadTable(TableName tableName, TableAttr& tableAttr)    = 0;
};

class PsqlConnection : public DbConnection {
        public:
                explicit PsqlConnection(syslogwrp::Syslog *slog);
                         ~PsqlConnection()                                                           override;
                void     connect(std::string dbname, std::string user, 
                                 std::string hostAddr, std::string port, 
                                 std::string pwd)                                   noexcept(false)  override;
                void     connect(std::string dbname, std::string user, 
                                 std::string hostAddr)                              noexcept(false)  override; 
                void     loadDbByOwner(TableList& table, const std::string owner)
                                                                                    noexcept(false)  override;
                void     loadDbByList(TableList& table, const std::string owner)
                                                                                    noexcept(false)  override;
                void     printDebug(const TableList& db)                            noexcept(true)   override;
                void     reset(void);
    
        protected:
                Stat                         statTempl;
                std::string                  connectionString;
                PGconn                       *conn;

                void     loadTable(TableName tableName, TableAttr& tableAttr)       noexcept(false)  override;
}; 

class DBIface{
       public:
           static DBIface&  getInstance(void)                                       noexcept(false); 
           std::unique_ptr<DbConnection>        getDbConn(std::string dbType, 
                                                       syslogwrp::Syslog *slog)     noexcept(false);

       private:
           DBIface(void);
           std::map<std::string, std::function<std::unique_ptr<DbConnection>(syslogwrp::Syslog *slog)>>  dbTypes;           

};

} // end namespace dbfsutils

#endif
