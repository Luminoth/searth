/*
====================
File: main.cc
Author: Shane Lillie
Description: Main source file

Copyright 2003 Energon Software

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
----
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
====================
*/


#include <cassert>
#include <memory>
#include <iostream>

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "main.h"
#include "SEarth.h"
#include "Callstack.h"


/*
 *  external globals
 *
 */


extern int errno;


/*
 *  functions
 *
 */


inline void print_usage()
{
    ENTER_FUNCTION(print_usage);

    std::cout << "Usage: " << PROGRAM_NAME << "  [options]" << std::endl << std::endl
            << "Options:" << std::endl
            << "-depth [depth]\tSet the window depth" << std::endl
            << "-fullscreen\tRun in fullscreen mode" << std::endl
            << "-window\t\tRun in window mode" << std::endl
            << "-m\t\tTurn music off" << std::endl
            << "-s\t\tTurn sound off" << std::endl
            << "-h\t\tPrint this message" << std::endl << std::endl;
}


bool process_arguments(const int argc, char* const argv[], SEarth* const searth)
{
    ENTER_FUNCTION(process_arguments);

    assert(searth);

    std::cout << "Command line arguments: ";
    for(int i=1; i<argc; ++i)
        std::cout << argv[i] << " ";
    std::cout << std::endl;

/* FIXME: verify the depth argument */
    for(int i=1; i<argc; ++i) {
        if(!strcmp("-d", argv[i]) || !strcmp("-depth", argv[i]))
            searth->set_depth(std::atoi(argv[++i]));
        else if(!strcmp("-f", argv[i]) || !strcmp("-fullscreen", argv[i]))
            searth->set_fullscreen(true);
        else if(!strcmp("-window", argv[i]))
            searth->set_fullscreen(false);
        else if(!strcmp("-m", argv[i]))
            searth->set_music(false);
        else if(!strcmp("-s", argv[i]))
            searth->set_sounds(false);
        else if(!strcmp("-h", argv[i]) || !strcmp("-help", argv[i])) {
            std::cout << std::endl;
            print_usage();
            exit(0);
        } else {
            std::cout << std::endl;
            print_usage();
            exit(0);
        }
    }
    return true;
}


// ensures the data directory exists
bool test_datadir()
{
    ENTER_FUNCTION(test_datadir);

    struct stat buf;
    if(stat(SEarth::data_directory().c_str(), &buf)) {
        std::cerr << "Could not stat datadir (" << SEarth::data_directory() << "): " << strerror(errno) << std::endl;
        return false;
    }

#if !defined WIN32
    if(!S_ISDIR(buf.st_mode)) {
        std::cerr << "datadir (" << SEarth::data_directory() << ") is not a directory" << std::endl;
        return false;
    }
#endif

    return true;
}


int main(int argc, char* argv[])
{
    ENTER_FUNCTION(main);

    std::auto_ptr<SEarth> searth;
    try {
        std::auto_ptr<SEarth> s(new SEarth);
        searth = s;
    } catch(Engine::EngineException& e) {
        std::cerr << "Engine initialization failed: " << e.what() << std::endl;
        SDL_Quit();
        return 1;
    }

    if(!process_arguments(argc, argv, searth.get()))
        return 1;

    if(!test_datadir()) {
        SDL_Quit();
        return 1;
    }

    searth->main();

#if defined DEBUG
    Callstack::dump_calls(std::cout);
#endif
    return 0;
}
