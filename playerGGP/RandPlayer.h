//
//  RandPlayer.h
//  playerGGP
//
//  Created by Dexter on 14/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#ifndef __playerGGP__RandPlayer__
#define __playerGGP__RandPlayer__

#include "Player.h"

class RandPlayer : public Player {
    
  public:
    RandPlayer(GamePtr g, RoleId pid);
    ~RandPlayer();
    
  private:
    MovePtr exploreOnce(PositionPtr position);
    MovePtr exploreNTimes(PositionPtr position, int descents);
    std::default_random_engine rand_gen;
};

#endif /* defined(__playerGGP__RandPlayer__) */
