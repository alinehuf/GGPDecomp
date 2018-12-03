//
//  GateFactory.h
//  playerGGP
//
//  Created by Dexter on 01/12/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef __playerGGP__GateFactory__
#define __playerGGP__GateFactory__

#include <unordered_set>
#include <stack>
#include "Gate.h"

class GateFactory { // Logic Gate Factory
  public:
    GateFactory() {};
    ~GateFactory();
    GatePtr getLatch(TermPtr t);
    GatePtr getWire(TermPtr t, GatePtr g);
    GatePtr getAnd(SetGatePtr&& in);
    GatePtr getOr(SetGatePtr&& in);
    GatePtr getNot(GatePtr g);
    
    GatePtr getOrAddGate(Gate::Type ty, SetGatePtr&& in);
    GatePtr getOrAddGate(Gate::Type ty, TermPtr t, SetGatePtr&& in);
    
    void replace(GatePtr tochange, Gate::Type type, TermPtr term, SetGatePtr&& inputs);
    
    bool clean();
    bool merge();
    bool factorizeNegations();
    bool factorize();
    bool twoInputs();
    
    /* debug & test */
    bool negationsAtInputs();
    
    size_t gatesSetSize() const { return gatesSet.size(); }
    size_t trashSize() const { return trash.size(); }
    
    // for debug
    size_t countInputs() const;
    size_t countOutputs() const;
    void printGatesSet() const ;
    void createGraphvizFile(const std::string& name) const;
    std::string graphvizRepr(const std::string& name) const;
    std::string graphvizRepr(GatePtr gate) const;
    std::string graphvizId(GatePtr gate) const;
    
  private:
    void emptyTrash();
    bool clean(GatePtr gate);
    
    bool merge(GatePtr gate, // to merge
               Gate::Type& type, TermPtr& term, SetGatePtr& inputs, // merged
               SetGatePtr& to_clean);
    bool factorizeNegations(GatePtr gate, Gate::Type& type, TermPtr& term,
                            SetGatePtr& inputs, SetGatePtr& to_clean, SetGatePtr& to_merge);
    bool factorize(GatePtr gate, Gate::Type& type, TermPtr& term,
                   SetGatePtr& inputs, SetGatePtr& to_clean);
    bool twoInputs(GatePtr gate, Gate::Type& new_type, TermPtr& term,
                   SetGatePtr& inputs, SetGatePtr& to_clean);
    
    /* debug & test */
    bool negationsAtInputs(GatePtr gate, Gate::Type& type, TermPtr& term,
                            SetGatePtr& inputs, SetGatePtr& to_clean, SetGatePtr& to_merge);
    
    struct Equal {
        inline bool operator() (const GatePtr x, const GatePtr y) const { return (*x == *y); }
    };
    struct Hash {
        inline size_t operator() (const GatePtr x) const { return x->getHash(); }
    };
    std::unordered_set<GatePtr, Hash, Equal> gatesSet; // to store every single Gate
    std::stack<GatePtr> trash;
    
    //debug
    bool twoSimilarGates(std::unordered_set<GatePtr, Hash, Equal> gateSet);
};


#endif /* defined(__playerGGP__GateFactory__) */
