//
//  PropnetReasoner.h
//  playerGGP
//
//  Created by Dexter on 17/03/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#ifndef __playerGGP__PropnetReasoner__
#define __playerGGP__PropnetReasoner__

#include "Reasoner.h"

class PropnetReasoner : public Reasoner {
  public:
    PropnetReasoner(Circuit& circuit);
    //PropnetReasoner(const PropnetReasoner& prop_res);
    ~PropnetReasoner() { std::cout << "Destroy PropnetReasoner" << std::endl; }

    //Reasoner* copy() const;
    
    GdlFactory& getGdlFactory() const { return circuit.getGdlFactory(); }
    const VectorTermPtr& getRoles() const;
    const VectorTermPtr& getInitPos() const;
    void setPosition(const VectorTermPtr& pos);
    const VectorTermPtr& getPosition() const;
    bool isTerminal() const;
    const VectorTermPtr& getLegals(TermPtr role) const;
    void setMove(TermPtr move);
    void next();
    void playout();
    Score getGoal(TermPtr role) const;
  private:
    Circuit& circuit;
    VectorBool values;
};

#endif /* defined(__playerGGP__PropnetReasoner__) */