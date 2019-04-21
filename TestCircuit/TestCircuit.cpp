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
    factory.printGdlCode(cout, gdlCode);

    
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

//    testCircuitStepByStep((user_home + "FAC/TH3_ggp.org_games/base/blocksWorldSerial_v0.kif").c_str());
    testCircuitStepByStep((user_home + "/FAC/TH2_PlayerGGP/_DEBUG_jeux_tests/blocksWorldSerialMedium.kif").c_str());
    return EXIT_SUCCESS;
}
