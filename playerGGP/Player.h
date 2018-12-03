//
//  Player.h
//  playerGGP
//
//  Created by Dexter on 12/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#ifndef __playerGGP__Player__
#define __playerGGP__Player__

#include <boost/thread.hpp>
#include "Game.h"

class Player {
  public:
    Player(GamePtr g, RoleId pid): game(g), myid(pid), position(g->getInitPos()) {}
    virtual ~Player() { std::cout << "Destroy Player " << myid << std::endl; }
    
    void start();
    void stop();
    void next(const std::vector<MovePtr>& moves);
    MovePtr bestMove(const boost::chrono::milliseconds& search_time);
    
  protected: // public for classes that inherit this one, private for other class
    GamePtr game;
    RoleId myid;
    
    virtual MovePtr exploreOnce(PositionPtr position) = 0;
    virtual MovePtr exploreNTimes(PositionPtr position, int descents) = 0;
    void setFullyExplored();
    
  private:
    PositionPtr position = nullptr;
    MovePtr move = nullptr;
    
    boost::thread thr;
    boost::mutex mtx;
    boost::condition_variable cond_go;
    bool go = true;
    bool die = true;
    bool fully_explored = false;
    
    void explorationThread();
    
  friend class Match;
};



#endif /* defined(__playerGGP__Player__) */
