//
//  TestCircuit.cpp
//  playerGGP
//
//  Created by alinehuf on 16/03/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#include "Circuit.h"
#include "test_globals.h"

using namespace std;

void testCircuitStepByStep(const char * filename) {
    cout << "Game: " << filename << endl;
    GdlFactory factory;
    VectorTermPtr gdlCode = factory.computeGdlCode(filename);
//    factory.printGdlCode(cout, gdlCode);

    
    Grounder grounder(factory, gdlCode);
    Propnet propnet(grounder);
    Circuit circuit(propnet);
    
    Circuit::Info c_infos = circuit.getInfos();
    const VectorTermPtr init_pos = circuit.getInitPos();
    std::default_random_engine rand_gen((std::random_device()).operator()());
    
    int min = 1000;
    int max = 0;
    int mean = 0;
    int nb_runs = 100;
    vector<double> mean_score(c_infos.roles.size(), 0);
    for (int i = 0; i < nb_runs; ++i){
        VectorBool values(c_infos.values_size, 0);
        circuit.setPosition(values, init_pos);
        cout << endl << "position : " << endl;
        factory.printGdlCode(cout, circuit.getPosition(values));
        cout << endl;
        
        circuit.terminal_legal_goal(values);
        
        int count = 0;
        while(!circuit.isTerminal(values)) {
            ++count;
            
            for (TermPtr role : c_infos.roles) {
                VectorTermPtr legals = circuit.getLegals(values, role);
                cout << "legal moves : " << endl;
                for (const auto& m : legals)
                    cout << "   " << *m << endl;
                TermPtr move = legals[rand_gen() % legals.size()];
                cout << *role << " play " << *move->getArgs().back() << endl;
                circuit.setMove(values, move);
            }
            circuit.next(values);
            
            cout << endl << "position : " << endl;
            factory.printGdlCode(cout, circuit.getPosition(values));
            cout << endl;
        }
        
        if (count < min)
            min = count;
        else if (count > max)
            max = count;
        mean += count;
        for (int r = 0; r < c_infos.roles.size(); r++) {
            TermPtr role = c_infos.roles[r];
            cout << "score " << *role << " = " << circuit.getGoal(values, role) << endl;
            mean_score[r] += circuit.getGoal(values, role);
        }
        if (circuit.isTerminal(values))
            cout << "terminal" << endl;
        else
            cout << "not terminal" << endl;
    }
    mean /= nb_runs;
    cout << "playout size = min : " << min << " max : " << max << " mean : " << mean << endl;
    cout << "score moyen : ";
    for (int r = 0; r < c_infos.roles.size(); r++)
        cout << mean_score[r] / nb_runs;
    cout << endl;
}

void testCircuitPlayout(const char * filename) {
    cout << "Game: " << filename << endl;
    GdlFactory factory;
    VectorTermPtr gdlCode = factory.computeGdlCode(filename);
    
    Grounder grounder(factory, gdlCode);
    Propnet propnet(grounder);
    Circuit circuit(propnet);
    
    VectorBool values(circuit.getInfos().values_size, 0);
    const VectorTermPtr init_pos = circuit.getInitPos();
    circuit.setPosition(values, init_pos);
    
    circuit.playout(values);
    
    for (TermPtr role : circuit.getRoles()) {
        cout << "score " << *role << " = " << circuit.getGoal(values, role) << endl;
    }
}


int main(int argc, const char * argv[]) {
    //testCircuitPlayout((log_dir + "FAC/M2_STAGE/GDL/tictactoe.kif").c_str());
    
    //testCircuitStepByStep((user_home + "FAC/M2_STAGE/GDL/tictactoe.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/M2_STAGE/GDL/eightPuzzle.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/M2_STAGE/GDL/breakthrough.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_Mppa_tests/22_MPPA_circuits_pour_tests/eightPuzzle_terminal_complexifié.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_(de)composition_des_jeux/GDL_KIF_composed_games/stanford/multiplebuttonsandlights.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_LeJoueur/_DEBUG_jeux_tests/tictactoe_not_action2.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_LeJoueur/_DEBUG_jeux_tests/serial_game_X3.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_LeJoueur/_DEBUG_jeux_tests/corridor-mobile-mark_grounded_obfuscated.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_LeJoueur/_DEBUG_jeux_tests/corridor-parallel_grounded_obfuscated.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_LeJoueur/_DEBUG_jeux_tests/corridor-parallel2_grounded.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_LeJoueur/_DEBUG_jeux_tests/corridor-mobile-mark2.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_LeJoueur/_DEBUG_jeux_tests/corridor-mobile-mark.gdl").c_str());
    
    //testCircuitStepByStep((user_home + "FAC/TH_PlayerGGP/_DEBUG_jeux_tests/doubleline.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_PlayerGGP/_DEBUG_jeux_tests/doubleline_parallel.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_PlayerGGP/_DEBUG_jeux_tests/doubleline_parallel_mini.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_PlayerGGP/_DEBUG_jeux_tests/doubleline_parallel1player.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_PlayerGGP/_DEBUG_jeux_tests/doubleline_crossed.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_PlayerGGP/_DEBUG_jeux_tests/colors-parallel.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_PlayerGGP/_DEBUG_jeux_tests/colors-parallel-3players.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_PlayerGGP/_DEBUG_jeux_tests/neveragree.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH_PlayerGGP/_DEBUG_jeux_tests/light2dark_serial.gdl").c_str());
    
    //testCircuitStepByStep((user_home + "FAC/TH_de_composition_des_jeux/GDL_KIF_composed_games/dresden/asteroidsparallel.gdl").c_str());
    
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/serial_game_X3_gameNinprogress.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/serial_game_X3_chevauchement.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/serial_game_X3_chevauchement_2.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/serial_game_X3_chevauchement_3.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/serial_game_X3_chevauchement_4.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/serial_game_X3_chevauchement_5.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/serial_game_X3_3eme_en_parallele.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/serial_game_X3_3eme_en_parallele_2.gdl").c_str());
    
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/candleInTheWind.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/candleInTheWind_2.gdl").c_str());
    
    //testCircuitStepByStep((user_home + "FAC/M2_STAGE/GDL/chomp.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_word2vec4GGP/games_gdl/circlesolitaire_simplified.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/ticTacToeSerial_X3.kif").c_str());
    
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/ttt_align2_Serial.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/ttt_align2.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/roshambo2Parallel.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/asteroidsserial_game1over_dans_next.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/tictactoeserialx4.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/tictactoeserial_normal_parallel.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/tictactoeparallelx2serial.gdl").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH3_ggp.org_games/base/racetrackcorridor_v1.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH3_ggp.org_games/dresden/ticblock_v0.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH3_ggp.org_games/base/atariGoVariant_7x7_v0.kif").c_str());

    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/snakeAssemblit_v1_action_dans_stepper.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/platformjumpers_v0_mini2.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH3_ggp.org_games/base/nonogram_10x10_1_v0.kif").c_str());

//    testCircuitStepByStep((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_1.kif").c_str());
    //testCircuitStepByStep((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_3.kif").c_str());
//    testCircuitStepByStep((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nurikabe_5x5_1.kif").c_str());
//    testCircuitStepByStep((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/puzzleloop_5x5_1.kif").c_str());
    testCircuitStepByStep((user_home + "FAC/TH4_nonogramme/nng-decomp/kif/nonogram_5x5_monotone_1.kif").c_str());

    return EXIT_SUCCESS;
}
