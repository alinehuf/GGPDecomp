//
//  test_globals_extern.h
//  playerGGP
//
//  Created by Dexter on 03/11/2015.
//  Copyright © 2015 dex. All rights reserved.
//

#ifndef test_globals_extern_h
#define test_globals_extern_h

// 2 => verbose debug + graphics
// 1 => verbose debug
// PLAYERGGP_DEBUG undef => no debug, test phase + creation of log files

//#define PLAYERGGP_DEBUG 2 // à régler aussi dans test_globals.h

#include <sys/time.h>
#include <fstream>
#include <iomanip>

#define TIME_SPENT(time_start, time_end) \
    std::setiosflags(std::ios::fixed) << std::setprecision(8) << \
    (((float)(time_end.tv_sec - time_start.tv_sec)) +  \
     ((float)(time_end.tv_usec - time_start.tv_usec))/1000000.0)

// global path
const std::string user_home("/Users/alinehuf");
//const std::string user_home("/media/sf_alinehuf/");
const std::string log_dir(user_home + "/Desktop/log/");

// timer
extern struct timeval time_start, time_end, total_time_start, total_time_end;

extern std::ofstream out_time;
extern std::ostream * out_dcmp;

#endif /* test_globals_extern_h */
