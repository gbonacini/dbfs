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

#ifndef  DB__FS__SYSLOG
#define  DB__FS__SYSLOG

#include <initializer_list>
#include <string>

#include <syslog.h>

namespace syslogwrp{
    class Syslog{
        public:
            Syslog(std::string ident, int opts, int facility);
            ~Syslog(void);
            int getPriority(void)                                  const noexcept(true);
            int getOldPriority(void)                               const noexcept(true);
            void setPriority(int newPriority)                            noexcept(true);
            void log(int msgPriority, 
                     std::initializer_list<std::string> arguments) const noexcept(true);
            void log(int msgPriority, const char* msg)             const noexcept(true); 
        private:
            int priority;
            int previousPriority;
};

class SyslogExc final {
      public:
         explicit    SyslogExc(int errNum);
         explicit    SyslogExc(std::string&  errString);
         explicit    SyslogExc(std::string&& errString);
                     SyslogExc(int errNum, std::string errString);
         std::string what(void)                                        const noexcept(true);
      private:
         int errorCode;
         std::string errorMessage;
   };

} // End namespace syslog

#endif
