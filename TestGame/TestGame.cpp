//
//  main.cpp
//  TestGame
//
//  Created by Dexter on 17/03/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#include <sys/time.h>
#include "PropnetReasoner.h"
#include "Game.h"
#include "test_globals.h"

using namespace std;

void testReasoner(const char * filename) {
    cout << "Game: " << filename << endl;
    GdlFactory factory;
    VectorTermPtr gdlCode = factory.computeGdlCode(filename);
    
    Grounder grounder(factory, gdlCode);
    Propnet propnet(grounder);
    Circuit circuit(propnet);
    PropnetReasoner reasoner(circuit);
    
    const VectorTermPtr init_pos = reasoner.getInitPos();
    reasoner.setPosition(init_pos);
    
    reasoner.playout();
    
    for (TermPtr role : reasoner.getRoles()) {
        cout << "score " << *role << " = " << reasoner.getGoal(role) << endl;
    }
}

void testGame(const char * filename) {
    cout << "Game: " << filename << endl;
    GdlFactory factory;
    VectorTermPtr gdlCode = factory.computeGdlCode(filename);
    
    Grounder grounder(factory, gdlCode);
    Propnet propnet(grounder);
    Circuit circuit(propnet);
    
    std::unique_ptr<Reasoner> reasoner(new PropnetReasoner(circuit));
    std::shared_ptr<Game> game(new Game(std::move(reasoner)));
 
    std::default_random_engine rand_gen((std::random_device()).operator()());
    
    // timer
    struct timeval time_start;
    struct timeval time_end;
    float time_limit = 10;
    float current_time = 0;
    int count = 0;

    cout << "start test" << endl;
    // work until timeout
    gettimeofday(&time_start, 0); // start timer

    while(current_time < time_limit) {
        ++count;
        PositionPtr pos = game->getInitPos();
        
        //cout << "Init pos: " << *pos << endl;
        while(!pos->isTerminal()) {
            MovePtr move = pos->getMove((RoleId) pos->getHavePlayed().size(), rand_gen() % pos->getNbLegal((RoleId) pos->getHavePlayed().size()));
            //cout << "Move: " << *move << endl;
            pos = game->next(pos, move);
            //cout << "Pos: " << *pos << endl;
        }
        //cout << pos->getGoal(0) << ' ' << pos->getGoal(1) << endl;

//        vector<Score> scores;
//        game->playout(pos, scores);
//        for(int s : scores)
//            cout << s << ' ';
//        cout << endl;
        
        gettimeofday(&time_end, 0);
        current_time = ((float)(time_end.tv_sec - time_start.tv_sec)) + ((float)(time_end.tv_usec - time_start.tv_usec))/1000000.0;
    }
    cout << "playouts per second:" << float(count) / time_limit << endl;
}

int main(int argc, const char * argv[]) {
    //testReasoner((user_home + "FAC/M2_STAGE/GDL/tictactoe.kif").c_str());
    //testReasoner((user_home + "FAC/TH_(de)composition_des_jeux/GDL_KIF_composed_games/dresden/doubletictactoe.gdl").c_str());

    testGame((user_home + "FAC/M2_STAGE/GDL/tictactoe.kif").c_str());
    
    
    return EXIT_SUCCESS;
}
