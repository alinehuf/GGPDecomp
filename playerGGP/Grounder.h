//
//  Grounder.h
//  playerGGP
//
//  Created by Dexter on 18/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef __playerGGP__Grounder__
#define __playerGGP__Grounder__

#include "GdlFactory.h"
#include "GdlTools.h"
#include "YapTranslator.h"

typedef unsigned int RoleId;

//! This class allows to instantiate a GDL description.
class Grounder {
  public:
    Grounder(GdlFactory& factory, const VectorTermPtr& gdl);
    ~Grounder() {};
    // accessors
    const VectorTermPtr& getRoles() const { return roles; }
    const std::map<TermPtr, RoleId>& getInvRoles() const { return inv_roles; }
    const VectorTermPtr& getInits() const {return inits; }
    const std::vector<VectorTermPtr>& getInputs() const { return inputs; }
    const VectorTermPtr& getBases() const { return bases; }
    const std::vector<VectorTermPtr>& getGoals() const { return goals; }
    const VectorTermPtr& getRules() const { return rules; }
    GdlFactory& getFactory() const { return factory; }
    
    struct Assertz;
    struct Table;
    
  private:
    GdlFactory& factory;                       // a factory : string -> Term
    std::unique_ptr<YapTranslator> translator; // translator Term -> Yap::Term
    std::unique_ptr<GdlTools> gdlTools;   // a tool to part static from dynamic and transform gdl
    std::unique_ptr<Yap> yap;
    
    //Yap::Term doAssertz(TermPtr t);
    //void doTable(TermPtr t);
    
    VectorTermPtr roles;
    std::map<TermPtr, RoleId> inv_roles;
    VectorTermPtr inits;
    std::vector<VectorTermPtr> inputs;
    VectorTermPtr bases;
    std::vector<VectorTermPtr> goals;
    VectorTermPtr rules;
    
    static constexpr const char* grounding_predicates =
#include "Grounder.pred"
    ;
    
    //VectorTermPtr decomposeClauses(VectorTermPtr original_gdl);
};

/*******************************************************************************
 *      Convenient : to ask tabling of a predicate or to assert a clause
 ******************************************************************************/

struct Grounder::Assertz {
    Assertz(Yap& yap, YapTranslator& translator) : yap_(yap), translator_(translator) {
        assertz = yap_.MkFunctor(yap_.LookupAtom(str_assertz), 1);
    }
    void operator()(TermPtr t) {
        Yap::Term yap_term = translator_.gdlToYap(t);
        if(!yap_.RunGoalOnce(yap_.MkApplTerm(assertz, 1, &yap_term)))
        std::cerr << "Cannot assert " << *t << std::endl;
    }
    Yap& yap_;
    YapTranslator& translator_;
    Yap::Functor assertz;
    static constexpr const char* str_assertz = "assertz_static";
};

struct Grounder::Table {
    Table(Yap& yap, GdlFactory& factory, YapTranslator& translator) : yap_(yap), factory_(factory), translator_(translator) {
        gdl_table = factory.getOrAddAtom("'table");
        gdl_slash = factory.getOrAddAtom("'/");
    }
    void operator()(TermPtr t) {
        VectorTermPtr args;
        args.push_back(factory_.getOrAddTerm(t->getFunctor()));
        args.push_back(factory_.getOrAddTerm('#' + std::to_string(t->getArgs().size())));
        VectorTermPtr args_with_slash(1, factory_.getOrAddTerm(gdl_slash, std::move(args)));
        TermPtr table = factory_.getOrAddTerm(gdl_table, std::move(args_with_slash));
        if(!yap_.RunGoalOnce(translator_.gdlToYap(table)))
        std::cerr << "Cannot table " << *t << std::endl;
    }
    Yap& yap_;
    GdlFactory& factory_;
    YapTranslator& translator_;
    AtomPtr gdl_table;
    AtomPtr gdl_slash;
    static constexpr const char* str_table = "table";
    static constexpr const char* str_slash = "'/";
    
};


#endif /* defined(__playerGGP__Grounder__) */
