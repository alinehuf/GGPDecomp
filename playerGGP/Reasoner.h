//
//  Reasoner.h
//  playerGGP
//
//  Created by Dexter on 17/03/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#ifndef playerGGP_Reasoner_h
#define playerGGP_Reasoner_h

#include "Circuit.h"

template <class R>
struct GetReasoner {};

//! Interface to be followed by any reasoner
class Reasoner {
public:
    virtual ~Reasoner() { std::cout << "Destroy Reasoner" << std::endl; }
    
    //virtual Reasoner* copy() const = 0;
    //virtual bool construct() = 0;
    
    virtual GdlFactory& getGdlFactory() const = 0;
    virtual const VectorTermPtr& getRoles() const = 0;
    virtual const VectorTermPtr& getInitPos() const = 0;
    virtual void setPosition(const VectorTermPtr& pos) = 0;
    virtual const VectorTermPtr& getPosition() const = 0;
    virtual bool isTerminal() const = 0;
    virtual const VectorTermPtr& getLegals(TermPtr role) const = 0;
    virtual void setMove(TermPtr move) = 0;
    virtual void next() = 0;
    virtual void playout() = 0;
    virtual Score getGoal(TermPtr role) const = 0;
    //virtual void interrupt() = 0;
};

#endif
