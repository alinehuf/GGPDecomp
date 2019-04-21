//
//  Circuit.cpp
//  playerGGP
//
//  Created by Dexter on 06/12/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#include <assert.h>
#include "Circuit.h"
#include "test_globals_extern.h"
#include <cstring>


using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::set;

/*******************************************************************************
 *      Circuit
 ******************************************************************************/

Circuit::Circuit(Propnet& propnet) : gdl_factory(propnet.getGdlFactory()), rand_gen((std::random_device()).operator()()){
    GdlFactory& factory = propnet.getGdlFactory();
    const Grounder& grounder = propnet.getGrounder();
    
    infos.roles = grounder.getRoles();
    infos.inv_roles = grounder.getInvRoles();
    
    // store init I as "(true I)"
    const VectorTermPtr& inits = grounder.getInits();
    AtomPtr gdl_true = factory.getOrAddAtom("true");
    for (TermPtr t : inits)
        infos.init.push_back(factory.getOrAddTerm(gdl_true, VectorTermPtr(1, t)));
    
    // to store goals and legals while playing
    goals.resize(infos.roles.size());
    legals.resize(infos.roles.size());
    
    // create the list of vars used in the circuits
    infos.vars.resize(2); // for always true and always false
    // get the trues
    infos.id_true = infos.vars.size();
    for (const GatePtr g : propnet.getWiresNext()) {
        TermPtr t = factory.getOrAddTerm(factory.getOrAddAtom("true"), VectorTermPtr(g->getTerm()->getArgs()));
        infos.inv_vars[t] = infos.vars.size();
        infos.vars.push_back(t);
    }
    // get the does
    infos.id_does.push_back(infos.vars.size());
    for (const VectorGatePtr& v : propnet.getWiresLegal()) {
        for(const GatePtr g : v) {
            TermPtr t = factory.getOrAddTerm(factory.getOrAddAtom("does"), VectorTermPtr(g->getTerm()->getArgs()));
            infos.inv_vars[t] = infos.vars.size();
            infos.vars.push_back(t);
        }
        infos.id_does.push_back(infos.vars.size());
    }
    // get terminal
    infos.id_terminal = infos.vars.size();
    TermPtr t = factory.getOrAddTerm("terminal");
    infos.inv_vars[t] = infos.vars.size();
    infos.vars.push_back(t);
    // get the legals
    infos.id_legal.push_back(infos.vars.size());
    for (const VectorGatePtr& v : propnet.getWiresLegal()) {
        for(const GatePtr g : v) {
            TermPtr t = g->getTerm();
            infos.inv_vars[t] = infos.vars.size();
            infos.vars.push_back(t);
        }
        infos.id_legal.push_back(infos.vars.size());
    }
    // get the nexts
    infos.id_next = infos.vars.size();
    for (const GatePtr g : propnet.getWiresNext()) {
        TermPtr t = g->getTerm();
        infos.inv_vars[t] = infos.vars.size();
        infos.vars.push_back(t);
    }
    // get the goals
    infos.id_goal.push_back(infos.vars.size());
    for (const VectorGatePtr& v : propnet.getWiresGoal()) {
        for(const GatePtr g : v) {
            TermPtr t = g->getTerm();
            infos.inv_vars[t] = infos.vars.size();
            infos.vars.push_back(t);
        }
        infos.id_goal.push_back(infos.vars.size());
    }
    
//    assert(infos.vars.size() == infos.inv_vars.size() + 2);
//    // get the subexpressions
//    infos.id_subexpr = infos.vars.size();
//    for (const GatePtr g : propnet.getWiresSubExpr()) {
//        TermPtr t = g->getTerm();
//        if (infos.inv_vars.find(t) == infos.inv_vars.end()) {
//            infos.inv_vars[t] = infos.vars.size();
//            infos.vars.push_back(t);
//        }
////        else
////            cout << *t << endl;
//        infos.subexpr[*g->getInputs().begin()] = t;
//    }
//    
    infos.id_end = infos.vars.size();
    assert(infos.vars.size() == infos.inv_vars.size() + 2);
    
    // prepare the circuits
    std::stack<GatePtr> s;
    // mark the outputs
    propnet.getWireTerminal()->setTypeCirc(Gate::TERM);
    s.push(propnet.getWireTerminal());
    insertAndMark(s, propnet.getWiresLegal(), Gate::LEG);
    insertAndMark(s, propnet.getWiresGoal(), Gate::GL);
    insertAndMark(s, propnet.getWiresNext(), Gate::NXT);
    // sort the gates from inputs to outputs and mark the circuit(s) which uses them
    std::deque<GatePtr> order = sortAndMark(s);    // get id, stratum and circuit of each gate,
    // add additional terms at the end of the values
    infos.values_size = infos.vars.size();
    markCircuitStratumAndId(order);
    cout << "Circuit size: " << order.size() << endl;
    
    // create the circuits
    // et collecte les strates du circuit GOAL
    for (GatePtr g : order) {
        assert(g->getId() < infos.values_size);
        if(g->getId() <= 1 || g->getType() == Gate::FACT) // always_true, always_false or fact
            continue;
        if(g->getStratum() >= (int) infos.circuits[g->getInsideCirc()].size()) // stratums
            infos.circuits[g->getInsideCirc()].resize(g->getStratum() + 1);
        GateId id_concl = g->getId();
        GateId id_arg1 = -1, id_arg2 = -1;
        if(g->getInputs().size() >= 1) {
            GatePtr in = *g->getInputs().begin();
            id_arg1 = in->getId();
        }
        if(g->getInputs().size() >= 2) {
            GatePtr in = *g->getInputs().rbegin();
            id_arg2 = in->getId();
        }
        switch(g->getType()) {
            case Gate::NOT:
                assert(g->getInputs().size() == 1);
                infos.circuits[g->getInsideCirc()][g->getStratum()].v_not.push_back(OpNot{id_concl, id_arg1});
                break;
            case Gate::AND:
                assert(g->getInputs().size() == 2);
                infos.circuits[g->getInsideCirc()][g->getStratum()].v_and.push_back(OpAnd{id_concl, id_arg1, id_arg2});
                break;
            case Gate::OR:
                assert(g->getInputs().size() == 2);
                infos.circuits[g->getInsideCirc()][g->getStratum()].v_or.push_back(OpOr{id_concl, id_arg1, id_arg2});
                break;
            case Gate::WIRE: // OR with only one input
                assert(g->getInputs().size() == 1);
                infos.circuits[g->getInsideCirc()][g->getStratum()].v_or.push_back(OpOr{id_concl, id_arg1, id_arg1});
                break;
            default:
                assert(false);
                break;
        }
    }
}

/*******************************************************************************
 *      Insert output gates into the stack and mark them with their circuit flag
 ******************************************************************************/

void Circuit::insertAndMark(std::stack<GatePtr>& s, const vector<VectorGatePtr>& vgates, Gate::TypeCirc type) {
    for (VectorGatePtr gates : vgates) {
        for (GatePtr g : gates) {
            s.push(g);
            g->setTypeCirc(type);
        }
    }
}
void Circuit::insertAndMark(std::stack<GatePtr>& s, const VectorGatePtr& gates, Gate::TypeCirc type) {
    for (GatePtr g : gates) {
        s.push(g);
        g->setTypeCirc(type);
    }
}

/*******************************************************************************
 *      Sort all the gates, and mark the circuits which use them
 ******************************************************************************/

std::deque<GatePtr> Circuit::sortAndMark(std::stack<GatePtr>& s) {
    std::deque<GatePtr> order;
    std::map<GatePtr, bool> visited;
    while(!s.empty()) {
        GatePtr top = s.top();
        auto it = visited.find(top);
        if(it == visited.end()) {
            visited[top] = true; // set first visit of this node
        } else {
            s.pop();             // whole branch visited
            if(it->second) {     // first visit
                order.push_back(top);  // add in ordered list
                it->second = false;
            }
        }
        // add each input node in stack to compute them before their parent
        for(GatePtr in : top->getInputs()) {
            // first visit of the top node OR typeCirc of input is different from this output (input used by different circuits)
            if (visited[top] || in->getTypeCirc() != top->getTypeCirc()) { // ((in->getTypeCirc() & top->getTypeCirc()) != top->getTypeCirc())
                in->setTypeCirc(in->getTypeCirc() | top->getTypeCirc()); // change type
                s.push(in); // add node to visit it's inputs
            }
        }
    }
    return order;
}

/*******************************************************************************
 *      Choose the id for each gate and circuit, stratum where to place it
 ******************************************************************************/

void Circuit::markCircuitStratumAndId(const std::deque<GatePtr>& order) {
//    // TEST -----
//    gettimeofday(&time_start, 0); // start timer
//    // ----------
    
    infos.gates.resize(infos.values_size, nullptr);
    for (GatePtr g : order) {
        // choose the circuit where to place the gate
        if(g->getTypeCirc() != Gate::LEG && g->getTypeCirc() != Gate::GL && g->getTypeCirc() != Gate::NXT && g->getTypeCirc() != Gate::LEG_NXT)
            g->setInsideCirc(Circuit::Info::TERMINAL);
        else if(g->getTypeCirc() == Gate::LEG || g->getTypeCirc() == Gate::LEG_NXT)
            g->setInsideCirc(Circuit::Info::LEGAL);
        else if(g->getTypeCirc() == Gate::GL)
            g->setInsideCirc(Circuit::Info::GOAL);
        else {
            assert(g->getTypeCirc() == Gate::NXT);
            g->setInsideCirc(Circuit::Info::NEXT);
        }
        // choose the id of the gate
        if((g->getType() == Gate::FACT || g->getType() == Gate::WIRE) && infos.inv_vars.find(g->getTerm()) != infos.inv_vars.end())
            g->setId((int) infos.inv_vars.at(g->getTerm()));
        else if(g->getType() == Gate::AND && g->getInputs().empty())
            g->setId(0);                                    // always true
        else if(g->getType() == Gate::OR && g->getInputs().empty())
            g->setId(1);                                    // always false
        else {
            g->setId((int) infos.values_size);
            ++infos.values_size;
            infos.gates.push_back(nullptr);
            // store subexpr names
            if (g->getTerm() != nullptr)
                infos.sub_expr_terms[g->getId()] = g->getTerm();
        }
        
        //choose the stratum
        assert(g->getInputs().size() <= 2);
        int stratum = g->getStratum();
//        int gstratum = g->getGstratum();
//        int tstratum = g->getTstratum();
        for(GatePtr in : g->getInputs()) {
            if(in->getType() != Gate::FACT) {
                assert(in->getInsideCirc() < 4);
                if(g->getInsideCirc() == in->getInsideCirc())
                    stratum = std::max(stratum, in->getStratum());
            }
//            gstratum = std::max(gstratum, in->getGstratum());
//            tstratum = std::max(tstratum, in->getTstratum());
        }
        g->setStratum(++stratum);
        // si c'est une porte du circuit GOAL
//        if ((g->getTypeCirc() & Gate::GL) == Gate::GL) {
//            g->setGstratum(++gstratum);
//            if(g->getGstratum() >= infos.goal_gates.size()) // gstratums
//                infos.goal_gates.resize(g->getGstratum() + 1);
//            infos.goal_gates[g->getGstratum()].push_back(g->getId());
//        }
//        // si c'est une porte du circuit TERMINAL
//        if ((g->getTypeCirc() & Gate::TERM) == Gate::TERM) {
//            g->setTstratum(++tstratum);
//            if(g->getTstratum() >= infos.terminal_gates.size()) // tstratums
//                infos.terminal_gates.resize(g->getTstratum() + 1);
//            infos.terminal_gates[g->getTstratum()].push_back(g->getId());
//        }

        // store the gate
        assert(g->getId() < infos.gates.size());
        assert(infos.gates[g->getId()] == nullptr);
        infos.gates[g->getId()] = g;
        // get the premisses
        //g->computePremises(infos.sub_expr, infos.id_does);
    }
    
//    printVars();
//    
//    cout << "SUBEXPR------------------" << endl;
//    for (GatePtr g : infos.sub_expr) {
//        for (const auto& p : g->getPremises()) {
//            for (const auto& f : p.trues) {
//                std::cout << f.id;
//                if (f.id != (*p.trues.rbegin()).id)
//                    std::cout << ".";
//            }
//            for (const auto& f : p.does) {
//                std::cout << f.id;
//                if (f.id != (*p.trues.rbegin()).id)
//                    std::cout << ".";
//            }
//            std::cout << " + ";
//        }
//        std::cout << std::endl;
//    }
//
//    // TEST -----
//    gettimeofday(&time_end, 0); // get current time
//    out_time << "Disjunctive normal forms collected in    ";
//    out_time << TIME_SPENT(time_start, time_end) << " seconds" << endl;
//    // ----------

}

/*******************************************************************************
 *      Use the circuit
 ******************************************************************************/

/* set a position */
void Circuit::setPosition(VectorBool& values, const VectorTermPtr& pos) const {
    memset(values.data() + infos.id_true, 0, (infos.id_does.front() - infos.id_true) * sizeof(values.front()));
    values[0] = -1; // au cas où...
    for (TermPtr t : pos) {
        auto it = infos.inv_vars.find(t);
        if (it != infos.inv_vars.end()) {
            values[it->second] = -1;
        }
    }
    //terminal_legal_goal(values); // supprimé pour diffuser le signal avec logique tri-valuée pendant la décomposiiton
}

const VectorTermPtr& Circuit::getInitPos() const {
    return infos.init;
}

const VectorTermPtr& Circuit::getRoles() const {
    return infos.roles;
}

const VectorTermPtr& Circuit::getPosition(const VectorBool& values) const {
    const_cast<VectorTermPtr&>(position).clear();
    for(size_t i = infos.id_true; i < infos.id_does.front(); ++i) {
        if(values[i])
            const_cast<VectorTermPtr&>(position).push_back(infos.vars[i]);
    }
    return position;
}

void Circuit::playout(VectorBool& values) {
//    int count = 0;
    vector<size_t> legals(infos.id_next - infos.id_legal[0]);
    int diff = (int) (infos.id_does[0] - infos.id_legal[0]);
    goals.assign(goals.size(), 0);
    for(;;) {
//        cout << "step=" << ++count << endl;
//        printTrueValues(values);
        compute(Info::TERMINAL, values);
        if (values[infos.id_terminal]) break;
        
        compute(Info::LEGAL, values);
        for(size_t i = 0; i < infos.id_legal.size() - 1; ++i) {
            legals.clear();
            for(size_t j = infos.id_legal[i]; j < infos.id_legal[i + 1]; ++j)
                if(values[j])
                    legals.push_back(j);
            if(legals.empty()) {
                cerr << "No legal move during playout" << endl;
                terminal = -1;
                return;
            }
            values[diff + legals[rand_gen() % legals.size()]] = -1;
        }
        compute(Info::NEXT, values);
        memcpy(values.data() + infos.id_true, values.data() + infos.id_next, (infos.id_does.front() - infos.id_true) * sizeof(values.front()));
        memset(values.data() + infos.id_does.front(), 0, (infos.id_does.back() - infos.id_does.front()) * sizeof(values.front()));
    }
    compute(Info::GOAL, values);
    
    for (size_t i = 0; i < infos.id_goal.size() - 1; ++i) {
        for(size_t j = infos.id_goal[i]; j < infos.id_goal[i+1]; ++j)
            if(values[j]) {
                Score s = stoi(infos.vars[j]->getArgs().back()->getFunctor()->getName());
                if (goals[i] != 0) cout << "MORE THAN ONE GOAL (" << goals[i] << "/" << s << ") FOR ROLE " << i << endl;
                if (goals[i] < s) goals[i] = s;
            }
    }

}

void Circuit::terminal_legal_goal(VectorBool& values) {
    compute(Info::TERMINAL, values);
    terminal = values[infos.id_terminal];
    if(!terminal) {
        compute(Info::LEGAL, values);
        for(size_t i = 0; i < infos.id_legal.size() - 1; ++i) {
            legals[i].clear();
            for(size_t j = infos.id_legal[i]; j < infos.id_legal[i+1]; ++j)
                if(values[j])
                    legals[i].push_back(infos.vars[j + infos.id_does.front() - infos.id_legal.front()]);
        }
    } else {
        compute(Info::GOAL, values);
        goals.assign(goals.size(), 0);
        for(size_t i = 0; i < infos.id_goal.size() - 1; ++i) {
            for(size_t j = infos.id_goal[i]; j < infos.id_goal[i+1]; ++j)
                if(values[j]) {
                    Score s = stoi(infos.vars[j]->getArgs().back()->getFunctor()->getName());
                    if (goals[i] != 0) cout << "MORE THAN ONE GOAL (" << goals[i] << "/" << s << ") FOR ROLE " << i << endl;
                    if (goals[i] < s) goals[i] = s;
                }
        }
    }
}

bool Circuit::isTerminal(const VectorBool& values) const {
    return terminal;
}

const VectorTermPtr& Circuit::getLegals(const VectorBool& values, TermPtr role) const {
    return legals[infos.inv_roles.at(role)];
}

void Circuit::setMove(VectorBool& values, TermPtr move) const {
    auto it = infos.inv_vars.find(move);
    if(it == infos.inv_vars.end())
        cerr << *move << " is not legal.";
    values[it->second] = -1;
}

void Circuit::next(VectorBool& values) {
//    cout << "before next" << endl;
//    printTrueValues(values);
    compute(Info::NEXT, values);
//    cout << "after next" << endl;
//    printTrueValues(values);
    memcpy(values.data() + infos.id_true, values.data() + infos.id_next, (infos.id_does.front() - infos.id_true) * sizeof(values.front()));
//    cout << "after copy on trues" << endl;
//    printTrueValues(values);
    memset(values.data() + infos.id_does.front(), 0, (infos.id_does.back() - infos.id_does.front()) * sizeof(values.front()));
//    cout << "after erase does" << endl;
//    printTrueValues(values);
    terminal_legal_goal(values);
//    cout << "after terminal legal goal" << endl;
//    printTrueValues(values);
//    cout << endl;
}

Score Circuit::getGoal(const VectorBool& values, TermPtr role) const {
    return goals[infos.inv_roles.at(role)];
}

void Circuit::computeGoal(VectorBool& values) {
    compute(Info::TERMINAL, values);
    terminal = values[infos.id_terminal];
    compute(Info::GOAL, values);
    goals.assign(goals.size(), -1);
    for(size_t i = 0; i < infos.id_goal.size() - 1; ++i) {
        for(size_t j = infos.id_goal[i]; j < infos.id_goal[i+1]; ++j)
            if(values[j]) {
                Score s = stoi(infos.vars[j]->getArgs().back()->getFunctor()->getName());
                if (goals[i] != -1) cout << "MORE THAN ONE GOAL (" << goals[i] << "/" << s << ") FOR ROLE " << i << endl;
                if (goals[i] < s) goals[i] = s;
            }
        if (goals[i] == -1) {
            cout << "NO GOAL FOR ROLE " << i;
            goals[i] = 0;
        }
    }
}

/*******************************************************************************
 *      Propagate the signal into the circuit
 ******************************************************************************/

/* compute a circuit */
void Circuit::compute(Info::TypeCircuit type, VectorBool& values) const {
    for(const Stratum& s : infos.circuits[type]) {
        compute_not(s.v_not, values);
        compute_and(s.v_and, values);
        compute_or(s.v_or, values);
    }
}
void Circuit::compute_not(const std::vector<OpNot>& v, VectorBool& values) const {
    for (const OpNot& op : v) {
        values[op.res] = ~values[op.arg];
        //cout << " " << op.res << "=" << values[op.res] << " :- ~" << op.arg << "=" << values[op.arg] << endl;
    }
}
void Circuit::compute_and(const std::vector<OpAnd>& v, VectorBool& values) const {
    for (const OpAnd& op : v) {
        values[op.res] = values[op.arg1] &  values[op.arg2];
        //cout << " " << op.res << "=" << values[op.res] << " :- " << op.arg1 << "=" << values[op.arg1] << " & " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}
void Circuit::compute_or(const std::vector<OpOr>& v, VectorBool& values) const {
    for (const OpOr& op : v) {
        values[op.res] = values[op.arg1] | values[op.arg2];
        //cout << " " << op.res << "=" << values[op.res] << " :- " << op.arg1 << "=" << values[op.arg1] << " | " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}


/* back propagate a signal into the circuit : don't care of the gate type */
void Circuit::backPropagate(VectorBool& values) const {
    for (auto rit_circ = infos.circuits.rbegin(); rit_circ != infos.circuits.rend(); ++rit_circ) {
        auto circ = *rit_circ;
        for (vector<Stratum>::reverse_iterator rit = circ.rbegin(); rit != circ.rend(); ++rit) {
            backPropagate(rit->v_not, values);
            backPropagate(rit->v_and, values);
            backPropagate(rit->v_or, values);
        }
    }
}
void Circuit::backPropagate(const std::vector<OpNot>& v, VectorBool& values) const {
    for (const OpNot& op : v) {
        values[op.arg] |= values[op.res];
        //cout << " " << op.res << "=" << values[op.res] << " => " << op.arg << "=" << values[op.arg] << endl;
    }
}
void Circuit::backPropagate(const std::vector<OpAnd>& v, VectorBool& values) const {
    for (const OpAnd& op : v) {
        values[op.arg1] |= values[op.res];
        values[op.arg2] |= values[op.res];
        //cout << " " << op.res << "=" << values[op.res] << " => " << op.arg1 << "=" << values[op.arg1] << " " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}
void Circuit::backPropagate(const std::vector<OpOr>& v, VectorBool& values) const {
    for (const OpOr& op : v) {
        values[op.arg1] |= values[op.res];
        values[op.arg2] |= values[op.res];
        //cout << " " << op.res << "=" << values[op.res] << " => " << op.arg1 << "=" << values[op.arg1] << " " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}

/* spread a signal into the circuit : don't care of the gate type */
void Circuit::spread(VectorBool& values) const {
    for (const auto& circ: infos.circuits) {
        for (const Stratum& s : circ) {
            spread(s.v_not, values);
            spread(s.v_and, values);
            spread(s.v_or, values);
        }
    }
}
void Circuit::spread(const std::vector<OpNot>& v, VectorBool& values) const {
    for (const OpNot& op : v) {
        values[op.res] |= values[op.arg];
        //cout << " " << op.res << "=" << values[op.res] << " <= ~" << op.arg << "=" << values[op.arg] << endl;
    }
}
void Circuit::spread(const std::vector<OpAnd>& v, VectorBool& values) const {
    for (const OpAnd& op : v) {
        values[op.res] |= values[op.arg1] | values[op.arg2];
        //cout << " " << op.res << "=" << values[op.res] << " <= " << op.arg1 << "=" << values[op.arg1] << " & " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}
void Circuit::spread(const std::vector<OpOr>& v, VectorBool& values) const {
    for (const OpOr& op : v) {
        values[op.res] |= values[op.arg1] | values[op.arg2];
        //cout << " " << op.res << "=" << values[op.res] << " <= " << op.arg1 << "=" << values[op.arg1] << " | " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}

/* Propagate a signal into the circuit : take care of the gate type but uses 3 states (true, false, undefined) : UNDEF.TRUE=TRUE */
void Circuit::propagate3States(VectorBool& values) const {
    for (const auto& circ: infos.circuits) {
        for (const Stratum& s : circ) {
            propagate3States(s.v_not, values);
            propagate3States(s.v_and, values);
            propagate3States(s.v_or, values);
        }
    }
}
void Circuit::propagate3States(const std::vector<OpNot>& v, VectorBool& values) const {
    // ~UNDEF=UNDEF  ~TRUE=FALSE  ~FALSE=TRUE
    for (const OpNot& op : v) {
        if (values[op.arg] != UNDEF) { // input different from UNDEF
            values[op.res] = ~values[op.arg];
        }
        //cout << " " << op.res << "=" << values[op.res] << " <= ~" << op.arg << "=" << values[op.arg] << endl;
    }
}
void Circuit::propagate3States(const std::vector<OpAnd>& v, VectorBool& values) const {
    // UNDEF.FALSE=FALSE  FALSE.FALSE=FALSE  TRUE.FALSE=FALSE
    // UNDEF.TRUE=TRUE    TRUE.TRUE=TRUE
    // UNDEF.UNDEF=UNDEF
    for (const OpAnd& op : v) {
        if ((values[op.arg1] & values[op.arg2]) == BOOL_F)
            values[op.res] = BOOL_F;
        else if ((values[op.arg1] | values[op.arg2]) == BOOL_T)
            values[op.res] = BOOL_T;
        else
            values[op.res] = UNDEF;
        //cout << " " << op.res << "=" << values[op.res] << " <= " << op.arg1 << "=" << values[op.arg1] << " & " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}
void Circuit::propagate3States(const std::vector<OpOr>& v, VectorBool& values) const {
    // UNDEF+TRUE=TRUE  UNDEF+FALSE=UNDEF  UNDEF+UNDEF=UNDEF TRUE+FALSE=TRUE
    for (const OpOr& op : v) {
        values[op.res] = values[op.arg1] | values[op.arg2];
        //cout << " " << op.res << "=" << values[op.res] << " <= " << op.arg1 << "=" << values[op.arg1] << " | " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}

/* Propagate a signal into the circuit : take care of the gate type but uses 3 states (true, false, undefined) : UNDEF.TRUE=UNDEF
 Preserve value of gates already setted */
void Circuit::propagate3StatesStrict(VectorBool& values) const {
    for (const auto& circ: infos.circuits) {
        for (const Stratum& s : circ) {
            propagate3StatesStrict(s.v_not, values);
            propagate3StatesStrict(s.v_and, values);
            propagate3StatesStrict(s.v_or, values);
        }
    }
}
void Circuit::propagate3StatesStrict(const std::vector<OpNot>& v, VectorBool& values) const {
    // ~UNDEF=UNDEF  ~TRUE=FALSE  ~FALSE=TRUE
    // but if res==TRUE or res==FALSE do not change anything
    for (const OpNot& op : v) {
        if (values[op.res] != UNDEF)
            continue;
        if (values[op.arg] != UNDEF) {
            values[op.res] = ~values[op.arg];
        }
        //cout << " " << op.res << "=" << values[op.res] << " <= ~" << op.arg << "=" << values[op.arg] << endl;
    }
}
void Circuit::propagate3StatesStrict(const std::vector<OpAnd>& v, VectorBool& values) const {
    // UNDEF.TRUE=UNDEF  UNDEF.FALSE=FALSE  UNDEF.UNDEF=UNDEF
    // but if res==TRUE or res==FALSE do not change anything
    for (const OpAnd& op : v) {
        if (values[op.res] != UNDEF)
            continue;
        else
            values[op.res] = values[op.arg1] & values[op.arg2];
        //cout << " " << op.res << "=" << values[op.res] << " <= " << op.arg1 << "=" << values[op.arg1] << " & " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}
void Circuit::propagate3StatesStrict(const std::vector<OpOr>& v, VectorBool& values) const {
    // UNDEF+TRUE=TRUE  UNDEF+FALSE=UNDEF  UNDEF+UNDEF=UNDEF  TRUE+FALSE=TRUE
    // but if res==TRUE or res==FALSE do not change anything
    for (const OpOr& op : v) {
        if (values[op.res] != UNDEF)
            continue;
        else
            values[op.res] = values[op.arg1] | values[op.arg2];
        //cout << " " << op.res << "=" << values[op.res] << " <= " << op.arg1 << "=" << values[op.arg1] << " | " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}

/* Propagate a signal into the circuit : take care only of not gates and uses 3 states (USED_FALSE, USED_TRUE, USED_TANDF, DIF_TRUE_FALSE) */
void Circuit::propagateNot(VectorBool& values) const {
    for (const auto& circ: infos.circuits) {
        for (const Stratum& s : circ) {
            propagateNot(s.v_not, values);
            propagateNot(s.v_and, values);
            propagateNot(s.v_or, values);
        }
    }
}
void Circuit::propagateNot(const std::vector<OpNot>& v, VectorBool& values) const {
    // switch values of two first bits
    for (const OpNot& op : v) {
        values[op.res] = ((values[op.arg] & USED_FALSE)>>DIF_TRUE_FALSE) | ((values[op.arg] & USED_TRUE)<<DIF_TRUE_FALSE);
        //cout << " " << op.res << "=" << values[op.res] << " <= " << op.arg << "=" << values[op.arg] << endl;
    }
}
void Circuit::propagateNot(const std::vector<OpAnd>& v, VectorBool& values) const {
    // output cumulates values of inputs
    for (const OpAnd& op : v) {
        values[op.res] = values[op.arg1] | values[op.arg2] | values[op.res];
        //cout << " " << op.res << "=" << values[op.res] << " <= " << op.arg1 << "=" << values[op.arg1] << " | " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}
void Circuit::propagateNot(const std::vector<OpOr>& v, VectorBool& values) const {
    // output cumulates values of inputs
    for (const OpOr& op : v) {
        values[op.res] = values[op.arg1] | values[op.arg2] | values[op.res];
        //cout << " " << op.res << "=" << values[op.res] << " <= " << op.arg1 << "=" << values[op.arg1] << " | " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}

/* back propagate a signal into the circuit to find fluents used in conjunction with an action (true or false)
   (signal has been already "propagateNot()" to the output with all gates NOT_USED and one input at USED_TRUE
   and all output except one (setted to USED_CONJ|USED_NORMAL|USED_TRUE or USED_CONJ|USED_NORMAL|USED_FALSE) are turned off (NOT_USED)
   the fluent flagged CONJ|WITH_TRUE or CONJ|WITH_FALSE are used respectively true or false in conjunction with the given action */
void Circuit::backPropagateConj(VectorBool& values) const {
    for (auto rit_circ = infos.circuits.rbegin(); rit_circ != infos.circuits.rend(); ++rit_circ) {
        auto circ = *rit_circ;
        for (vector<Stratum>::reverse_iterator rit = circ.rbegin(); rit != circ.rend(); ++rit) {
            backPropagateConj(rit->v_not, values);
            backPropagateConj(rit->v_and, values);
            backPropagateConj(rit->v_or, values);
        }
    }
}
void Circuit::backPropagateConj(const std::vector<OpNot>& v, VectorBool& values) const {
    for (const OpNot& op : v) {
        //cout << "A" << std::dec << op.res << "=" << std::hex << values[op.res] << " => ~" << std::dec << op.arg << "=" << std::hex << values[op.arg] << endl;
        values[op.arg] |= (values[op.res] & USED_CONJ) | ((values[op.res] & USED_FALSE)>>DIF_TRUE_FALSE) | ((values[op.res] & USED_TRUE)<<DIF_TRUE_FALSE)
                                                       | ((values[op.res] & USED_REVERS)>>DIF_TRUE_FALSE) | ((values[op.res] & USED_NORMAL)<<DIF_TRUE_FALSE)
                                                       | ((values[op.res] & WITH_FALSE)>>DIF_TRUE_FALSE) | ((values[op.res] & WITH_TRUE)<<DIF_TRUE_FALSE);
        //cout << "B" << std::dec << op.res << "=" << std::hex << values[op.res] << " => ~" << std::dec << op.arg << "=" << std::hex << values[op.arg] << endl;
    }
}
void Circuit::backPropagateConj(const std::vector<OpAnd>& v, VectorBool& values) const {
    for (const OpAnd& op : v) {
        //cout << "A" << std::dec << op.res << "=" << std::hex << values[op.res] << " => " << std::dec << op.arg1 << "=" << std::hex << values[op.arg1] << " & " << std::dec << op.arg2 << "=" << std::hex << values[op.arg2] << endl;
        if ((values[op.res] & USED_CONJ) == USED_CONJ) {
            // portes en aval de la conjonction, on propage juste le signal
            if ((values[op.res] & USED_TANDF) == NOT_USED) {
                values[op.arg1] |= values[op.res];
                values[op.arg2] |= values[op.res];
            }
            // porte en amont de la conjonction (à considérer comme un ET ou un OU)
            else {
                Boolean new_arg1 = values[op.arg1] & values[op.res] & USED_TANDF;
                Boolean new_arg2 = values[op.arg2] & values[op.res] & USED_TANDF;
                if (new_arg1 != NOT_USED) {
                    values[op.arg1] = new_arg1 | (values[op.res] & USED_STATUS) | USED_CONJ;
                    if ((values[op.res] & USED_STATUS) == USED_NORMAL && new_arg2 == NOT_USED) // élément utilisé en conjonction
                        values[op.arg2] |= WITH_TRUE | USED_CONJ;
                }
                if (new_arg2 != NOT_USED) {
                    values[op.arg2] = new_arg2 | (values[op.res] & USED_STATUS) | USED_CONJ;
                    if ((values[op.res] & USED_STATUS) == USED_NORMAL && new_arg1 == NOT_USED) // élément utilisé en conjonction
                        values[op.arg1] |= WITH_TRUE | USED_CONJ;
                }
            }
        }
        //cout << "B" << std::dec << op.res << "=" << std::hex << values[op.res] << " => " << std::dec << op.arg1 << "=" << std::hex << values[op.arg1] << " & " << std::dec << op.arg2 << "=" << std::hex << values[op.arg2] << endl;
    }
}
void Circuit::backPropagateConj(const std::vector<OpOr>& v, VectorBool& values) const {
    for (const OpOr& op : v) {
        //cout << "A" << std::dec << op.res << "=" << std::hex << values[op.res] << " => " << std::dec << op.arg1 << "=" << std::hex << values[op.arg1] << " | " << std::dec << op.arg2 << "=" << std::hex << values[op.arg2] << endl;
        if ((values[op.res] & USED_CONJ) == USED_CONJ) {
            // portes en aval de la conjonction, on propage juste le signal
            if ((values[op.res] & USED_TANDF) == NOT_USED) {
                values[op.arg1] |= values[op.res];
                values[op.arg2] |= values[op.res];
            }
            // porte en amont de la conjonction (à considérer comme un ET ou un OU)
            else {
                Boolean new_arg1 = values[op.arg1] & values[op.res] & USED_TANDF;
                Boolean new_arg2 = values[op.arg2] & values[op.res] & USED_TANDF;
                if (new_arg1 != NOT_USED) {
                    values[op.arg1] = new_arg1 | (values[op.res] & USED_STATUS) | USED_CONJ;
                    if ((values[op.res] & USED_STATUS) == USED_REVERS && new_arg2 == NOT_USED) // élément utilisé en conjonction
                        values[op.arg2] |= WITH_TRUE | USED_CONJ;
                }
                if (new_arg2 != NOT_USED) {
                    values[op.arg2] = new_arg2 | (values[op.res] & USED_STATUS) | USED_CONJ;
                    if ((values[op.res] & USED_STATUS) == USED_REVERS && new_arg1 == NOT_USED) // élément utilisé en conjonction
                        values[op.arg1] |= WITH_TRUE | USED_CONJ;
                }
            }
        }
        //cout << "B" << std::dec << op.res << "=" << std::hex << values[op.res] << " => " << std::dec << op.arg1 << "=" << std::hex << values[op.arg1] << " | " << std::dec << op.arg2 << "=" << std::hex << values[op.arg2] << endl;
    }
}

/* Back propagate a signal into the circuit : do not take care of the gate type except NOT and uses 4 states (NOT_USED, USED_FALSE, USED_TRUE, USED_TANDF + DIF_TRUE_FALSE) */
void Circuit::backPropagateNot(VectorBool& values) const {
    for (auto rit_circ = infos.circuits.rbegin(); rit_circ != infos.circuits.rend(); ++rit_circ) {
        auto circ = *rit_circ;
        for (vector<Stratum>::reverse_iterator rit = circ.rbegin(); rit != circ.rend(); ++rit) {
            backPropagateNot(rit->v_not, values);
            backPropagateNot(rit->v_and, values);
            backPropagateNot(rit->v_or, values);
        }
    }
}
void Circuit::backPropagateNot(const std::vector<OpNot>& v, VectorBool& values) const {
    // switch values of two first bits
    for (const OpNot& op : v) {
        values[op.arg] |= ((values[op.res] & USED_FALSE)>>DIF_TRUE_FALSE) | ((values[op.res] & USED_TRUE)<<DIF_TRUE_FALSE);
        //cout << " " << op.res << "=" << values[op.res] << " <= " << op.arg << "=" << values[op.arg] << endl;
    }
}
void Circuit::backPropagateNot(const std::vector<OpAnd>& v, VectorBool& values) const {
    for (const OpAnd& op : v) {
        values[op.arg1] |= values[op.res];
        values[op.arg2] |= values[op.res];
        //cout << " " << op.res << "=" << values[op.res] << " => " << op.arg1 << "=" << values[op.arg1] << " " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}
void Circuit::backPropagateNot(const std::vector<OpOr>& v, VectorBool& values) const {
    for (const OpOr& op : v) {
        values[op.arg1] |= values[op.res];
        values[op.arg2] |= values[op.res];
        //cout << " " << op.res << "=" << values[op.res] << " => " << op.arg1 << "=" << values[op.arg1] << " " << op.arg2 << "=" << values[op.arg2] << endl;
    }
}

/* Propagate a signal into the circuit : take care of the gate type but uses 5 states (BOOL_T, BOOL_F, UNDEF, MAYB_T, MAYB_F) : UNDEF.TRUE=TRUE
 * la sortie ne change que si au moins une des entrées est différente de UNDEF */
void Circuit::propagate3StatesCcl(VectorBool& values) const {
    for (const auto& circ: infos.circuits) {
        for (const Stratum& s : circ) {
            propagate3StatesCcl(s.v_not, values);
            propagate3StatesCcl(s.v_and, values);
            propagate3StatesCcl(s.v_or, values);
        }
    }
}
void Circuit::propagate3StatesCcl(const std::vector<OpNot>& v, VectorBool& values) const {
    for (const OpNot& op : v) {
        //cout << "A " << std::dec << op.res << "=" << std::hex << values[op.res] << " <= ~" << std::dec << op.arg << "=" << std::hex << values[op.arg] << endl;
        if (values[op.arg] != UNDEF) { // input different from UNDEF
            values[op.res] = ~values[op.arg];
        }
        //cout << "B " << std::dec << op.res << "=" << std::hex << values[op.res] << " <= ~" << std::dec << op.arg << "=" << std::hex << values[op.arg] << endl;
    }
}
void Circuit::propagate3StatesCcl(const std::vector<OpAnd>& v, VectorBool& values) const {
    for (const OpAnd& op : v) {
        //cout << "A " << std::dec << op.res << "=" << std::hex << values[op.res] << " <= " << std::dec << op.arg1 << "=" << std::hex << values[op.arg1] << " & " << std::dec << op.arg2 << "=" << std::hex << values[op.arg2] << endl;
        if (values[op.arg1] == BOOL_F || values[op.arg2] == BOOL_F)
            values[op.res] = BOOL_F;
        else if ((values[op.arg1] & values[op.arg2]) == BOOL_T)
            values[op.res] = BOOL_T;
        else if ((values[op.arg1] == MAYB_F && (values[op.arg2] == MAYB_F || values[op.arg2] == UNDEF)) ||
                 (values[op.arg2] == MAYB_F && values[op.arg1] == UNDEF) )
            values[op.res] = MAYB_F;
        else if (values[op.arg1] != UNDEF || values[op.arg2] != UNDEF)
            values[op.res] = MAYB_T;
        //cout << "B " << std::dec << op.res << "=" << std::hex << values[op.res] << " <= " << std::dec << op.arg1 << "=" << std::hex << values[op.arg1] << " & " << std::dec << op.arg2 << "=" << std::hex << values[op.arg2] << endl;
    }
}
void Circuit::propagate3StatesCcl(const std::vector<OpOr>& v, VectorBool& values) const {
    for (const OpOr& op : v) {
        //cout << "A " << std::dec << op.res << "=" << std::hex << values[op.res] << " <= " << std::dec << op.arg1 << "=" << std::hex << values[op.arg1] << " | " << std::dec << op.arg2 << "=" << std::hex << values[op.arg2] << endl;
        if ((values[op.arg1] | values[op.arg2]) == BOOL_F)
            values[op.res] = BOOL_F;
        else if (values[op.arg1] == BOOL_T || values[op.arg2] == BOOL_T)
            values[op.res] = BOOL_T;
        else if (values[op.arg1] == MAYB_T || values[op.arg2] == MAYB_T)
            values[op.res] = MAYB_T;
        else if (values[op.arg1] != UNDEF || values[op.arg2] != UNDEF)
            values[op.res] = MAYB_F;
        //cout << "B " << std::dec << op.res << "=" << std::hex << values[op.res] << " <= " << std::dec << op.arg1 << "=" << std::hex << values[op.arg1] << " | " << std::dec << op.arg2 << "=" << std::hex << values[op.arg2] << endl;
    }
}


/*******************************************************************************
 *      for debug : print vars and rules
 ******************************************************************************/

void Circuit::printVars(std::ostream& out) const {
    out << "[0] always true" << endl;
    out << "[1] always false" << endl;
    for (size_t i = 2; i < infos.vars.size(); ++i)
        out << "[" << i << "] " << *infos.vars[i] << endl;
    for (const auto& entry : infos.sub_expr_terms)
        out << "[" << entry.first << "] " << *entry.second << endl;
}

void Circuit::printValues(const VectorBool& values) const {
    cout << "[0]=" <<  values[0] << " always true" << endl;
    cout << "[1]=" <<  values[1] << " always false" << endl;
    for (size_t i = 2; i < infos.vars.size(); ++i)
        cout << "[" << i << "]=" << values[i] << "  " << *infos.vars[i] << endl;
    for (const auto& entry : infos.sub_expr_terms)
        cout << "[" << entry.first << "]=" << values[entry.first] << "  " << *entry.second << endl;
}

void Circuit::printTrueValues(const VectorBool& values) const {
    for (size_t i = 2; i < infos.vars.size(); ++i)
        if (values[i])
            cout << "[" << i << "]=" << values[i] << "  " << *infos.vars[i] << endl;
    for (const auto& entry : infos.sub_expr_terms)
        if (values[entry.first])
            cout << "[" << entry.first << "]=" << values[entry.first] << "  " << *entry.second << endl;
}

void Circuit::printRules(std::ostream& out) const {
    for (int c_id = 0; c_id < Info::NB_TYPE_PROG; c_id++) {
        switch (c_id) {
            case Info::TERMINAL:
                out << "CIRCUIT TERMINAL -------------" << endl;
                break;
            case Info::LEGAL:
                out << "CIRCUIT LEGAL -------------" << endl;
                break;
            case Info::GOAL:
                out << "CIRCUIT GOAL -------------" << endl;
                break;
            case Info::NEXT:
                out << "CIRCUIT NEXT -------------" << endl;
                break;
            default:
                break;
        }
        for (size_t s_id = 0; s_id < infos.circuits[c_id].size(); s_id++) {
            out << "    stratum " << s_id << "-------------" << endl;
            for (OpNot op : infos.circuits[c_id][s_id].v_not)
                out << op.res << " :- ~" << op.arg << endl;
            for (OpAnd op : infos.circuits[c_id][s_id].v_and)
                out << op.res << " :- " << op.arg1 << " and " << op.arg2 << endl;
            for (OpOr op : infos.circuits[c_id][s_id].v_or)
                out << op.res << " :- " << op.arg1 << " or " << op.arg2 << endl;
        }
    }
}

