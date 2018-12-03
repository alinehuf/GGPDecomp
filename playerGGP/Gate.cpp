//
//  Gate.cpp
//  playerGGP
//
//  Created by Dexter on 01/12/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#include "Gate.h"

using std::set;
using std::vector;

/*******************************************************************************
 *      Gate
 ******************************************************************************/

/* create a Gate, keep a trace of the original GDL Term */
Gate::Gate(Type ty, TermPtr t, SetGatePtr&& in): type(ty), term(t), inputs(std::forward<SetGatePtr>(in)) {
    // inputs may have been replaced since their adresses were recorded
    SetGatePtr inp;
    for(GatePtr g : inputs) {
        while(g->type == NONE)      // type is undefined
            g = *g->inputs.begin(); // find the replacement as the first input
        inp.insert(g);
    }
    inputs.swap(inp);
    computeHash();
    
    typeCirc = 0;
    insideCirc = 0;
    stratum = -1;
//    gstratum = -1;
    gate_id = (size_t) -1;
    //no_does = true;
    //is_subexpr = false;
//    status = NO_DOES;
}

void Gate::computeHash() {
    long int i;
    assert(type != NONE);
    for(GatePtr g : inputs)
        assert(g->type != NONE);
    switch (type) {
        case AND:
            hash = std::hash<std::string>()("AND");
            i = 2;
            for(GatePtr gi : inputs) {
                assert(gi->type != NONE);
                //hash ^= gi->hash * i;
                hash ^= (long int) gi * i;
                i += 2;
            }
            hash *= 11;
            break;
        case OR:
            hash = std::hash<std::string>()("OR");
            i = 2;
            for(GatePtr gi : inputs) {
                //hash ^= gi->hash * i;
                hash ^= (long int) gi * i;
                i += 2;
            }
            hash *= 11;
            break;
        case NOT:
            hash = ~(*inputs.begin())->hash;
            break;
        case FACT: case WIRE:
            hash = term->getHash();
            break;
        default:
            hash = 0;
            break;
    }
}

/* necessary for set of gates inside GateFactory */
bool Gate::operator==(const Gate& g) const {
    return (type == g.type
            && ((type != FACT && type != WIRE) || term == g.term)
            && inputs == g.inputs);
}

/* exchange the content of two gates except their outputs */
void Gate::swapExceptOutputs(Gate& gate) {
    if (&gate == this) return;
    std::swap(type, gate.type);
    if (type == WIRE || type == FACT)
        std::swap(term, gate.term);
    std::swap(hash, gate.hash);
    std::swap(inputs, gate.inputs);
}

/*******************************************************************************
 *      Gate::computePremises : usefull for decomposition
 ******************************************************************************/

/* compute premisses : for decomposition */
/*
void Gate::computePremises(set<GateId>& sub_expr, const vector<GateId>& id_does, bool redo) {
    // re-compute the premisses after changing the gate status
    if (redo) {
        // if premisses have been already computed
        set_premisses.clear();
    }
    // compute the status
    else {
        // ignore always false and always true
        if (gate_id < 2)
            return;
        // FACT : no premisses but mark inputs corresponding to actions
        else if (type == Gate::FACT) {
            if (term->getFunctor()->getName() == "does")
                status = WITH_DOES;
            return;
        }
        // what is the status of this gate : is there actions in its premisses
        int in1_status = (*inputs.begin())->status;
        int in2_status = (*inputs.rbegin())->status;
        status = NO_DOES & in1_status & in2_status;
        // no action in its premisses
        if (status == NO_DOES) {
            // an expression (not a simple fact negated or not)
            if (type == AND || type == OR) {
                status |= EXPR;
            }
            // if it is used more than once           //  and has an expression (not a simple fact) at input => it's a sub-expression
            if (outputs.size() > 1) {                 // && (type != NOT || ((in1_status & EXPR) == EXPR))) {
                status |= SUB_EXPR;
                sub_expr.insert(gate_id);
            }
            // if it is used once but it is a NOT gate or an OR gate used inside a AND gate :
            // declare it as a subexpression to use its id and avoid a big expression after development
            else if ((type == NOT && ((in1_status & EXPR) == EXPR)) || (type == OR && (*outputs.begin())->type == AND)) {
                status |= SUB_EXPR;
                sub_expr.insert(gate_id);
            }
        }
    }
    
    // collect the inputs premisses
    vector<GatePtr> in(inputs.begin(), inputs.end());
    vector<set<Premises>> vsp(2);
    for (size_t in_id = 0; in_id < in.size(); ++in_id) {
        // input of the gate is an input of the circuit
        if (in[in_id]->type == Gate::FACT || in[in_id]->getId() < 2) { // in[in_id]->set_premisses.empty()
            Premises p;
            if (in[in_id]->status == WITH_DOES)
                p.does.insert(Fact(in[in_id]->gate_id, true));
            else
                p.trues.insert(Fact(in[in_id]->gate_id, true));
            vsp[in_id].insert(p);
        }
        // get the id of the input
        else if ((in[in_id]->status & SUB_EXPR) == SUB_EXPR) {
        //else if ((in[in_id]->status & MAJOR_SUB_EXPR) == MAJOR_SUB_EXPR) {
            Premises p;
            p.trues.insert(Fact(in[in_id]->gate_id, true));
            vsp[in_id].insert(p);
            sub_expr.insert(in[in_id]->gate_id);
        }
        // get the premisses
        else {
            vsp[in_id] = in[in_id]->set_premisses;
        }
    }
    
    // WIRE : copy premisses
    if (type == Gate::WIRE) {
        assert(inputs.size() == 1);
        set_premisses = move(vsp[0]);
    }
    // NOT : negation of the disjunctive normal form
    else {
        if (type == Gate::NOT) {
            assert(inputs.size() == 1);
            
            for (const Premises& p1 : vsp[0]) {
                set<Premises> set_premisses_copy;
                set_premisses_copy.swap(set_premisses);
                
                for (const Fact& t1 : p1.trues) {
                    Premises p;
                    if (t1.id < 2) // always true or always false
                        p.trues.insert(Fact(1 - t1.id, t1.value));
                    else
                        p.trues.insert(Fact(t1.id, !t1.value));
                    if (set_premisses_copy.empty())
                        set_premisses.insert(p);
                    else
                        for (Premises p2 : set_premisses_copy)
                            insertValidatedPremises(p2.add(p), id_does);
                }
                for (const Fact& d1 : p1.does) {
                    Premises p;
                    p.does.insert(Fact(d1.id, !d1.value));
                    if (set_premisses_copy.empty())
                        set_premisses.insert(p);
                    else
                        for (Premises p2 : set_premisses_copy)
                            insertValidatedPremises(p2.add(p), id_does);
                }
            }
        }
        // AND or OR : get premisses or subexpression id for the two inputs
        // create a new disjunctive normal form
        else {
            assert(inputs.size() == 2);
            // AND : compound premisses
            if (type == Gate::AND) {
                for (const Premises& p1 : vsp[0]) {
                    for (Premises p2 : vsp[1]) { // copy the group of premisses in p2 to combine it with p1
                        insertValidatedPremises(p2.add(p1), id_does);
                    }
                }
            }
            // OR : regroup all premisses
            else if (type == Gate::OR) {
                set_premisses = move(vsp[0]);
                for (Premises p : vsp[1])
                    insertAndSimplifyPremises(std::move(p));
            }
            // unknown type
            else {
                std::cerr << *this << std::endl;
                assert(false);
            }
        }
    }
}

void Gate::insertValidatedPremises(Premises&& new_prem, const vector<GateId>& id_does) {
    bool always_false = false;
    // if a group of premisses contains 2 positive actions for the same player, remove it
    size_t nb_roles = id_does.size() - 1;
    vector<set<Fact>> does(nb_roles);
    vector<int> nb_does(nb_roles, 0);
    vector<set<Fact>> not_does(nb_roles);
    // for each player collect positives and negatives actions
    for (const Fact& d : new_prem.does) {
        int role = 0;
        while (d.id >= id_does[role+1]) role++;
        if (d.value) {
            // 2 positive actions into a rule body ? impossible case
            if (does[role].size() > 0){
                always_false = true;
                break;
            }
            does[role].insert(d);
        } else
            not_does[role].insert(d);
    }
    // if the premise group contains two contradictory facts we do not add it. impossible case
    if (new_prem.trues.size() > 1) {
        auto it1 = new_prem.trues.begin();
        auto it2 = it1;
        it2++;
        for (; it2 != new_prem.trues.end(); it2++) {
            if (it1->id == it2->id && it1->value != it2->value) {
                always_false = true;
                break;
            }
            it1 = it2;
        }
    }
    for (int role = 0; role < nb_roles; role++) {
        // if the premise group contains two contradictory actions we do not add it. impossible case
        if (does[role].size() > 0 && not_does[role].size() > 0)
            if ( not_does[role].find(Fact((*does[role].begin()).id, false)) != not_does[role].end() ){
                always_false = true;
                break;
            }
        // all negative actions from one player into a rule body ? impossible case
        if (not_does[role].size() == id_does[role+1] - id_does[role]) {
            always_false = true;
            break;
        }
        // if a group of premisses contains 1 positive action, remove all negative actions from the same player
        if (nb_does[role] > 0) {
            assert(not_does[role].size() == 0);
            new_prem.does.erase(not_does[role].begin(), not_does[role].end());
        }
    }
    
    if (always_false) {
        new_prem.trues.clear();
        new_prem.does.clear();
        new_prem.trues.insert(Fact(1, true));
    }
    
    // if a group of premisses includes another, remove it.
    insertAndSimplifyPremises(std::move(new_prem));
}

void Gate::insertAndSimplifyPremises(Premises&& p1) {
    // if a group of premisses includes another, remove it.
    auto it = set_premisses.begin();
    
//    // for debug
//    std::cout << "p1 = ";
//    for (const Fact& f : p1.trues)
//        std::cout << f.id << " (" << f.value << ") ";
//    for (const Fact& f : p1.does)
//        std::cout << f.id << " (" << f.value << ") ";
//    std::cout << std::endl;
//    std::cout << "p2 = " << std::endl;
//    for (const auto& p : set_premisses) {
//        for (const Fact& f : p.trues)
//            std::cout << f.id << " (" << f.value << ") ";
//        for (const Fact& f : p.does)
//            std::cout << f.id << " (" << f.value << ") ";
//        std::cout << std::endl;
//    }
//    std::cout << "-----------------------------" << std::endl;
    
    if (p1.trues.size() > 0 && p1.trues.begin()->id == 1) {        // this premise group is always false
        if (set_premisses.size() > 0)
            return;
    } else if (p1.trues.size() > 0 && p1.trues.begin()->id == 0) { // this premise group contains always true
        if (p1.trues.size() + p1.does.size() > 1)
            p1.trues.erase(p1.trues.begin()); // true.X => X
        else
            set_premisses.clear();  // X.Y. ... . true => true
    }
    else if (set_premisses.size() == 1) { // only one premisse for now
        if (set_premisses.begin()->trues.size() > 0 && set_premisses.begin()->trues.begin()->id == 1) // always false => erase it
            set_premisses.clear();
        else if (set_premisses.begin()->trues.size() == 1 && set_premisses.begin()->does.size() == 0
                 && set_premisses.begin()->trues.begin()->id == 0) // always true => keep only it
            return;
    } else {
        while (it != set_premisses.end()) {
            if (std::includes(p1.trues.begin(), p1.trues.end(), it->trues.begin(), it->trues.end()) &&
                std::includes(p1.does.begin(), p1.does.end(), it->does.begin(), it->does.end())) {
                    return;
            } else if (std::includes(it->trues.begin(), it->trues.end(), p1.trues.begin(), p1.trues.end()) &&
                       std::includes(it->does.begin(), it->does.end(), p1.does.begin(), p1.does.end())) {
                set_premisses.erase(it++);
            } else {
                ++it;
            }
        }
    }
    set_premisses.insert(p1);

//    // for debug
//    std::cout << "result = " << std::endl;
//    for (const auto& p : set_premisses) {
//        for (const Fact& f : p.trues)
//            std::cout << f.id << " (" << f.value << ") ";
//        for (const Fact& f : p.does)
//            std::cout << f.id << " (" << f.value << ") ";
//        std::cout << std::endl;
//    }
//    std::cout << "-----------------------------" << std::endl;
}
*/

/*******************************************************************************
 *      Print functions for debug
 ******************************************************************************/

/* for debugging, print a Gate */
std::ostream& operator<<(std::ostream& out, const Gate& g) {
    // name or adress
    if(g.term != nullptr)
        out << *(g.term) << " ";
    //else
        out << &g;
    // type
    const char* type_name[] = {"NONE", "FACT", "WIRE", "AND", "OR", "NOT"};
    out << " type " << type_name[(int) g.type] << std::endl;
    out << "    in <=";
    // inputs
    for (GatePtr gi : g.inputs) {
        if(gi->term != nullptr)
            out << ' ' << *(gi->term);
        else
            out << ' ' << gi;
    }
    // outputs
    if (g.outputs.size() > 0)
        std::cout << std::endl << "   out =>";
    for (GatePtr go : g.outputs){
        if(go->term != nullptr)
            out << ' ' << *(go->term);
        else
            out << ' ' << go;
    }
    return out;
}
