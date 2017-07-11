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
#include <syslog.hpp>

using namespace std;
using namespace dbfs;
using namespace dbfsutils;
using namespace syslogwrp;
using namespace typeutils;

void paramError(const char* progname, const char* err=nullptr){
    if(err != nullptr) cerr << err << endl << endl;

    cerr << "dbfs - Mounting a db like a file system. GBonacini - (C) 2017   " << endl;
    cerr << "Version: " << VERSION << endl;
    cerr << "Syntax: " << endl;
    cerr << "       " << progname << " [-m mountpoint] [-d db_name] [-u user] [-a address] [-p port] [-o owner] [-f filepath] [-P password] [-D] | [-h]" << endl;
    cerr << "       " << "-m sets the mount point." << endl;
    cerr << "       " << "-d sets the db name."    << endl;
    cerr << "       " << "-u sets the user name."  << endl;
    cerr << "       " << "-a sets the db address." << endl;
    cerr << "       " << "-f sets a custom refresh file." << endl;
    cerr << "       " << "-o sets the user name of the tables' owner." << endl;
    cerr << "       " << "-p sets the db port."    << endl;
    cerr << "       " << "-P sets the db password." << endl;
    cerr << "       " << "-D sets the debug mode." << endl;
    cerr << "       " << "-h print this help message." << endl;

    exit(1);
}

int main(int argc, char *argv[]) {
    int           fuseErr           {0};
    exception_ptr exPtr;
    const string  DBFS_CONF_FILE    {"./dbfs.config"};

    Syslog        syslog("Dbfs ", LOG_PID|LOG_NDELAY, LOG_AUTHPRIV);

    try{
        string         mountpoint  {""},
                       dbname      {""},
                       user        {""},
                       address     {""},
                       port        {""},
                       pwd         {""},
                       tablesOwner {""},
                       cfgFile     {""};
        const char     flags[]     {"m:d:u:a:p:P:f:o:hD"};
        
        int            c           {0};
        bool           debug       {false};
    
        vector<string> parVals;
    
        parVals.push_back(argv[0]); 
        parVals.push_back("-o"); 
        parVals.push_back("nonempty"); 
    
        opterr         =  0;
        while ((c = getopt(argc, argv, flags)) != -1){
                switch (c){
                    case 'm':
                             mountpoint  = optarg;
                    break;
                    case 'd':
                             dbname      = optarg;
                    break;
                    case 'u':
                             user        = optarg;
                    break;
                    case 'a':
                             address     = optarg;
                    break;
                    case 'p':
                             port        = optarg;
                    break;
                    case 'P':
                             pwd         = optarg;
                    break;
                    case 'f':
                             cfgFile     = optarg;
                    break;
                    case 'o':
                             tablesOwner = optarg;
                    break;
                    case 'D':
		             debug       = true;
                    break;
                    case 'h':
                             paramError(argv[0]);
                }
        }

        if(mountpoint.size() == 0 || dbname.size() == 0 || user.size() == 0 ||
           address.size() == 0    || optind != argc )
               paramError(argv[0], "Invalid parameter(s).");

        if(tablesOwner.size() != 0 )
             cerr << "'Owner' parameter specified: the config file will be ignored." << endl;
        else 
	     if(cfgFile.size() == 0)  cfgFile = DBFS_CONF_FILE;

        if(debug)
            syslog.setPriority(LOG_UPTO (LOG_DEBUG));
        else
            syslog.setPriority(LOG_UPTO (LOG_WARNING));

        parVals.push_back(mountpoint); 
    
        int                 paramsc      {safeInt(parVals.size())};
        unique_ptr<char*[]> paramsv {new char*[paramsc+1]()}; 
    
        for(size_t i=0; i<parVals.size(); i++){
            paramsv[i] = strdup(parVals[i].c_str());
            if(debug) cerr << "- Fuse main param: <" << paramsv[i] << ">" <<  endl;
        }

        Dbfs* dbfs          {Dbfs::setInstance(mountpoint, &syslog, cfgFile, tablesOwner)};

        if(!dbfs->initFileSystem(dbname, user, address, port, pwd)){
	   cerr << "Init Error: File System." << endl;
           throw(string("Init Error: File System."));	   
	};

        fuseErr     =  fuse_main(paramsc, paramsv.get(), &(dbfs->fuse), nullptr);

    }catch(DbConnExc& ex){
        cerr << ex.what() << endl;
    }catch(SyslogExc& ex){
        cerr << ex.what() << endl;
    }catch(string& ex){
        cerr << ex << endl;
    }catch(...){
	exPtr = current_exception();
	genericExcPtrHdlr(&syslog, exPtr);
	fuseErr = EXIT_FAILURE;
    }

    return fuseErr;
}
