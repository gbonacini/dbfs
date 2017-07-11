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

#ifndef  DB__FS
#define  DB__FS

#define FUSE_USE_VERSION 26
#define _XOPEN_SOURCE    700

#ifndef USE_FDS
#define USE_FDS 15
#endif

#include <config.h>

#include <fuse.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <signal.h>

#include <exception>
#include <stdexcept>
#include <map>
#include <vector>
#include <tuple>
#include <string>
#include <cstring>
#include <utility>
#include <iostream>
#include <limits>
#include <memory>
#include <algorithm>
#include <thread> 
#include <atomic> 
#include <mutex>
#include <condition_variable>

#include <db_utils.hpp>
#include <syslog.hpp>

namespace dbfs{

    typedef struct fuse_operations                    Fuse;
    typedef struct fuse_file_info                     FileInfo;

    typedef std::string                               Filename;
    typedef std::string                               Path;
    typedef dbfsutils::TableList                      Filesystem;
    typedef struct sockaddr                           Sockaddr; 
    typedef struct sockaddr_un                        SockaddrUn;
    typedef siginfo_t                                 Siginfo;
    typedef struct sigaction                          Sigaction;

    void genericExcPtrHdlr(syslogwrp::Syslog* slog, std::exception_ptr exptr)            noexcept(false);

    class Dbfs{
         public:
                    Fuse   fuse;
                          
             static Dbfs*  getInstance(               void)                               noexcept(true);

             static Dbfs*  setInstance(               const std::string& dir,
                                                      syslogwrp::Syslog* slog,
                                                      const std::string& confFile,
                                                      const std::string& tableOwner)      noexcept(false);
           
                           ~Dbfs(                     void);
             bool          initFileSystem(            std::string         dbname,
                                                      std::string         user, 
                                                      std::string         address,
                                                      std::string         port,   
                                                      std::string         pwd)            noexcept(false);
             bool          refreshDb(                 void)                               noexcept(false);
           
             static void   refreshHdlr(               int                 sig, 
                                                      Siginfo             *sinfo,   
                                                      void                *ctx)           noexcept(true);
             static bool   extractIds(                const char*         buff,
                                                      std::string&        path, 
                                                      std::string&        file)           noexcept(true);
             static int    readdirCb(                 const char          *path, 
                                                      void                *buf, 
                                                      fuse_fill_dir_t     filler, 
                                                      off_t               offset, 
                                                      FileInfo            *fi)            noexcept(true);
             static int    openCb(                    const char          *path, 
                                                      FileInfo            *fi)            noexcept(true);
             static int    readCb(                    const char          *path, 
                                                      char                *buf,
                                                      size_t              size, 
                                                      off_t               offset,
                                                      FileInfo            *fi)            noexcept(true); 
             static int    getattrCb(                 const char          *path,
                                                      dbfsutils::Stat     *stbuf)         noexcept(true);
               
             static const  dbfsutils::Stat            statTempl;
         private:
                           Dbfs(                      const std::string& dir,
                                                      syslogwrp::Syslog* slog,
                                                      const std::string& confFile,
                                                      const std::string& tableOwner);
                           Dbfs(                      Dbfs const&);
                    void   operator=(                 Dbfs const&);
                    void   openSrvSocket(             void)                               noexcept(false);
                  static   syslogwrp::Syslog          *syslog;
 
                           std::string                mountPoint,
                                                      configurationFile,
                                                      owner,
                                                      dbName,
                                                      userName, 
                                                      dbAddress,
                                                      dbPort,   
                                                      dbPwd;
                           std::unique_ptr<dbfsutils::DbConnection>
                                                      dbconn;
                           std::thread                *thRefresh; 
                  static   Dbfs*                      singleDbfs;
                  static   Filesystem                 fsdb;
                  static   std::atomic<bool>          refreshing; 
                  static   std::atomic<unsigned long> running; 
                  static   Sigaction                  saction;
                  static   std::mutex                 mtxRefresh;
                  static   std::condition_variable    cndRefresh;
    };

} // namespace dbfs

#endif
