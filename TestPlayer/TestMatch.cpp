//
//  main.cpp
//  TestPlayer
//
//  Created by Dexter on 12/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#include "PropnetReasoner.h"
#include "UctPlayer.h"
#include "RandPlayer.h"
#include "Match.h"
#include "test_globals.h"

using namespace std;

void testMatchUct(const char * filename) {
    cout << "Game: " << filename << endl;
    GdlFactory factory;
    VectorTermPtr gdlCode = factory.computeGdlCode(filename);
    
    Grounder grounder(factory, gdlCode);
    Propnet propnet(grounder);
    Circuit circuit(propnet);
    std::unique_ptr<Reasoner> reasoner(new PropnetReasoner(circuit));
    std::shared_ptr<Game> game(new Game(std::move(reasoner)));
    
    vector<Player *> players(circuit.getRoles().size());
    for (size_t i = 0; i < players.size(); ++i) {
        players[i] = new UctPlayer(game, (RoleId) i);
    }
    
    Match match(game, players);
    vector<Score> scores = match.play_with_time(2);
    //vector<Score> scores = match.play_with_iter(50000);
    
    for(RoleId i = 0; i < players.size(); ++i) {
       cout << "player" << i << " = " << scores[i] << endl;
       delete players[i];
    }
}

void testUctVsRandom(const char * filename) {
    cout << "Game: " << filename << endl;
    GdlFactory factory;
    VectorTermPtr gdlCode = factory.computeGdlCode(filename);
    
    Grounder grounder(factory, gdlCode);
    Propnet propnet(grounder);
    Circuit circuit(propnet);
    
    vector<Player *> players(circuit.getRoles().size());
    
    std::unique_ptr<Reasoner> reasoner(new PropnetReasoner(circuit));
    std::shared_ptr<Game> game(new Game(std::move(reasoner)));
    players[0] = new UctPlayer(game, 0);
    
    for (size_t i = 1; i < players.size(); ++i) {
        players[i] = new RandPlayer(game, (RoleId) i);
    }
    
    Match match(game, players);
    vector<Score> scores = match.play_with_time(1);
    //vector<Score> scores = match.play_with_iter(100000);
    
    for(RoleId i = 0; i < players.size(); ++i) {
        cout << "player" << i << " = " << scores[i] << endl;
        delete players[i];
    }
}

void testFullyExplored(const char * filename) {
    cout << "Game: " << filename << endl;
    GdlFactory factory;
    VectorTermPtr gdlCode = factory.computeGdlCode(filename);
    Grounder grounder(factory, gdlCode);
    Propnet propnet(grounder);
    Circuit circuit(propnet);
    std::unique_ptr<Reasoner> reasoner(new PropnetReasoner(circuit));
    std::shared_ptr<Game> game(new Game(std::move(reasoner)));
    Player * player = new UctPlayer(game, 0);
    
    player->start();
    boost::this_thread::sleep_for(boost::chrono::seconds(100));
    player->stop();
    
    delete player;
}


void testOnePlayerPermanentSearchPlayFirst(const char * filename) {
    cout << "Game: " << filename << endl;
    GdlFactory factory;
    VectorTermPtr gdlCode = factory.computeGdlCode(filename);
    Grounder grounder(factory, gdlCode);
    Propnet propnet(grounder);
    Circuit circuit(propnet);
    std::unique_ptr<Reasoner> reasoner(new PropnetReasoner(circuit));
    std::shared_ptr<Game> game(new Game(std::move(reasoner)));
    
    Circuit circuit1(propnet);
    std::unique_ptr<Reasoner> reasoner1(new PropnetReasoner(circuit1));
    std::shared_ptr<Game> game1(new Game(std::move(reasoner1)));
    
    Player * player = new UctPlayer(game1, 0);
    
    PositionPtr pos = game->getInitPos();
    
    player->start();
    
    int i = 0;
    vector<MovePtr> moves(1);
    while(!pos->isTerminal()) {
        cout << "*** Step " << ++i << endl;
        cout << *pos << endl;
        
        boost::this_thread::sleep_for(boost::chrono::seconds(1));
        vector<MovePtr> moves(1, pos->getMove(0, 0));
        cout << *moves[0] << endl;
 
        pos = game->next(pos, moves[0]);
        player->next(moves);
    }
    
    cout << "player" << i << " = " << pos->getGoal(0) << endl;
    
    player->stop();
    delete player;

}

void testMatchPermanentSearch(const char * filename) {
    cout << "Game: " << filename << endl;
//    GdlFactory factory;
//    VectorTermPtr gdlCode = factory.computeGdlCode(filename);
//    Grounder grounder(factory, gdlCode);
//    GamePtr game_p;
//    
//    vector<Player *> players(grounder.getRoles().size());
//    for (size_t i = 0; i < players.size(); ++i) {
//        Propnet propnet(grounder);
//        Circuit circuit(propnet);
//        std::unique_ptr<Reasoner> reasoner(new PropnetReasoner(circuit));
//        std::shared_ptr<Game> game(new Game(std::move(reasoner)));
//        if (i == 0)
//            game_p = game;
//        players[i] = new UctPlayer(game, (RoleId) i);
//    }
//    
//    PositionPtr pos = game_p->getInitPos();
//    
//    for(RoleId i = 0; i < players.size(); ++i) {
//        players[i]->start();
//    }
    
    GdlFactory factory;
    VectorTermPtr gdlCode = factory.computeGdlCode(filename);
    Grounder grounder(factory, gdlCode);
    Propnet propnet(grounder);
    Circuit circuit(propnet);
    std::unique_ptr<Reasoner> reasoner(new PropnetReasoner(circuit));
    std::shared_ptr<Game> game(new Game(std::move(reasoner)));
    Player * player = new UctPlayer(game, 0);
    
    PositionPtr pos = game->getInitPos();
    
    player->start();
    
    int i = 0;
    vector<MovePtr> moves(1);
    while(!pos->isTerminal()) {
        cout << "*** Step " << ++i << endl;
        cout << *pos << endl;
        
        moves[0] = player->bestMove(boost::chrono::milliseconds(1 * 1000));
        cout << *moves[0] << endl;
        pos = game->next(pos, moves[0]); // BUUGGG !! même game que dans player et il est pas thread safe
        player->next(moves);
//        cout << *pos << endl;
    }
    
    cout << "player" << i << " = " << pos->getGoal(0) << endl;
    player->stop();
    delete player;
}


int main(int argc, const char * argv[]) {
    //testMatchUct((user_home + "FAC/M2_STAGE/GDL/tictactoe.kif").c_str());
    //testMatchUct((user_home + "FAC/M2_STAGE/GDL/eightpuzzle.kif").c_str());

    //testUctVsRandom((user_home + "FAC/M2_STAGE/GDL/tictactoe.kif").c_str());
    //testUctVsRandom((user_home + "FAC/M2_STAGE/GDL/breakthrough.kif").c_str());
    //testMatchPermanentSearch((user_home + "FAC/M2_STAGE/GDL/tictactoe.kif").c_str());
    //testMatchPermanentSearch((user_home + "FAC/M2_STAGE/GDL/eightpuzzle.kif").c_str());
    //testOnePlayerPermanentSearchPlayFirst((user_home + "FAC/M2_STAGE/GDL/eightpuzzle.kif").c_str());
    
    testFullyExplored((user_home + "FAC/M2_STAGE/GDL/tictactoe.kif").c_str());

}
