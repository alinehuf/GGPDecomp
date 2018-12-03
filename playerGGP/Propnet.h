//
//  Propnet.h
//  playerGGP
//
//  Created by Dexter on 01/12/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef __playerGGP__Propnet__
#define __playerGGP__Propnet__

#include "GdlFactory.h"
#include "Grounder.h"
#include "GateFactory.h"

class Propnet {
  private:
    GdlFactory& gdl_factory;
    Grounder& grounder;
    std::unique_ptr<GateFactory> gate_factory; // contents the real propnet circuit
    
    GatePtr gate_true;
    GatePtr gate_false;
    GatePtr wire_terminal;
    std::vector<VectorGatePtr> wires_legal;
    std::vector<VectorGatePtr> wires_goal;
    VectorGatePtr wires_next;
    //VectorGatePtr wires_true;
    //VectorGatePtr wires_subexpr;
    
  public:
    Propnet(Grounder& grounder);
    ~Propnet() {};
    
    GdlFactory& getGdlFactory() const { return gdl_factory; }
    const Grounder& getGrounder() const { return grounder; }
    const GatePtr& getWireTerminal() const { return wire_terminal; }
    const std::vector<VectorGatePtr>& getWiresLegal() const { return wires_legal; }
    const std::vector<VectorGatePtr>& getWiresGoal() const { return wires_goal; }
    const VectorGatePtr& getWiresNext() const { return wires_next; }
    //const VectorGatePtr& getWiresSubExpr() const { return wires_subexpr; }

    friend class Splitter5;
};


#endif /* defined(__playerGGP__Propnet__) */
