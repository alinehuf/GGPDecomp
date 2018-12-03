//
//  Grounder.cpp
//  playerGGP
//
//  Created by Dexter on 18/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#include "Grounder.h"
#include "IBFinder.h"
#include "YapData.h"
#include <fstream>
#include <cassert>
#include "test_globals_extern.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

/*******************************************************************************
 *      Grounder
 ******************************************************************************/

Grounder::Grounder(GdlFactory& fact, const VectorTermPtr& original_gdl) : factory(fact) {
    
    // TEST -----
    gettimeofday(&time_start, 0); // start timer
    // ----------
    
    // part dynamic from static terms
    gdlTools = std::unique_ptr<GdlTools>(new GdlTools(factory, original_gdl));

    
    
    // decomposition of actions
//    ActionSplitter a_splitter(*gdlTools, factory, original_gdl);
//    VectorTermPtr gdl = original_gdl;
//    if (a_splitter.somethingToSplit())
//        gdl = a_splitter.rewriteGdl();
//    
//    ActionSplitter2 a_splitter(factory, *gdlTools, original_gdl);
    VectorTermPtr gdl = original_gdl;
    
    
    
    
    // start yap
    YapData data;                 // data storage
    yap = std::unique_ptr<Yap>(Yap::GetYap(&data));
    if (yap->FastInit(nullptr) == YAP_BOOT_ERROR)
        throw std::exception();
    
    // prepare a translator
    translator = std::unique_ptr<YapTranslator>(new YapTranslator(*yap, factory));
    data.init(*yap, *translator);
    Assertz doAssertz(*yap, *translator);
    Table doTable(*yap, factory, *translator);
    
    AtomPtr gdl_role = factory.getOrAddAtom("role");
    AtomPtr gdl_input = factory.getOrAddAtom("input");
    AtomPtr gdl_base = factory.getOrAddAtom("base");
    AtomPtr gdl_storeground = factory.getOrAddAtom("'storeground");
    TermPtr gdl_var = factory.getOrAddTerm("?__var");
    TermPtr gdl_storeground_and_var = factory.getOrAddTerm(gdl_storeground, VectorTermPtr(1, gdl_var));
    TermPtr gdl_groundnot = factory.computeGdlCode(string("('groundnot ?x)")).front();
    
    // prepare grounding
    VectorTermPtr to_assert;
    std::set<TermPtr> to_table;
    int count = 0;
    for(TermPtr t : gdl) {
        if(gdlTools->isStatic(t)) {  // static term
            TermPtr concl = gdlTools->getConclusion(t);
            if(concl->getFunctor() == gdl_input && concl->getArgs().size() == 2)
                continue;           // ignore input
            if(concl->getFunctor() == gdl_base && concl->getArgs().size() == 1)
                continue;           // ignore base
            t = gdlTools->checkVars(t);   // can be fully instanciated ?
            TermPtr with_dummy = gdlTools->addDummy(t); // add dummy to force instanciation
            to_assert.push_back(with_dummy);
            to_table.insert(gdlTools->getFunctorWithVars(gdlTools->getConclusion(with_dummy)));
            //cout << "static: " << *with_dummy << endl;
            if(t->getFunctor() == gdl_role && t->getArgs().size() == 1) {
                roles.push_back(t->getArgs().back());
                cout << *t << endl;          // print role
            }
        } else {                    // static term
            VectorTermPtr no_or = gdlTools->removeOr(t);
            for(TermPtr no_dummy : no_or) {
                no_dummy = gdlTools->checkVars(no_dummy); // can be fully instanciated ?
                /* create two rules for grounding
                 * rule A = conclusion : new conclusion without not + dummy + original rule without statics
                 *          premises : original premises without not
                 *
                 * rule B = conclusion : original conclusion without not + dummy
                 *          premise1 : new conclusion without not + dummy + ?var (to unify with instanciated original rule)
                 *          premise2 : side effect (store ?var)
                 */
                
                TermPtr no_static = gdlTools->removeStatic(no_dummy);  // original rule without statics
                
                //VectorTermPtr decomposed = gdlTools->decomposeClauses(no_dummy); // original rule without statics and decomposed
                //for (TermPtr& rule : decomposed) {
                //    TermPtr no_static = gdlTools->removeStatic(gdlTools->addWildcard(rule)); // original rule without statics, decomposed and with wildcards
                    
                    TermPtr no_not = gdlTools->addDummy(gdlTools->removeNot(no_dummy)); // original rule without not + dummy
                    TermPtr concl = gdlTools->getConclusion(no_not);                    // original conclusion without not + dummy
                    AtomPtr new_concl_functor = factory.getOrAddAtom(concl->getFunctor()->getName() + ' ' + std::to_string(count++));
                    VectorTermPtr new_concl_args = concl->getArgs();
                    TermPtr new_concl = factory.getOrAddTerm(new_concl_functor, std::move(new_concl_args)); // new conclusion without not + dummy
                    TermPtr concl_with_se = gdlTools->addArg(new_concl, no_static);    // new conclusion without not + dummy + original rule without statics
                    TermPtr concl_call_se = gdlTools->addArg(new_concl, gdl_var);      // new conclusion without not + dummy + ?var
                    TermPtr with_se = gdlTools->changeConclusion(no_not, concl_with_se);                                              // rule A
                    //cout << "with_se: " << *with_se << endl;
                    TermPtr call_se = gdlTools->addArg(gdlTools->makeRuleWithPremise(concl, concl_call_se), gdl_storeground_and_var); // rule B
                    //cout << "call_se: " << *call_se << endl;
                    to_assert.push_back(call_se);
                    to_assert.push_back(with_se);
                    to_table.insert(gdlTools->getFunctorWithVars(gdlTools->getConclusion(call_se)));
                //}
            }
        }
    }
    
    // ask table and assertz
    for(TermPtr t : to_table)
        doTable(t);
    doTable(gdl_groundnot);
    for(TermPtr clause : to_assert)
        doAssertz(clause);
    
    // get inputs and bases
    IBFinder input_base_finder(factory, gdl, *gdlTools);
    VectorTermPtr all_inputs = input_base_finder.getAllInputs();
    bases = input_base_finder.getbases();
    
    // assert inputs and bases
    for (TermPtr clause : all_inputs)
        doAssertz(clause);
    for (TermPtr clause : bases)
        doAssertz(clause);
    
    // get id from roles TermPtr
    for(size_t i = 0; i < roles.size(); ++i)
        inv_roles[roles[i]] = (RoleId) i;

    // sort inputs for each player
    inputs.resize(roles.size());
    for(TermPtr t : all_inputs) {
        inputs[inv_roles.at(t->getArgs().front())].push_back(t);
    }
    
    // add special clauses for grounding
    VectorTermPtr ground_prog = factory.computeGdlCode(string(grounding_predicates));
    for(TermPtr clause : ground_prog) {
        doAssertz(clause);
    }
    
    // get init terms
    if(!yap->RunGoalOnce(translator->gdlToYap(factory.getOrAddTerm("'init"))))
        cerr << "Error init" << endl;
    inits = data.storeset->getData();
//    for(TermPtr t : inits) {
//        std::cout << *t << std::endl;
//    }
    
    // get goals terms
    if(!yap->RunGoalOnce(translator->gdlToYap(factory.getOrAddTerm("'goals"))))
        cerr << "Error goals" << endl;
    // sort goals for each player
    VectorTermPtr all_goals =  data.storeset->getData();
    goals.resize(roles.size());
    for(TermPtr t : all_goals) {
        goals[inv_roles.at(t->getArgs().front())].push_back(t);
        //std::cout << *t << std::endl;
    }
    
    // ground all the rules
    if(!yap->RunGoalOnce(translator->gdlToYap(factory.getOrAddTerm("'groundall"))))
        cerr << "Error grounding" << endl;
    rules = data.storeground->getData();
    cout << "Grounded: " << rules.size() << endl;
    
    // TEST -----
    gettimeofday(&time_end, 0); // get current time
    out_time << "Grounding in                                  ";
    out_time << TIME_SPENT(time_start, time_end) << " seconds" << endl;
    // ----------
    
#if PLAYERGGP_DEBUG == 2
        std::ofstream out(log_dir + "grounded.kif", std::ios::out);
        for(TermPtr t : rules) {
            out << *t << std::endl;
            //cout << *t << std::endl;
        }
#endif

    
//    cout << "yap listing  -------------" << endl;
//    yap->RunGoalOnce(yap->MkAtomTerm(yap->LookupAtom("listing")));
}

