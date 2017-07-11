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

#include <syslog.hpp>

using std::initializer_list;
using std::string;

namespace syslogwrp{

    Syslog::Syslog(string ident, int opts, int facility){
            openlog(ident.c_str(), opts, facility);
    }
    
    Syslog::~Syslog(void){
            closelog();
    }
    
    int Syslog::getPriority(void) const noexcept(true){
            return priority;
    }
    
    int Syslog::getOldPriority(void) const noexcept(true){
            return previousPriority;
    }
    
    void Syslog::setPriority(int newPriority) noexcept(true){
            priority         = newPriority;
            previousPriority = setlogmask(newPriority);
    }
    
    void Syslog::log(int msgPriority, const char* msg) const noexcept(true){
            syslog(msgPriority, "%s", msg);
    }

    void Syslog::log(int msgPriority,  initializer_list<string> arguments) const noexcept(true){
            string body;
            for(string arg : arguments ) body.append(arg);
            syslog(msgPriority, "%s",  body.c_str());
    }


    SyslogExc::SyslogExc(int errNum)
                         : errorCode{errNum}, errorMessage{"None"}{}

    SyslogExc::SyslogExc(string& errString)
                         : errorCode{0}, errorMessage{errString}{}

    SyslogExc::SyslogExc(string&& errString)
                         : errorCode{0}, errorMessage{move(errString)}{}

    SyslogExc::SyslogExc(int errNum, string errString)
                         : errorCode{errNum}, errorMessage{errString}{}

    string SyslogExc::what() const noexcept(true){
      return errorMessage;
    }


} // End namespace syslog
