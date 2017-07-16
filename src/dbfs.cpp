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

#include <dbfs.hpp>

using std::string;
using std::get;
using std::endl;
using std::vector;
using std::map;
using std::copy;
using std::fill;
using std::cerr;
using std::to_string;
using std::exception_ptr;
using std::exception;
using std::rethrow_exception;
using std::current_exception;
using std::numeric_limits; 
using std::atomic;
using std::mutex;
using std::condition_variable;
using std::unique_lock;
using std::memory_order_relaxed;

using dbfsutils::DbConnExc;
using dbfsutils::Stat;
using dbfsutils::SSTAT;
using dbfsutils::DATA;
using dbfsutils::RNUM;
using dbfsutils::TableData;
using dbfsutils::DBIface;

using syslogwrp::Syslog;

namespace dbfs{

    const string        ROOT_DIR                 {"/"}; 
    const string        PATH_SEPARATOR           {"/"}; 

    enum                STDCONST                 { STRBUFF_LEN=1024 };

    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    #endif

    atomic<bool>           Dbfs::refreshing(false);
    atomic<unsigned long>  Dbfs::running(0);
    mutex                  Dbfs::mtxRefresh;
    condition_variable     Dbfs::cndRefresh;
    Syslog*                Dbfs::syslog          {nullptr};
    Sigaction              Dbfs::saction         {};
    Filesystem             Dbfs::fsdb;
    Dbfs*                  Dbfs::singleDbfs      {nullptr}; 

    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif

    void genericExcPtrHdlr(Syslog* slog, exception_ptr exptr)   noexcept(false){
           try {
              if(exptr) rethrow_exception(exptr);
           }catch(const exception& e) {
              cerr << "- Caught Unexpected Exception : " << e.what() << endl;
              slog->log(LOG_ERR, {"- Caught Unexpected Exception : ", e.what()});
           }
    }

    void Dbfs::refreshHdlr( int sig, Siginfo *sinfo, void *ctx) noexcept(true){
         static_cast<void>(sinfo);
         static_cast<void>(ctx);
         static_cast<void>(sig);

         exception_ptr exPtr; 
         
         try{
             Dbfs::refreshing.store(true, memory_order_relaxed); 
    
             Dbfs::syslog->log(LOG_DEBUG, "- refreshHdlr : received refresh signal.");
    
             Dbfs::syslog->log(LOG_DEBUG, "- refreshHdlr : waiting the end of I/O on the old data.");
             for(unsigned long int s = Dbfs::running.load(memory_order_relaxed); 
                 s!=0; s=Dbfs::running.load(memory_order_relaxed)){
                   Dbfs::syslog->log(LOG_DEBUG, {"- refreshHdlr : I/O threads running: ", to_string(s)});
                   sleep(1); 
             }

             Dbfs::syslog->log(LOG_DEBUG, "- refreshHdlr : refreshing.");
             Dbfs::getInstance()->refreshDb();

             Dbfs::syslog->log(LOG_DEBUG, "- refreshHdlr : all data load, sending notification..");
             unique_lock<mutex> lock(Dbfs::mtxRefresh);
             Dbfs::cndRefresh.notify_all();

             Dbfs::syslog->log(LOG_DEBUG, "- refreshHdlr : end.");
             Dbfs::refreshing.store(false, memory_order_relaxed); 
         }catch(...){
             exPtr = current_exception();
	     genericExcPtrHdlr(Dbfs::syslog, exPtr);
         }
    }

    int Dbfs::getattrCb(const char *path, Stat *stbuf) noexcept(true){
      Dbfs::syslog->log(LOG_DEBUG, {"- getattrCb : FullPath:", path});

      exception_ptr exPtr; 
      int    res             {0};
      bool   refr            {Dbfs::refreshing.load(memory_order_relaxed)};

      unique_lock<mutex> lock(Dbfs::mtxRefresh);
      while(refr)
         Dbfs::cndRefresh.wait(lock);

         Dbfs::running.store(Dbfs::running.load(memory_order_relaxed) + 1, memory_order_relaxed); 

      try{
         #ifdef __GNUC__
         #pragma GCC diagnostic push
         #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
         #endif

         *stbuf = {};

         #ifdef __GNUC__
         #pragma GCC diagnostic pop
         #endif
   
         string fullPath{path}, 
                fileName{path}; 

         if(fullPath.compare(ROOT_DIR) == 0) {
	       stbuf->st_mode  = S_IFDIR | 0777;
               stbuf->st_nlink = 2;
                
               goto   END;
         }
   
         Dbfs::extractIds(path, fullPath, fileName);
   
         Dbfs::syslog->log(LOG_DEBUG, {"- getattrCb - Path: <", fullPath, "> - File Name<", fileName, ">"});
   
         auto file = Dbfs::fsdb.find(fileName);
         if(file != Dbfs::fsdb.end()){
	     *stbuf  = get<SSTAT>(file->second);
             Dbfs::syslog->log(LOG_DEBUG, {"- getattrCb - Found file: <", fileName, "> size: ", to_string(stbuf->st_size), " - owner: <", to_string(stbuf->st_uid), ">"});

             goto   END;
         }else{
             res =  -ENOENT;
         }
      }catch(...){
          exPtr = current_exception();
	  genericExcPtrHdlr(Dbfs::syslog, exPtr);
          res =  -ENOENT;
      } 

      END:

      Dbfs::running.store(Dbfs::running.load(memory_order_relaxed) - 1, memory_order_relaxed); 
      return res;
    }

    bool Dbfs::extractIds(const char* buff, string& path, string& file) noexcept(true){
	 exception_ptr exPtr;

	 try{
	     path           = buff;
	     file           = buff;
             size_t lastSep = path.find_last_of(PATH_SEPARATOR);
             if(lastSep != string::npos && lastSep != 0){
                  path.erase(path.find_last_of(PATH_SEPARATOR), path.size()-1);
                  file.erase(0, lastSep+1);
             }else if(lastSep == 0){
	          path = ROOT_DIR;
                  file.erase(0, 1);
             }
	  }catch(...){
		exPtr = current_exception();
		genericExcPtrHdlr(Dbfs::syslog, exPtr);
		return true;
          }

	  return false;
    }
    
    int Dbfs::readdirCb(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, FileInfo *fi) noexcept(true){
   
      static_cast<void>(offset);
      static_cast<void>(fi);
      static_cast<void>(path);

      Dbfs::syslog->log(LOG_DEBUG, "- readdirCb.");

      exception_ptr exPtr;

      int  ret  {0};

      try{
          bool refr {Dbfs::refreshing.load(memory_order_relaxed)};

          unique_lock<mutex> lock(Dbfs::mtxRefresh);
          while(refr)
             Dbfs::cndRefresh.wait(lock);

          Dbfs::running.store(Dbfs::running.load(memory_order_relaxed) + 1, memory_order_relaxed); 

          filler(buf, ".", nullptr, 0);
          filler(buf, "..", nullptr, 0);
    
          string fpath{path}; 
    
          Dbfs::syslog->log(LOG_DEBUG, {"- readdirCb: Path: <", fpath, ">"});
    
          for(auto &eit : Dbfs::fsdb){
	       filler(buf, eit.first.c_str(), nullptr, 0);
               Dbfs::syslog->log(LOG_DEBUG, {"- readdirCb: File: <", eit.first, ">"});
          }
      }catch(...){
          exPtr =  current_exception();
	  genericExcPtrHdlr(Dbfs::syslog, exPtr);
	  ret   =  -EIO;
      }

      Dbfs::running.store(Dbfs::running.load(memory_order_relaxed) - 1, memory_order_relaxed); 
      return ret;
    }

    int Dbfs::openCb(const char *path, FileInfo *fi) noexcept(true){
      Dbfs::syslog->log(LOG_DEBUG, "- openCb.");
    
      static_cast<void>(path);
      static_cast<void>(fi);

      return 0;
    }

    int Dbfs::readCb(const char *path, char *buf, size_t size, off_t offset,
                     FileInfo *fi) noexcept(true){
    
      static_cast<void>(fi);

      Dbfs::syslog->log(LOG_DEBUG, {"- readCb: Full Path: ", path, " Size requested:", to_string(size), " Offset: ", to_string(offset)});

      exception_ptr exPtr; 

      int  ret  {0};

      try{
          bool refr {Dbfs::refreshing.load(memory_order_relaxed)};
          unique_lock<mutex> lock(Dbfs::mtxRefresh);
          while(refr)
             Dbfs::cndRefresh.wait(lock);

          Dbfs::running.store(Dbfs::running.load(memory_order_relaxed) + 1, memory_order_relaxed); 

          string fullPath    {""},
                 fileName    {""}; 
    
          Dbfs::extractIds(path, fullPath, fileName);
    
          Dbfs::syslog->log(LOG_DEBUG, {"- readCb: Path: ", fullPath, " File Name: ", fileName});
        
          auto file = Dbfs::fsdb.find(fileName);
          if(file != Dbfs::fsdb.end() ){
              size_t len = get<SSTAT>(file->second).st_size;
              Dbfs::syslog->log(LOG_DEBUG, {"- readCb: Size: ", to_string(len)});
    
              if(offset < 0){
                  Dbfs::syslog->log(LOG_ERR, "- readCb: Offset negative.");
                  ret  =  -ENOENT;
                  goto END;
              }
    
              if(len > INT_MAX){
                  Dbfs::syslog->log(LOG_ERR, "- readCb: INT_MAX exceeded.");
                  ret  =  -ENOENT;
                  goto END;
              }
        
              if (static_cast<size_t>(offset) >= len){
                  Dbfs::syslog->log(LOG_ERR, "- readCb: end of file exceeded.");
                  goto END;
              } 

              const TableData& tdata = get<DATA>(file->second);  

              if (offset + size > len) {
                      Dbfs::syslog->log(LOG_DEBUG, {"- readCb: reading all the available bytes from: ", to_string(offset), " to: ",  to_string(offset+len)});
		      copy(tdata.data() + offset, tdata.data() + len, buf);
                      ret =   len - offset;
                      goto END;
              }
            
              Dbfs::syslog->log(LOG_DEBUG, {"- readCb: reading from: ", to_string(offset), " to: ",  to_string(offset+len)});
              copy(tdata.data() + offset, tdata.data() + offset + size, buf);

              ret =   size;
          }
      }catch(...){
	  exPtr = current_exception(); 
	  genericExcPtrHdlr(Dbfs::syslog, exPtr);
          ret =   -ENOENT;
      }     

      END:

      Dbfs::running.store(Dbfs::running.load(memory_order_relaxed) - 1, memory_order_relaxed); 
      return ret;
    }

    Dbfs*  Dbfs::setInstance(const string& pdir, Syslog* slog, const string& confFile , const string& tableOwner) noexcept(false){
	  static Dbfs* sDbfs; 
          if(Dbfs::singleDbfs == nullptr){
	      sDbfs = new Dbfs(pdir, slog, confFile, tableOwner);
              sDbfs->Dbfs::singleDbfs = sDbfs;
          }
          return Dbfs::singleDbfs; 
    }

    Dbfs* Dbfs::getInstance(void) noexcept(true){

          return Dbfs::singleDbfs;
    }

    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    #endif

    Dbfs::Dbfs(const string& dir, Syslog* slog, const string& confFile, const string& tableOwner) 
               : mountPoint{dir}, configurationFile{confFile}, owner{tableOwner}, dbName{""}, 
                 userName{""}, dbAddress{""}, dbPort{""}, dbPwd{""}, dbconn{DBIface::getInstance().getDbConn("postgresql", slog)}, thRefresh{nullptr} {

         syslog           = slog;

         fuse             = {};

         fuse.getattr     = Dbfs::getattrCb;
         fuse.open        = Dbfs::openCb;
         fuse.read        = Dbfs::readCb;
         fuse.readdir     = Dbfs::readdirCb;

         Dbfs::saction.sa_sigaction = &Dbfs::refreshHdlr;
         Dbfs::saction.sa_flags     = SA_SIGINFO | SA_RESTART; // TODO: check
         sigfillset(&Dbfs::saction.sa_mask);

         if(sigaction(SIGUSR2, &Dbfs::saction, nullptr) ==  -1) {
              Dbfs::syslog->log(LOG_ERR, {"- Dbfs cons: setting signal handler", strerror(errno)});
              throw(string("Dbfs cons: error setting signal handler."));
	 }
    }

    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif

    Dbfs::~Dbfs(void){ 
        delete Dbfs::getInstance();
    }

    bool  Dbfs::initFileSystem(string dbname, string user, string address, string port, string pwd)  noexcept(false){

         dbName      = dbname;
         userName    = user;
         dbAddress   = address;
         dbPort      = port;
         dbPwd       = pwd;
    
        Dbfs::syslog->log(LOG_DEBUG, "- initFileSystem: begin.");
        bool ret = true;
        try{
            Dbfs::syslog->log(LOG_DEBUG, {"- initFileSystem - connecting - db: ", dbname, " usr: ", user, " addr: ", address, " port: ", port });
            dbconn->connect(dbname, user, address, port, pwd);
            Dbfs::syslog->log(LOG_DEBUG, {"- initFileSystem - loading tables - owner: ", owner });
            if(owner.size() != 0 && !Dbfs::refreshing)
                dbconn->loadDbByOwner(Dbfs::fsdb, owner);
            else
                dbconn->loadDbByList( Dbfs::fsdb, configurationFile);

            if(Dbfs::syslog->getPriority() == LOG_DEBUG) dbconn->printDebug(Dbfs::fsdb);

        }catch(DbConnExc* ex){
            cerr << ex->what() << endl;
            Dbfs::syslog->log(LOG_ERR, {"- initFileSystem: psql exception:", ex->what()});
            ret  =  false;
        }
	return ret;
    }

bool  Dbfs::refreshDb(void) noexcept(false){
    if(dbName.size() == 0    || userName.size() == 0 ||
       dbAddress.size() == 0 || dbPort.size() == 0   ||
       dbPwd.size() == 0 )
            return false;
       
    return initFileSystem(dbName, userName, dbAddress, dbPort, dbPwd);
}


} // namespace dbfs
