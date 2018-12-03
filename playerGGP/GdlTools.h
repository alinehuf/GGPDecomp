//
//  GdlTools.h
//  playerGGP
//
//  Created by Dexter on 22/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef __playerGGP__GdlTools__
#define __playerGGP__GdlTools__

#include <stdio.h>
#include <set>
#include "GdlFactory.h"

class GdlTools {
  private:
    GdlFactory& factory;
    VectorTermPtr gdlCode;
    SetTermPtr all_preds;
    SetTermPtr static_preds;
    SetTermPtr dynamic_preds;
    AtomPtr gdl_distinct;
    AtomPtr gdl_implied;
    AtomPtr gdl_or;
    AtomPtr gdl_not;
    AtomPtr gdl_ground_not;
    TermPtr gdl_dummy;
    AtomPtr gdl_wildcard;
    
  public:
    GdlTools(GdlFactory& factory, VectorTermPtr gdl);
    
    void partDynamicsFromStatics(const VectorTermPtr& gdl);
    bool isStatic(TermPtr t);
    const SetTermPtr& getAllPreds() {
        return all_preds;
    }
    
    TermPtr getConclusion(TermPtr t);
    TermPtr getFunctorWithVars(TermPtr t);
    SetTermPtr getPremises(TermPtr t);
    void extractTerms(TermPtr t, SetTermPtr& extracted);
    //void getArgs(TermPtr t, SetTermPtr& args);
    
    TermPtr checkVars(TermPtr t);
    bool checkVars(TermPtr t, SetTermPtr& vars);
    void getVars(TermPtr t, SetTermPtr& vars);
    TermPtr tryToRepair(TermPtr t);
    bool noNotOrDistinct(TermPtr t);
    bool insertNoNotOrDistinct(VectorTermPtr& args, VectorTermPtr::iterator arg);
    
    TermPtr addDummy(TermPtr t);
    VectorTermPtr removeOr(TermPtr t);
    void removeOr(TermPtr t, VectorTermPtr& l);
    TermPtr removeNot(TermPtr t);
    TermPtr replaceNot(TermPtr t);
    bool containsNot(TermPtr t);
    TermPtr removeStatic(TermPtr t);
    TermPtr addArg(TermPtr t, TermPtr arg);
    TermPtr changeConclusion(TermPtr t, TermPtr concl);
    TermPtr makeRuleWithPremise(TermPtr t, TermPtr prem);
    
    
    //TermPtr replaceWildcard(TermPtr t);
    TermPtr addWildcard(TermPtr t);
    TermPtr replaceVarsByWilcard(TermPtr t, std::set<AtomPtr> vars);
    void getVars(TermPtr t, std::map<TermPtr, int>& vars);
    
    //TermPtr keepOnlyVars(TermPtr t, std::set<AtomPtr> vars);
    VectorTermPtr decomposeClauses(TermPtr clause);
    
};

#endif /* defined(__playerGGP__GdlTools__) */
