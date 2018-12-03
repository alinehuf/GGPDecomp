//
//  Propnet.cpp
//  playerGGP
//
//  Created by Dexter on 01/12/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#include <assert.h>
#include <set>
#include <queue>
#include "Propnet.h"
#include "GdlTools.h"
#include "test_globals_extern.h"


using std::cout;
using std::endl;
using std::set;
using std::map;

/*******************************************************************************
 *      Propnet
 ******************************************************************************/

Propnet::Propnet(Grounder& g) : gdl_factory(g.getFactory()), grounder(g) {
    // TEST -----
    gettimeofday(&time_start, 0); // start timer
    // ----------
    
    map<TermPtr, RoleId> inv_roles = grounder.getInvRoles();
    TermPtr gdl_terminal = gdl_factory.getOrAddTerm("terminal");
    AtomPtr gdl_next = gdl_factory.getOrAddAtom("next");
    AtomPtr gdl_legal = gdl_factory.getOrAddAtom("legal");
    AtomPtr gdl_implied = gdl_factory.getOrAddAtom("<=");
    AtomPtr gdl_not = gdl_factory.getOrAddAtom("not");
    
    // create a gate factory
    gate_factory = std::unique_ptr<GateFactory>(new GateFactory());
    
    // special gates without inputs for true and false
    gate_true = gate_factory->getOrAddGate(Gate::AND, gdl_factory.getOrAddTerm("always true"), SetGatePtr());
    gate_false = gate_factory->getOrAddGate(Gate::OR, gdl_factory.getOrAddTerm("always false"), SetGatePtr());
    
    map<TermPtr, GatePtr> term2gate; // storage for the gates created (rule to gate body)
    map<TermPtr, SetGatePtr> disj; // storage for the implicit disjunctions (conclusion to body)
    // for each instanciated rule,
    // create FACT gates for each premises, NOT gates for the negated terms
    // and an AND gate to bring premises together (rule body)
    for(TermPtr r : grounder.getRules()) {
        // conclusion without premisses (exemple : incredible)
        if(r->getFunctor() != gdl_implied)
            r = gdl_factory.getOrAddTerm(gdl_implied, VectorTermPtr(1, r));
        SetGatePtr prem;
        for(auto arg = r->getArgs().cbegin() + 1; arg != r->getArgs().cend(); ++arg) {
            GatePtr gate;
            auto it_gate = term2gate.find(*arg);
            if(it_gate != term2gate.end()) {
                gate = it_gate->second;
            } else {
                bool negation = (*arg)->getFunctor() == gdl_not && (*arg)->getArgs().size() == 1;
                TermPtr t = negation ? (*arg)->getArgs().front() : *arg;
                auto it_neg = negation ? term2gate.find(t) : it_gate;
                if(it_neg != term2gate.end())
                    gate = it_neg->second;
                else {
                    gate = gate_factory->getLatch(t);
                    term2gate[t] = gate;
                }
                if(negation) {
                    gate = gate_factory->getNot(gate);
                    term2gate[*arg] = gate;
                }
            }
            prem.insert(gate);
        }
        GatePtr rule_body = gate_factory->getAnd(std::move(prem));
        disj[r->getArgs().front()].insert(rule_body);
        //term2gate[r] = rule_body;
    }
    
    // replace implicit disjunction from gdl by OR gates.
    // and there is some FACT gates in the premises of other rules that
    // coincide with this conclusion. let's replace them by this OR gate.
    for(std::pair<TermPtr, SetGatePtr> d : disj) {
        // prepare the inputs of a disjunction
        SetGatePtr inputs;
        for(GatePtr g : d.second) {
            while(g->getType() == Gate::NONE) // gate already replaced
                g = *g->getInputs().begin();  // get the replacement
            inputs.insert(g);
        }
        // is there a premise which is the conclusion of this rule_body ?
        auto it_gate = term2gate.find(d.first);
        if(it_gate != term2gate.end()) {
            //cout << "----------------------" << endl;
            gate_factory->replace(it_gate->second, Gate::OR, nullptr, std::move(inputs));
        }
        else
            term2gate[d.first] = gate_factory->getOr(std::move(inputs));
    }

    
    // get a wire for terminal
    wire_terminal = gate_factory->getWire(gdl_terminal, term2gate.at(gdl_terminal));

    // get a wire for each next
    for(TermPtr t : grounder.getInits()) { // a next wire for each init
        TermPtr next = gdl_factory.getOrAddTerm(gdl_next, VectorTermPtr(1, t));
        auto it_gate = term2gate.find(next);
        if(it_gate == term2gate.end()) {
            cout << *next << " always false" << endl;
            wires_next.push_back(gate_factory->getWire(next, gate_false));
        }
    }
    for(TermPtr t : grounder.getBases()) { // a next wire for each base
        TermPtr next = gdl_factory.getOrAddTerm(gdl_next, VectorTermPtr(t->getArgs()));
        auto it_gate = term2gate.find(next);
        if(it_gate != term2gate.end())
            wires_next.push_back(gate_factory->getWire(it_gate->first, it_gate->second));
    }
    
    // get a wire for each legal
    wires_legal.resize(inv_roles.size());
    for(VectorTermPtr v : grounder.getInputs()) { // a legal wire for each input
        for(TermPtr t : v) {
            TermPtr legal = gdl_factory.getOrAddTerm(gdl_legal, VectorTermPtr(t->getArgs()));
            auto it_gate = term2gate.find(legal);
            if(it_gate != term2gate.end()) {
                auto index = inv_roles.at(legal->getArgs().front());
                //auto index = inv_roles.at(legal->getArgs().front()->getArgs().back());
                wires_legal[index].push_back(gate_factory->getWire(it_gate->first, it_gate->second));
            }
        }
    }
    
    // get a wire for each goal
    wires_goal.resize(inv_roles.size());
    for(VectorTermPtr v : grounder.getGoals()) { // a goal wire for each goal
        for(TermPtr t : v) {
            auto it_gate = term2gate.find(t);
            if(it_gate != term2gate.end()) {
                auto index = inv_roles.at(t->getArgs().front());
                //auto index = inv_roles.at(t->getArgs().front()->getArgs().back());
                wires_goal[index].push_back(gate_factory->getWire(it_gate->first, it_gate->second));
            }
        }
    }

    // TEST -----
    gettimeofday(&time_end, 0); // get current time
    out_time << "Create propnet in                             ";
    out_time << TIME_SPENT(time_start, time_end) << " seconds" << endl;
    // ----------
    
    // TEST -----
    gettimeofday(&time_start, 0); // start timer
    // ----------
    
    // merge, clean, factorize, develop and clean again
    // TEST -----
    out_time << "Propnet size                                  " << gate_factory->gatesSetSize() << endl;
    // ----------
    cout << "Propnet: " << gate_factory->gatesSetSize() << " (" << gate_factory->trashSize() << ")" << endl;
    gate_factory->clean();
    cout << "Propnet after clean: " << gate_factory->gatesSetSize() << " (" << gate_factory->trashSize() << ")" << endl;
#if PLAYERGGP_DEBUG == 2
    gate_factory->createGraphvizFile("propnet");
#endif
//    gate_factory->merge();
//    cout << "Propnet after merge: " << gate_factory->gatesSetSize() << " (" << gate_factory->trashSize() << ")" << endl;
//    gate_factory->clean();
//    cout << "Propnet after clean: " << gate_factory->gatesSetSize() << " (" << gate_factory->trashSize() << ")" << endl;

//    gate_factory->createGraphvizFile("propnet_merged");
    
//    /* test what happens when negations are moved to the inputs */
//    while(gate_factory->negationsAtInputs());
//    cout << "Propnet after negationsAtInputs: " << gate_factory->gatesSetSize() << " (" << gate_factory->trashSize() << ")" << endl;
//    //gate_factory->createGraphvizFile("propnet_negationsAtInputs");
//    /* test what happens when negations are moved to the inputs */
    
    while(gate_factory->factorizeNegations() || gate_factory->factorize());
    //while(gate_factory->factorize());
    // TEST -----
    out_time << "Propnet factorized                            " << gate_factory->gatesSetSize() << endl;
    // ----------
    cout << "Propnet after factorize: " << gate_factory->gatesSetSize() << " (" << gate_factory->trashSize() << ")" << endl;
    gate_factory->clean();
    cout << "Propnet after clean: " << gate_factory->gatesSetSize() << " (" << gate_factory->trashSize() << ")" << endl;
#if PLAYERGGP_DEBUG == 2
    gate_factory->createGraphvizFile("propnet_factorized");
#endif
    gate_factory->twoInputs();
    cout << "Propnet after twoInputs: " << gate_factory->gatesSetSize() << " (" << gate_factory->trashSize() << ")" << endl;
    gate_factory->clean();
    cout << "Propnet after clean: " << gate_factory->gatesSetSize() << " (" << gate_factory->trashSize() << ")" << endl;
#if PLAYERGGP_DEBUG == 2
    gate_factory->createGraphvizFile("circuit");
#endif
    // TEST -----
    gettimeofday(&time_end, 0); // get current time
    out_time << "Propnet factorized in                         ";
    out_time << TIME_SPENT(time_start, time_end) << " seconds" << endl;

    out_time << "Circuit size                                  " << gate_factory->gatesSetSize() << endl;
    // ----------
}
