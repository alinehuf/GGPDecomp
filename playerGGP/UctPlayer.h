//
//  UctPlayer.h
//  playerGGP
//
//  Created by Dexter on 12/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#ifndef __playerGGP__UctPlayer__
#define __playerGGP__UctPlayer__

#include "Player.h"

class UctPlayer : public Player {
  public:
    struct UctNode;
    typedef std::shared_ptr<UctNode> UctNodePtr;
    
    UctPlayer(GamePtr game, RoleId pid, float uct_const = 0.4f);
    ~UctPlayer() {}
    
private:
    UctNodePtr root = nullptr;
    typedef std::unordered_map<PositionPtr, UctNodePtr, Position::Hash, Position::Equal> NodeCache;
    NodeCache cache;
    float uct_const;
    std::default_random_engine rand_gen;
    
    MovePtr getNextMove(UctNodePtr root);
    MovePtr exploreOnce(PositionPtr position);
    MovePtr exploreNTimes(PositionPtr position, int descents);
    void uct(UctNodePtr root);
    int selection(UctNodePtr root);

};


#endif /* defined(__playerGGP__UctPlayer__) */
