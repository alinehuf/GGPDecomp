//
//  Match.h
//  playerGGP
//
//  Created by Dexter on 14/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#ifndef __playerGGP__Match__
#define __playerGGP__Match__

#include "player.h"

class Match {
  public:
    Match(GamePtr g,  std::vector<Player*> p) : game(g), players(p) {}
    ~Match() {};
    std::vector<Score> play_with_time(int play_time);
    std::vector<Score> play_with_iter(int iter);
    
  protected:
    GamePtr game;
    std::vector<Player*> players;
    int init_time;
    int play_time;
    int iterations;
};

#endif /* defined(__playerGGP__Match__) */
