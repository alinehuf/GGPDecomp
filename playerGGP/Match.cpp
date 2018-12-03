//
//  Match.cpp
//  playerGGP
//
//  Created by Dexter on 14/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#include "Match.h"

using std::cout;
using std::endl;
using std::vector;

vector<Score> Match::play_with_time(int play_time) {
    cout << *game << endl;
    
    PositionPtr pos = game->getInitPos();
    
    int i = 0;
    vector<MovePtr> moves(game->getNbRoles());
    while(!pos->isTerminal()) {
        cout << "*** Step " << ++i << endl;
        cout << *pos << endl;
        
        for(size_t pl = 0; pl < game->getNbRoles(); ++pl) {
            boost::posix_time::ptime until(boost::posix_time::microsec_clock::universal_time() + boost::posix_time::millisec(play_time * 1000));
            while(!players[pl]->fully_explored && boost::posix_time::microsec_clock::universal_time() < until)
                moves[pl] = players[pl]->exploreOnce(pos);
            if(players[pl]->fully_explored && !pos->isTerminal())
                moves[pl] = players[pl]->exploreOnce(pos);
        }
        for(size_t pl = 0; pl < game->getNbRoles(); ++pl) {
            cout << *moves[pl] << endl;
            pos = game->next(pos, moves[pl]);
            players[pl]->next(moves);
//            cout << *pos << endl;
        }
    }
    
    vector<Score> v;
    for(size_t i = 0; i < players.size(); ++i)
        v.push_back(pos->getGoal((RoleId) i));
    return v;
}

vector<Score> Match::play_with_iter(int iter) {
    cout << *game << endl;
    
    PositionPtr pos = game->getInitPos();
    
    int i = 0;
    vector<MovePtr> moves(game->getNbRoles());
    while(!pos->isTerminal()) {
        cout << "*** Step " << ++i << endl;
        cout << *pos << endl;
        
        for(size_t pl = 0; pl < game->getNbRoles(); ++pl) {
            moves[pl] = players[pl]->exploreNTimes(pos, iter);
            if(players[pl]->fully_explored && !pos->isTerminal())
                moves[pl] = players[pl]->exploreOnce(pos);
        }
        for(size_t pl = 0; pl < game->getNbRoles(); ++pl) {
            cout << *moves[pl] << endl;
            pos = game->next(pos, moves[pl]);
            players[pl]->next(moves);
//            cout << *pos << endl;
        }
    }
    
    vector<Score> v;
    for(size_t i = 0; i < players.size(); ++i)
        v.push_back(pos->getGoal((RoleId) i));
    return v;
}

