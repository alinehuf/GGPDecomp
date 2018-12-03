//
//  IBFinder.cpp
//  playerGGP
//
//  Created by Dexter on 03/12/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#include "IBFinder.h"
#include "Grounder.h"
#include "YapData.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;


/*******************************************************************************
 *      Method to get inputs and bases from GDL
 ******************************************************************************/


IBFinder::IBFinder(GdlFactory& factory, const VectorTermPtr& gdl, GdlTools& gdlTools) : factory(factory) {
    // start yap
    YapData data;                 // data storage
    yap = std::unique_ptr<Yap>(Yap::GetYap(&data));
    if (yap->FastInit(nullptr) == YAP_BOOT_ERROR)
        throw std::exception();
    
    // prepare a tranlator
    translator = std::unique_ptr<YapTranslator>(new YapTranslator(*yap, factory));
    data.init(*yap, *translator);
    Grounder::Assertz doAssertz(*yap, *translator);
    Grounder::Table doTable(*yap, factory, *translator);
    
    
    AtomPtr gdl_role = factory.getOrAddAtom("role");
    const std::set<TermPtr>& all_preds = gdlTools.getAllPreds();
    
    // ask tabling for all predicates
    for(TermPtr t : all_preds) {
        int arity = (int) ( t->getArgs().size());
        if(t->getFunctor() != gdl_role || arity != 1)
            doTable(t);
    }
    
    // assert all clauses (without not : to prevent yap from faillure)
    for(TermPtr clause : gdl) {
        clause = gdlTools.removeNot(gdlTools.checkVars(clause));
        doAssertz(clause);
    }
    // add special clauses for instanciation of inputs and bases
    VectorTermPtr gdl_my = factory.computeGdlCode(string(input_base_predicates));
    for(TermPtr clause : gdl_my)
        doAssertz(clause);

//    // instanciates all inputs and bases
//    yap->RunGoalOnce(translator->gdlToYap(factory.getOrAddTerm("'input")));
//    all_inputs = data.storeset->getData();
//
//    yap->RunGoalOnce(translator->gdlToYap(factory.getOrAddTerm("'base")));
//    bases = data.storeset->getData();
//
//     // if no existing input or base, find them
//     if(all_inputs.empty() || bases.empty()) {
        Yap::Term call_input = translator->gdlToYap(factory.getOrAddTerm("'inputs"));
        if(!yap->RunGoalOnce(call_input))
            cerr << "Error while computing inputs" << endl;
        all_inputs = data.storeset->getData();
        Yap::Term call_base = translator->gdlToYap(factory.getOrAddTerm("'bases"));
        if(!yap->RunGoalOnce(call_base))
            cerr << "Error while computing bases" << endl;
        bases = data.storeset->getData();
//    }
    // print number of inputs and bases found
    cout << "Inputs: " << all_inputs.size() << endl;
    cout << "Bases: " << bases.size() << endl;
   
//    cout << "-------------INPUTS" << endl;
//    for (TermPtr t : all_inputs)
//        cout << *t << endl;
//    cout << "-------------BASES" << endl;
//    for (TermPtr t : bases)
//        cout << *t << endl;
}

