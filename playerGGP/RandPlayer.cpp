//
//  RandPlayer.cpp
//  playerGGP
//
//  Created by Dexter on 14/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#include "RandPlayer.h"

RandPlayer::RandPlayer(GamePtr g, RoleId pid): Player(g, pid), rand_gen((std::random_device()).operator()()) {}

RandPlayer::~RandPlayer() {}

MovePtr RandPlayer::exploreOnce(PositionPtr position) {
    return position->getMove(myid, rand_gen() % position->getNbLegal(myid));
}

MovePtr RandPlayer::exploreNTimes(PositionPtr position, int descents) {
    return exploreOnce(position);
}
