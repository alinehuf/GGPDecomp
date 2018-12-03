//
//  Player.cpp
//  playerGGP
//
//  Created by Dexter on 12/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#include "Player.h"

using std::vector;
using std::cout;
using std::endl;

/*******************************************************************************
 *      Player : management of the permanent game tree exploration thread
 ******************************************************************************/

void Player::start() {
    position = game->getInitPos();
    fully_explored = false;
    die = false;
    if(thr.joinable())
        throw 1;
    thr = boost::thread(&Player::explorationThread, this);
    std::cout << "PLAYER " << myid << " THREAD START" << std::endl;
}

void Player::explorationThread() {
    while(!die && !fully_explored) {
        boost::unique_lock< boost::mutex > lock(mtx);
        if(!go)
            cond_go.wait(lock);
        if(position->isTerminal())
            move = nullptr;
        else
            move = exploreOnce(position);
    }
}

void Player::stop() {
    die = true;
    if(thr.joinable())
        thr.join();
    std::cout << "PLAYER " << myid << " THREAD STOP" << std::endl;
}

/*******************************************************************************
 *      Player : change root of tree search or ask for the best move
 ******************************************************************************/

void Player::next(const vector<MovePtr>& moves) {
    assert(moves.size() == game->getNbRoles());
    {
        go = false;
        boost::unique_lock< boost::mutex > lock(mtx);
        for(MovePtr m : moves)
            position = game->next(position, m);
    }
    go = true;
    cond_go.notify_all();
}

void Player::setFullyExplored() {
    fully_explored = true;
}

MovePtr Player::bestMove(const boost::chrono::milliseconds& search_time) {
    if(fully_explored && !position->isTerminal()) {
        cout << "player fully_explored" << endl;
        return exploreOnce(position);
    } else
        boost::this_thread::sleep_for(search_time);
    return move;
}
