//
//  main.cpp
//  playerGGP
//
//  Created by dexter on 24/10/13.
//  Copyright (c) 2013 dex. All rights reserved.
//

#include <dirent.h>
#include <fstream>
#include "Circuit.h"
#include "Splitter6.h"
#include "UctSinglePlayer.hpp"
#include "UctSinglePlayer2.hpp"
#include "UctSinglePlayer3.hpp"
#include "UctSinglePlayerBW.hpp"
#include "UctSinglePlayerDecomp2.hpp"
#include "UctSinglePlayerDecompAndNormal.hpp"
#include "test_globals.h"
#include <signal.h>
#include <unistd.h>

using namespace std;
#define TIME_VALUE(time_start, time_end) \
        (((float)(time_end.tv_sec - time_start.tv_sec)) +  \
        ((float)(time_end.tv_usec - time_start.tv_usec))/1000000.0)

void playDecomposeGdlFile(const char * filename) {
    // TEST -----
    std::streambuf * buf_dcmp;
    std::ofstream of_dcmp;
#ifndef PLAYERGGP_DEBUG
    of_dcmp.open(log_file_dcmp_path, std::ios::app);
    buf_dcmp = of_dcmp.rdbuf();
#else
    buf_dcmp = std::cout.rdbuf();
#endif
    out_dcmp = new std::ostream(buf_dcmp);

#ifndef PLAYERGGP_DEBUG
    out_time = std::ofstream(log_file_time_path, std::ios::app);
    cout << "Game: " << filename << std::endl;
#endif
    out_time << "Game: " << filename << std::endl;
    *out_dcmp << "Game: " << filename << std::endl;
    // ----------

    GdlFactory factory;
    VectorTermPtr gdlCode = factory.computeGdlCode(filename);

    //factory.printGdlCode(std::cout, gdlCode);
    //factory.printAtomsDict(std::cout);
    //factory.printTermsDict(std::cout);

    Grounder grounder(factory, gdlCode);
    Propnet propnet(grounder);
    Circuit circuit(propnet);

    // TEST -----
#if PLAYERGGP_DEBUG == 3
    circuit.printVars(cout);
#endif
    // ----------

    int num_test = 100;
    int itermax = 500000;
    pair<bool, int> result;

    // TEST AVEC DECOMPOSITION 2 --------------------------------------------------------
    out_time << "-----------------------------------------------------jeu avec decomposition" << endl;
    for (int n = 0; n < num_test; n++) {
        cout << "test " << n+1 << " " << std::flush;
        // TEST -----
        gettimeofday(&total_time_start, 0); // start timer
        gettimeofday (&time_start, 0);
        // ----------

        Splitter6 splitter1(circuit, propnet);

        // TEST -----
        gettimeofday (&time_end, 0);
        out_time << "Decomp in                                     ";
        out_time << TIME_SPENT(time_start, time_end) << " seconds" << endl;
        gettimeofday (&time_start, 0);
        // ----------

        UctSinglePlayerDecomp2 uct1(circuit, splitter1.subgamesTruesOnly());
        result = uct1.run(itermax);

        // TEST -----
        gettimeofday (&time_end, 0);
        gettimeofday(&total_time_end, 0); // get current time
        if (result.first)
            out_time << "Uct with decomp :                             solved (" << result.second << ")" << endl;
        else
            out_time << "Uct with decomp :                             fail (" << itermax << ")" << endl;
        out_time << "Uct with decomp in                            ";
        out_time << TIME_SPENT(time_start, time_end) << " seconds" << endl;
        out_time << "Uct with decomp total time :                  ";
        out_time << TIME_SPENT(total_time_start, total_time_end) << " seconds" << endl;
        // ----------
#if PLAYERGGP_DEBUG == 2
        if (result.first) {
            printf("solved after %d iterations - time %.2f s\n", result.second, TIME_VALUE(total_time_start, total_time_end));
            uct1.printCurrent();
        }
        else
            printf("not solved after %d iterations - time %.2f s\n", result.second,  TIME_VALUE(total_time_start, total_time_end));
#endif
    }

    // TEST AVEC BLACK et WHITE LIST --------------------------------------------------
    /*for (int n = 0; n < num_test; n++) {
        cout << "test " << n+1 << " " << std::flush;
        // TEST -----
        gettimeofday(&total_time_start, 0); // start timer
        gettimeofday (&time_start, 0);
        // ----------

        Splitter6 splitter2(circuit, propnet);

        // TEST -----
        gettimeofday (&time_end, 0);
        out_time << "-----------------------------------------------------" << endl;
        out_time << "Decomp in                                     ";
        out_time << TIME_SPENT(time_start, time_end) << " seconds" << endl;
        gettimeofday (&time_start, 0);
        // ----------

        UctSinglePlayerBW uct2(circuit, splitter2.blackList(), splitter2.whiteList());
        result = uct2.run(itermax);

        // TEST -----
        gettimeofday (&time_end, 0);
        gettimeofday(&total_time_end, 0); // get current time
        if (result.first)
            out_time << "Uct with decomp :                             solved (" << result.second << ")" << endl;
        else
            out_time << "Uct with decomp :                             fail (" << itermax << ")" << endl;
        out_time << "Uct with decomp in                            ";
        out_time << TIME_SPENT(time_start, time_end) << " seconds" << endl;
        out_time << "Uct with decomp total time :                  ";
        out_time << TIME_SPENT(total_time_start, total_time_end) << " seconds" << endl;
        // ----------
#if PLAYERGGP_DEBUG
        if (result.first) {
            printf("solved after %d iterations - time %.2f s\n", result.second, TIME_VALUE(total_time_start, total_time_end));
            uct2.printCurrent();
        }
        else
            printf("not solved after %d iterations - time %.2f s\n", result.second,  TIME_VALUE(total_time_start, total_time_end));
#endif
    }*/

    //TEST SANS DECOMPOSITION -------------------------------------------------------- UCT classique
    /*out_time << "-----------------------------------------------------UCT classique" << endl;
    for (int n = 0; n < num_test; n++) {
        cout << "test " << n+1 << " " << std::flush;
        // TEST -----
        gettimeofday(&total_time_start, 0); // start timer
        // ----------
        UctSinglePlayer uct3(circuit);
        result = uct3.run(itermax);
        // TEST -----
        gettimeofday(&total_time_end, 0); // get current time
        if (result.first)
            out_time << "Uct without decomp :                          solved (" << result.second << ")" << endl;
        else
            out_time << "Uct without decomp :                          fail (" << itermax << ")" << endl;
        out_time << "Uct without decomp total time :               ";
        out_time << TIME_SPENT(total_time_start, total_time_end) << " seconds" << endl;
        // ----------
#if PLAYERGGP_DEBUG
        if (result.first) {
            printf("solved after %d iterations - time %.2f s\n", result.second, TIME_VALUE(total_time_start, total_time_end));
            uct3.printCurrent();
        }
        else
            printf("not solved after %d iterations - time %.2f s\n", result.second,  TIME_VALUE(total_time_start, total_time_end));
#endif
   }*/

    // TEST SANS DECOMPOSITION 2 -------------------------------------------------------- arbre comme sous-jeu sans transpositions
    /*for (int n = 0; n < num_test; n++) {
    cout << "test " << n+1 << " " << std::flush;
        // TEST -----
        gettimeofday(&total_time_start, 0); // start timer
        // ----------
        UctSinglePlayer2 uct4(circuit);
        result = uct4.run(itermax);
        // TEST -----
        gettimeofday(&total_time_end, 0); // get current time
        if (result.first)
            out_time << "Uct without decomp :                          solved (" << result.second << ")" << endl;
        else
            out_time << "Uct without decomp :                          fail (" << itermax << ")" << endl;
        out_time << "Uct without decomp total time :               ";
        out_time << TIME_SPENT(total_time_start, total_time_end) << " seconds" << endl;
        // ----------
#if PLAYERGGP_DEBUG
        if (result.first) {
            printf("solved after %d iterations - time %.2f s\n", result.second, TIME_VALUE(total_time_start, total_time_end));
            uct4.printCurrent();
        }
        else
            printf("not solved after %d iterations - time %.2f s\n", result.second,  TIME_VALUE(total_time_start, total_time_end));
#endif
   }*/

    // TEST SANS DECOMPOSITION 3 -------------------------------------------------------- arbre comme sous-jeu + transpositions
    /* out_time << "-----------------------------------------------------arbre comme sous-jeu + transpositions" << endl;
    for (int n = 0; n < num_test; n++) {
        cout << "test " << n+1 << " " << std::flush;
        // TEST -----
        gettimeofday(&total_time_start, 0); // start timer
        // ----------
        UctSinglePlayer3 uct5(circuit);
        result = uct5.run(itermax);
        // TEST -----
        gettimeofday(&total_time_end, 0); // get current time
        if (result.first)
            out_time << "Uct without decomp :                          solved (" << result.second << ")" << endl;
        else
            out_time << "Uct without decomp :                          fail (" << itermax << ")" << endl;
        out_time << "Uct without decomp total time :               ";
        out_time << TIME_SPENT(total_time_start, total_time_end) << " seconds" << endl;
        // ----------
#if PLAYERGGP_DEBUG
        if (result.first) {
            printf("solved after %d iterations - time %.2f s\n", result.second, TIME_VALUE(total_time_start, total_time_end));
            uct5.printCurrent();
        }
        else
            printf("not solved after %d iterations - time %.2f s\n", result.second,  TIME_VALUE(total_time_start, total_time_end));
#endif
    }*/

    // TEST AVEC DECOMPOSITION + ARBRE GLOBAL -------------------------------------------------------- debug : arbre global + sous-arbres
    /*for (int n = 0; n < num_test; n++) {
        cout << "test " << n+1 << " " << std::flush;
        // TEST -----
        gettimeofday(&total_time_start, 0); // start timer
        gettimeofday (&time_start, 0);
        // ----------

        Splitter6 splitter2(circuit, propnet);

        // TEST -----
        gettimeofday (&time_end, 0);
        out_time << "-----------------------------------------------------" << endl;
        out_time << "Decomp in                                     ";
        out_time << TIME_SPENT(time_start, time_end) << " seconds" << endl;
        gettimeofday (&time_start, 0);
        // ----------

        UctSinglePlayerDecompAndNormal uct6(circuit, splitter2.subgamesTruesOnly());
        result = uct6.run(itermax);

        // TEST -----
        gettimeofday (&time_end, 0);
        gettimeofday(&total_time_end, 0); // get current time
        if (result.first)
            out_time << "Uct with decomp :                             solved (" << result.second << ")" << endl;
        else
            out_time << "Uct with decomp :                             fail (" << itermax << ")" << endl;
        out_time << "Uct with decomp in                            ";
        out_time << TIME_SPENT(time_start, time_end) << " seconds" << endl;
        out_time << "Uct with decomp total time :                  ";
        out_time << TIME_SPENT(total_time_start, total_time_end) << " seconds" << endl;
        // ----------
#if PLAYERGGP_DEBUG == 2
        if (result.first) {
            printf("solved after %d iterations - time %.2f s\n", result.second, TIME_VALUE(total_time_start, total_time_end));
            uct6.printCurrent();
        }
        else
            printf("not solved after %d iterations - time %.2f s\n", result.second,  TIME_VALUE(total_time_start, total_time_end));
#endif
    }*/
    cout << endl;

    // TEST -----
#ifndef PLAYERGGP_DEBUG
    of_dcmp.close();
    string game_path(filename);
    size_t middle_id = game_path.find_last_of("/") + 1; // premier caractère du nom du jeu
    size_t start_id = game_path.find_last_of("/", middle_id - 2) + 1; // premier caractère du nom du repertoire
    size_t stop_id = game_path.find_last_of(".");

    string game_name = game_path.substr(start_id, middle_id - start_id - 1) + string("_") + game_path.substr(middle_id, stop_id - middle_id);
    if (rename(log_file_dcmp_path.c_str(), (string(log_dir) + string("game_dcmp/") + game_name + string("dcmp.txt")).c_str()) != 0)
        perror("rename dcmp");
#endif
    // ----------
}

int main(int argc, const char * argv[]) {
    string test(user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/");
    string base(user_home + "FAC/TH3_ggp.org_games/base/");
    string stanford(user_home + "FAC/TH3_ggp.org_games/stanford/");
    string dresden(user_home + "FAC/TH3_ggp.org_games/dresden/");



//    playDecomposeGdlFile((test + "double_blocks_world.kif").c_str());
//    playDecomposeGdlFile((test + "double_blocks_world_medium.kif").c_str());
//    playDecomposeGdlFile((test + "double_blocks_world_small.kif").c_str());
//    playDecomposeGdlFile((test + "double_blocks_world_very_small.kif").c_str());
//    playDecomposeGdlFile((test + "double_maze.kif").c_str());


//    playDecomposeGdlFile((base + "lightsOnSimul4_v0.kif").c_str());        // RESOLU EN 1 à 3 PLAYOUTS !!!
//    playDecomposeGdlFile((base + "lightsOnSimultaneous_v0.kif").c_str());  // RESOLU EN 1 à 3 PLAYOUTS !!!

//    playDecomposeGdlFile((base + "futoshiki4_v0.kif").c_str());
//    playDecomposeGdlFile((stanford + "selectivesukoshi_v0.kif").c_str());
//    playDecomposeGdlFile((base + "queens08lg_v0.kif").c_str());



    playDecomposeGdlFile((base + "blocksWorldParallel_v0.kif").c_str());

//    playDecomposeGdlFile((base + "incredible_v0.kif").c_str());

//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_1.kif").c_str()); // 14sj - G
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_6.kif").c_str()); // 22sj - damier
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_4.kif").c_str()); // 13sj - tetris
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_3.kif").c_str()); // 12sj - chien assis
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_2.kif").c_str()); // 11sj - iG
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_5.kif").c_str()); // 10sj - sorciere

//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_6x6_3.kif").c_str()); // 33sj - damier clos
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_6x6_1.kif").c_str()); // 22sj - cube
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_6x6_2.kif").c_str()); // 16sj - canard

//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_10x10_4.kif").c_str()); // 61sj - grand damier
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_10x10_3.kif").c_str()); // 37sj - halloween
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_10x10_1.kif").c_str()); // 12sj - cheval
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_10x10_2.kif").c_str()); // 10sj - chaos


//    playDecomposeGdlFile((stanford + "multiplehamilton_v0.kif").c_str());
//    playDecomposeGdlFile((stanford + "multiplesukoshi_v0.kif").c_str());

//    playDecomposeGdlFile((base + "asteroidsParallel_v1.kif").c_str());
//    playDecomposeGdlFile((base + "factoringMutuallyAssuredDestruction_v0.kif").c_str());

//    playDecomposeGdlFile((base + "ruleDepthLinear_v0.kif").c_str());
//    playDecomposeGdlFile((dresden + "ruledepthlinear_v0.kif").c_str());

//    playDecomposeGdlFile((base + "queens31lg_v0.kif").c_str()); // trop long
//    playDecomposeGdlFile((base + "hanoi_6_disks_v0.kif").c_str()); // mal decomposé...



//    playDecomposeGdlFile((test + "jeu_simpliste.kif").c_str());
//    playDecomposeGdlFile((test + "choose10-10.kif").c_str());

//    playDecomposeGdlFile((user_home + "FAC/TH3_ggp.org_games/base/firefighter_v0.kif").c_str()); // trop long

//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_game_Nsoluce.kif").c_str());
//    playDecomposeGdlFile((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_monotone_1.kif").c_str());

    return EXIT_SUCCESS;
}
