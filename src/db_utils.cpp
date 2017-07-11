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

#include <db_utils.hpp>

using std::string;
using std::vector;
using std::make_tuple;
using std::endl;
using std::get;
using std::ifstream;
using std::getline;
using std::to_string;
using std::unique_ptr;

using syslogwrp::Syslog; 

using typeutils::TypesUtilsException; 
using typeutils::safeSizeT; 

namespace dbfsutils{

DbConnExc::DbConnExc(int errNum) 
                         : errorCode{errNum}, errorMessage{"None"}{}  
   
DbConnExc::DbConnExc(string& errString)
                         : errorCode{0}, errorMessage{errString}{}
   
DbConnExc::DbConnExc(string&& errString)
                         : errorCode{0}, errorMessage{move(errString)}{}     
                             
DbConnExc::DbConnExc(int errNum, string errString)
                         : errorCode{errNum}, errorMessage{errString}{}                       
         
string DbConnExc::what() const noexcept(true){
      return errorMessage;
}

PsqlConnection::PsqlConnection(Syslog *slog)
                     : DbConnection{slog}, conn{nullptr}{
      #ifdef __GNUC__
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
      #endif

      statTempl          = {};
      statTempl.st_mode  = S_IFREG | 0444;
      statTempl.st_nlink = 2;
      statTempl.st_uid   = getuid();
      statTempl.st_gid   = getgid();

      #ifdef __GNUC__
      #pragma GCC diagnostic pop
      #endif
}

PsqlConnection::~PsqlConnection(){
        PQfinish(conn);
}

void PsqlConnection::reset(void){
        if(conn != nullptr) PQreset(conn);
}

void PsqlConnection::connect(string dbname, string user, string hostAddr, string port, string pwd) noexcept(false){
        connectionString  = "dbname=" + dbname +  " user=" + user + " password=" + pwd + \
                          " hostaddr=" + hostAddr + " port=" + port;
        conn              = PQconnectdb(connectionString.c_str());
        if(PQstatus(conn) == CONNECTION_BAD)
                throw DbConnExc("Connection Error");
}

void PsqlConnection::connect(string dbname, string user, string hostAddr) noexcept(false){

        connectionString  = "dbname=" + dbname +  " user=" + user + " hostaddr=" + hostAddr;
        conn              = PQconnectdb(connectionString.c_str());
        if(PQstatus(conn) == CONNECTION_BAD)
                throw DbConnExc("Connection Error");
}

void PsqlConnection::loadTable(TableName tableName, TableAttr& tableAttr) noexcept(false){
     int             retRows          {0},
                     retFields        {0};
     const string    listContPrefix   {"select * from "};
     string          cmdBuff          {listContPrefix + tableName},
                     errBuff          {""};

     PGresult        *result          {PQexecParams(conn, cmdBuff.c_str(), 0, nullptr, nullptr, nullptr, nullptr, 0)};
     TableData&      tdata            {get<DATA>(tableAttr)};
     Stat&           thisStat         {get<SSTAT>(tableAttr)};
     struct timeval  ltime;
     struct timespec lbuff;
   

     switch(PQresultStatus(result)) {
          case PGRES_TUPLES_OK:
          case PGRES_COMMAND_OK:
                retRows   = PQntuples(result);
                retFields = PQnfields(result);
    
                get<RNUM>(tableAttr)  = retRows;
                thisStat              =  statTempl;

                gettimeofday(&ltime, nullptr);
                lbuff.tv_sec      = ltime.tv_sec;
                lbuff.tv_nsec     = ltime.tv_usec * 1000;
                thisStat.st_atim  = lbuff;
                thisStat.st_mtim  = lbuff;
                thisStat.st_ctim  = lbuff;

                for(int r = 0; r < retRows; r++) {
                    for(int f = 0; f < retFields; f++){
                        char*  rowdata = PQgetvalue(result, r, f);
                        tdata.insert(tdata.end(), rowdata, rowdata + strlen(rowdata));
                        tdata.push_back(';');
                    }
                    tdata.push_back('\n');
                }

                thisStat.st_size = tdata.size();
          break;
          case PGRES_EMPTY_QUERY:
                        errBuff = "Empty Query: ";
          case PGRES_NONFATAL_ERROR:
                        errBuff = "Nofatal Error: ";
          case PGRES_BAD_RESPONSE:
          case PGRES_FATAL_ERROR:
          default:
                        throw DbConnExc(string("Query Error: ").append(errBuff).append(string(PQerrorMessage(conn))));
       }
       PQclear(result);
}

void PsqlConnection::loadDbByOwner(TableList& db, const string owner){
        syslog->log(LOG_DEBUG, "- loadDbByOwner : Loading Tables.");

        int          retRows          {0};
        const string listTables       {"select tablename from pg_tables where tableowner = $1"};
        string       cmdBuff          {""},
                     errBuff          {""};
        const char   *params[2]       {owner.c_str(), nullptr};
        int          lengths[2]       {0,             0};

        try{ 
            lengths[0]       = safeSizeT(owner.size());
        }catch(TypesUtilsException& ex){
             throw DbConnExc("Owner's name too long.");
        }

        if(owner.size() == 0 ) 
             throw DbConnExc("Owner's name param is empty." );

        PGresult     *result          {PQexecParams(conn, listTables.c_str(), 1, nullptr, params, lengths, nullptr, 0)};

        switch(PQresultStatus(result)) {
                case PGRES_TUPLES_OK:
                case PGRES_COMMAND_OK:
                        retRows   = PQntuples(result);

                        for(int r = 0; r < retRows; r++){
                                string tableName = PQgetvalue(result, r, 0);
                                db[tableName];
                        }
                break;
                case PGRES_EMPTY_QUERY:
                        errBuff = "Empty Query: ";
                case PGRES_NONFATAL_ERROR:
                        errBuff = "Nofatal Error: ";
                case PGRES_BAD_RESPONSE:
                case PGRES_FATAL_ERROR:
                default:
                        throw DbConnExc(string("Query Error: ").append(errBuff).append(string(PQerrorMessage(conn))));
        }
        PQclear(result);

        for(auto &i : db)
           loadTable(i.first, i.second); 
}

void PsqlConnection::loadDbByList(TableList& db, const string cfile){
        syslog->log(LOG_DEBUG, "- loadDbByList : Loading Tables.");

        if(cfile.size() == 0 ) 
             throw DbConnExc("Config file's name param is empty." );

        Stat         fileStat;
        int          ret       {stat(cfile.c_str(), &fileStat)};
        if(ret == -1)
             throw DbConnExc(string("Invalid config file").append(strerror(errno)) );
        if(fileStat.st_size == 0 )
             throw DbConnExc(string("Config file is empty"));

        for(auto &i : db)
           loadTable(i.first, i.second); 

        ifstream ifcfg (cfile.c_str(), ifstream::in);

        string tableName;
        while(getline(ifcfg, tableName)){
            auto tableIt = db.find(tableName);
            if(tableIt != db.end())
                get<DATA>(tableIt->second).clear();
            else
                db[tableName]; 

            loadTable(tableName, db[tableName]); 
        }
}

void PsqlConnection::printDebug(const TableList& db) noexcept(true){
     try{
         for(auto i : db){
             syslog->log(LOG_DEBUG, { "- postgresql_utils : printDebug :  Table: ",  
                                      i.first, " - Rows: ",  to_string(get<RNUM>(i.second)), 
                                      " - Characters: ", to_string(get<DATA>(i.second).size())
                                    });
	     const TableData& tdata {get<DATA>(i.second)};

             string buff           {""};
             for(auto ii : tdata)
                 buff.push_back(ii);
             syslog->log(LOG_DEBUG, { "- postgresql_utils : printDebug : ",  buff} );
         }
      }catch(...){
             syslog->log(LOG_DEBUG, "- postgresql_utils : printDebug : unexpected exception.");
      }
}

DbConnection::DbConnection(Syslog *slog) : syslog{slog}{}

DbConnection::~DbConnection(){}

DBIface::DBIface(void){
    dbTypes["postgresql"] = [&](syslogwrp::Syslog *slog){ auto paramsv = unique_ptr<DbConnection>{new PsqlConnection(slog)}; 
                                                          return paramsv; };
}

unique_ptr<DbConnection> DBIface::getDbConn(std::string dbType, syslogwrp::Syslog *slog) noexcept(false){
       return dbTypes[dbType](slog);
}

DBIface& DBIface::getInstance(void){
     static DBIface dBIface;
     return dBIface;
}

} // end namespace dbfsutils
