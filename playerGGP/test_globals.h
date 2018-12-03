//
//  text_globals.h
//  playerGGP
//
//  Created by Dexter on 03/11/2015.
//  Copyright © 2015 dex. All rights reserved.
//

#ifndef text_globals_h
#define text_globals_h

#include <sys/time.h>
#include <fstream>
#include <iomanip>

// 2 => verbose debug + graphics
// 1 => verbose debug
// PLAYERGGP_DEBUG undef => no debug, test phase + creation of log files

//#define PLAYERGGP_DEBUG 2 // à régler aussi dans test_globals_extern.h

#define TIME_SPENT(time_start, time_end) \
std::setiosflags(std::ios::fixed) << std::setprecision(8) << \
(((float)(time_end.tv_sec - time_start.tv_sec)) +  \
((float)(time_end.tv_usec - time_start.tv_usec))/1000000.0)

// timer
struct timeval time_start, time_end, total_time_start, total_time_end;

// path and file names
const std::string user_home("/Users/alinehuf/");
//const std::string user_home("/media/sf_alinehuf/");
const std::string log_dir(user_home + "Desktop/log/");

const std::string log_file_time_name("log_time.txt");
const std::string log_file_time_path = log_dir + log_file_time_name;

const std::string log_file_dcmp_name("log_dcmp.txt");
const std::string log_file_dcmp_path = log_dir + log_file_dcmp_name;

std::ofstream out_time;
std::ostream * out_dcmp;

#endif /* text_globals_h */
