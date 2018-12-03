//
//  GateFactory.cpp
//  playerGGP
//
//  Created by Dexter on 01/12/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#include <assert.h>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include "GateFactory.h"
#include "test_globals_extern.h"

using std::set;
using std::string;
using std::stringstream;
using std::ofstream;
using std::cout;
using std::cerr;
using std::endl;

/*******************************************************************************
 *      GateFactory
 ******************************************************************************/

GateFactory::~GateFactory() {
    // delete the Gates allocated and stored in the dictionnary
    for(GatePtr g : gatesSet) {
        delete g;
    }
    // delete the remaining thrown gates
    emptyTrash();
}

/*******************************************************************************
 *      Create different Gate
 ******************************************************************************/

/* create FACT gate */
GatePtr GateFactory::getLatch(TermPtr t) {
    return getOrAddGate(Gate::FACT, t, SetGatePtr());
}

/* create WIRE gate */
GatePtr GateFactory::getWire(TermPtr t, GatePtr g) {
    SetGatePtr in;
    in.insert(g);
    return getOrAddGate(Gate::WIRE, t, std::move(in));
}

/* create anonymous AND gate */
GatePtr GateFactory::getAnd(SetGatePtr&& in) {
    return getOrAddGate(Gate::AND, nullptr, std::move(in));
}

/* create anonymous OR gate */
GatePtr GateFactory::getOr(SetGatePtr&& in) {
    return getOrAddGate(Gate::OR, nullptr, std::move(in));
}

/* create anonymous NOT gate */
GatePtr GateFactory::getNot(GatePtr g) {
    SetGatePtr in;
    in.insert(g);
    return getOrAddGate(Gate::NOT, nullptr, std::move(in));
}

/* create anonymous gate */
GatePtr GateFactory::getOrAddGate(Gate::Type ty, SetGatePtr&& in) {
    return getOrAddGate(ty, nullptr, std::move(in));
}

/* get a Gate in gatesDict if it exist, otherwise create a new Gate */
GatePtr GateFactory::getOrAddGate(Gate::Type ty, TermPtr t, SetGatePtr&& in) {
    if((ty == Gate::AND || ty == Gate::OR) && in.size() == 1)
        return *in.begin();
    GatePtr gate = new Gate(ty, t, std::move(in));
    auto it = gatesSet.find(gate);
    if (it != gatesSet.end()) {
        delete gate;
        return *it;
    }
    auto res = gatesSet.insert(gate);
    assert(res.second);
    for (GatePtr inp : gate->inputs) { // creates a backlink
        assert(inp->type != Gate::NONE);
        inp->outputs.insert(*res.first);
    }
    return *res.first;
}

/*******************************************************************************
 *      Replace Gate by another one
 ******************************************************************************/

void GateFactory::replace(GatePtr to_change, Gate::Type type, TermPtr term, SetGatePtr&& inputs) {

    //cout << "remplace " << *to_change << endl;

    // remove back-links to inputs
    for (GatePtr input : to_change->inputs)
        input->outputs.erase(to_change);
    // check if the gate to_change is effectively in the gatesSet and erase it
    auto it_to_change = gatesSet.find(to_change);
    assert(it_to_change != gatesSet.end() && to_change == *it_to_change);
    gatesSet.erase(it_to_change);
    // create a new gate
    Gate new_gate(type, term, std::forward<SetGatePtr>(inputs));
    if((type == Gate::AND || type == Gate::OR) && new_gate.inputs.size() == 1)
        new_gate = **new_gate.inputs.begin();

    //cout << "with "  << new_gate << endl;

    // swap both but keep the outputs
    to_change->swapExceptOutputs(new_gate);
    // insert the updated gate in the gatesSet and get the same or another pre-existing gate
    auto res = gatesSet.insert(to_change);
    
    // do new_gate existed before? In this case update the pre-existing outputs
    if (!res.second) {
        // realy the same gate ?
        assert(to_change->hash == (*res.first)->hash && *to_change == **res.first);
        GatePtr res_gate = *res.first;
        // keep the term
        if (res_gate->term == nullptr)
            res_gate->term = to_change->term;
        // change all the outputs of to_change
        VectorGatePtr outputs(to_change->outputs.begin(), to_change->outputs.end());
        for(GatePtr out : outputs) {
            SetGatePtr inp(out->inputs);
            inp.erase(to_change);     // replace to_change
            inp.insert(*res.first);   // by the new gate
            replace(out, out->type, out->term, std::move(inp)); // replace this output
            // TODO : BUG ICI DANS /base/amazonsTorus_10x10_v0.kif, cubicup_3player_v0.kif PRESENCE D'UN CYCLE..
            // DONE : on utilise l'adresse des entrées et non leur hash, l'update n'est necessaire que sur un seul niveau 
        }
        // the address of the old gate can still be used (ex : ticTacToeParallel)
        // don't delete it now and keeps track of its replacement
        to_change->type = Gate::NONE;         // marked as garbage
        to_change->hash = reinterpret_cast<size_t>(to_change);
        to_change->outputs.clear();
        to_change->inputs.clear();
        to_change->inputs.insert(res_gate); // its replacement
        trash.push(to_change);

    } else { // new gate : we need restore the back links and to update the outputs
        GatePtr res_gate = *res.first;
        // restore the back links
        for (GatePtr inp : res_gate->inputs) {
            assert(inp->type != Gate::NONE);
            inp->outputs.insert(*res.first);
        }
        // la clef de hash n'est plus calculée d'après le hash des entrées mais d'après l'adresse : plus besoin de propager la modification, l'adresse n'a pas changé.
//        // need to update the hash-key of all the outputs
//        VectorGatePtr outputs(res_gate->outputs.begin(), res_gate->outputs.end());
//        for(GatePtr out : outputs) {
//            if (out->getType() == Gate::WIRE) continue;
//            SetGatePtr inp(out->inputs);
//            replace(out, out->type, out->term, std::move(inp)); // replace this output   // TODO : BUG ICI DANS /base/atariGoVariant_7x7_v0.kif
//                                                                                         // PRESENCE D'UN CYCLE...
//        }
    }
}

/*******************************************************************************
 *      Clean, factorize dans developp
 ******************************************************************************/

/* empty trash */
void GateFactory::emptyTrash() {
    // delete the remaining trashed gates
    while (!trash.empty()) {
        delete trash.top();
        trash.pop();
    }
}

/* iterate through all gates and erases parts of the propnet
 * which are not connected to the outputs */
bool GateFactory::clean() {
    bool cleaned = false;
    // copy the gates (we can not iterate through the set while we update it)
    std::vector<GatePtr> to_clean;
    to_clean.reserve(gatesSet.size());
    to_clean.assign(gatesSet.begin(), gatesSet.end());
    // iterate through the gates
    for (GatePtr gate : to_clean)
        if (clean(gate))
            cleaned = true;
    emptyTrash();
    return cleaned;
}
/* check if a gate is connected to the outputs, if no erase it recursively */
bool GateFactory::clean(GatePtr gate) {
    if (gate->type == Gate::NONE) // already erased
        return false;
    if(gate->type != Gate::WIRE && gate->outputs.empty() && !gate->inputs.empty()) {
        //cout << "Erase: " << *gate << endl;
        for (GatePtr in : gate->inputs) {
            in->outputs.erase(gate);
            if (in->outputs.empty())
                clean(in);
        }
        gatesSet.erase(gate);
        // don't delete it now and mark this gate as garbage
        gate->type = Gate::NONE;
        gate->hash = reinterpret_cast<size_t>(gate);
        gate->outputs.clear();
        gate->inputs.clear();
        trash.push(gate);
        return true;
    }
    return false;
}

/* merge similar gates */
bool GateFactory::merge() {
    bool merged = false;
    // copy the gates (we can not iterate through the set while we update it)
    std::vector<GatePtr> to_merge;
    to_merge.reserve(gatesSet.size());
    to_merge.assign(gatesSet.begin(), gatesSet.end());
    // iterate through the gates
    for (GatePtr gate : to_merge) {
        Gate::Type type;
        TermPtr term;
        SetGatePtr inputs;
        SetGatePtr to_clean;
        if (merge(gate, type, term, inputs, to_clean)) {
            merged = true;
            replace(gate, type, term, std::move(inputs));
            for (GatePtr g : to_clean)
                clean(g);
        }
    }
    return merged;
}
/* factorize negations with morgan law (AND or OR with only negated inputs) */
bool GateFactory::merge(GatePtr gate, Gate::Type& new_type, TermPtr& term, SetGatePtr& inputs, SetGatePtr& to_clean) {
    if (gate->type == Gate::AND || gate->type == Gate::OR) {
        set<GatePtr> inputs_same_type;
        for (GatePtr in : gate->inputs) {
            // don't merge inputs which are named subexpressions
            if (in->type == gate->type && in->getTerm() == nullptr)
                inputs_same_type.insert(in);
        }
        if (inputs_same_type.size() > 0) {
            new_type = gate->type;
            term = gate->term;
            inputs.insert(gate->inputs.begin(), gate->inputs.end());
            for (GatePtr in : inputs_same_type) {
                inputs.erase(in);
                to_clean.insert(in);
                inputs.insert(in->inputs.begin(), in->inputs.end());
            }
            return true;
        }
    }
    return false;
}

/* factorize the negations into the propnet */
bool GateFactory::factorizeNegations() {
    bool factorized = false;
    // copy the gates (we can not iterate through the set while we update it)
    std::vector<GatePtr> to_factorize;
    to_factorize.reserve(gatesSet.size());
    to_factorize.assign(gatesSet.begin(), gatesSet.end());
    // iterate through the gates
    for (GatePtr gate : to_factorize) {
        Gate::Type type;
        TermPtr term;
        SetGatePtr inputs;
        SetGatePtr to_clean;
        SetGatePtr to_merge;
        if (factorizeNegations(gate, type, term, inputs, to_clean, to_merge)) {
            factorized = true;
            replace(gate, type, term, std::move(inputs));
            for (GatePtr g : to_clean)
                clean(g);
            // merge with inputs if the new gate "to_merge" as the same type
            for (GatePtr g_to_merge : to_merge) {
                Gate::Type type2;
                TermPtr term2;
                SetGatePtr inputs2;
                SetGatePtr to_clean2;
                if (merge(g_to_merge, type2, term2, inputs2, to_clean2)) {
                    replace(g_to_merge, type2, term2, std::move(inputs2));
                    for (GatePtr g : to_clean2)
                        clean(g);
                }
            }
        }
    }
    return factorized;
}
/* factorize negations with morgan law (AND or OR with only negated inputs) */
bool GateFactory::factorizeNegations(GatePtr gate, Gate::Type& new_type, TermPtr& term, SetGatePtr& inputs, SetGatePtr& to_clean, SetGatePtr& to_merge) {
    // factorize if the gate has at least 2 negated inputs
    int count = 0;
    for (GatePtr in : gate->inputs) {
        if (in->type == Gate::NOT)
            ++count;
    }
    if (count == 1 && gate->type == Gate::NOT) {
        to_merge = gate->outputs;
        GatePtr replacement = (*(*gate->inputs.begin())->inputs.begin());
        new_type = replacement->getType();
        term = replacement->getTerm();
        inputs = replacement->getInputs();
        to_clean.insert(*gate->inputs.begin());
        return true;
    }
    else if (count > 1) {
        // get the negated and other inputs
        set<GatePtr> inputs_without_not;
        for (GatePtr in : gate->inputs) {
            if (in->type == Gate::NOT) {
                inputs_without_not.insert(*in->inputs.begin());
                to_clean.insert(in);
            } else {
                inputs.insert(in);
            }
        }
        // get type and inverse type AND/OR or OR/AND
        Gate::Type inv_type = (gate->type == Gate::AND)? Gate::OR : Gate::AND;
        term = gate->term;
        if (inputs.size() == 0) { // no remaining inputs
            to_merge.insert(getOrAddGate(inv_type, std::move(inputs_without_not)));
            inputs.insert(*to_merge.begin());
            new_type = Gate::NOT;
        } else { // keep the gate with the remaining inputs
            set<GatePtr> new_inputs;
            to_merge.insert(getOrAddGate(inv_type, std::move(inputs_without_not)));
            new_inputs.insert(*to_merge.begin());
            inputs.insert(getOrAddGate(Gate::NOT, std::move(new_inputs)));
            new_type = gate->type;
            to_clean.insert(gate);
        }
        return true;
    }
    return false;
}

/* factorize the propnet */
bool GateFactory::factorize() {
    bool factorized = false;
    // copy the gates (we can not iterate through the set while we update it)
    std::vector<GatePtr> to_factorize;
    to_factorize.reserve(gatesSet.size());
    to_factorize.assign(gatesSet.begin(), gatesSet.end());
    // iterate through the gates
    for (GatePtr gate : to_factorize) {
        Gate::Type type;
        TermPtr term;
        SetGatePtr inputs;
        SetGatePtr to_clean;
        if (factorize(gate, type, term, inputs, to_clean)) {
            factorized = true;
            replace(gate, type, term, std::move(inputs));
            for (GatePtr g : to_clean)
                clean(g);
            // merge with output if it has the same type
            while (gate->type == Gate::NONE)
                gate = *gate->inputs.begin();
            
            std::vector<GatePtr> to_merge;
            to_merge.reserve(gate->outputs.size());
            to_merge.assign(gate->outputs.begin(), gate->outputs.end());
            
            for (GatePtr out : to_merge) {
                Gate::Type type2;
                TermPtr term2;
                SetGatePtr inputs2;
                SetGatePtr to_clean2;
                if (merge(out, type2, term2, inputs2, to_clean2)) {
                    replace(out, type2, term2, std::move(inputs2));
                    for (GatePtr g : to_clean2)
                        clean(g);
                }
            }
        }
    }
    return factorized;
}
/* factorize a gate (AND or OR with at least 2 inputs) */
bool GateFactory::factorize(GatePtr gate, Gate::Type& new_type, TermPtr& term, SetGatePtr& inputs, SetGatePtr& to_clean) {
    // factorize only AND or OR gates with at least 2 inputs
    if ((gate->type != Gate::AND && gate->type != Gate::OR) || gate->inputs.size() < 2)
        return false;
    // get type and inverse type AND/OR or OR/AND
    Gate::Type type = gate->type;
    Gate::Type inv_type = (type == Gate::AND)? Gate::OR : Gate::AND;
    // get for each input of the inputs the inputs which own them
    std::unordered_map<GatePtr, SetGatePtr> occurences;
    for (GatePtr in : gate->inputs) {
        if(in->type == inv_type) {
            for (GatePtr in_in : in->inputs)
                occurences[in_in].insert(in);
        }
    }
    // set of the most frequent factors
    GatePtr most_used = nullptr;
    size_t max_used = 1;
    for (const auto& entry : occurences) {
        if (entry.second.size() > max_used) {
            max_used = entry.second.size();
            most_used = entry.first;
        }
    }
    // if no factor is found, give up
    if (most_used == nullptr)
        return false;
    
    // get new inputs without the factor
    set<GatePtr> yes;                // inputs with the factor removed
    set<GatePtr> no = gate->inputs;  // inputs which does not used the factor
    for(GatePtr in : occurences[most_used]) {
        no.erase(in);
        to_clean.insert(in);
        set<GatePtr> args;
        for(GatePtr in_in : in->inputs)
            if(in_in != most_used)
                args.insert(in_in);
        yes.insert(getOrAddGate(inv_type, std::move(args)));
    }
    // create a new gate to combine factorized inputs and the factor
    set<GatePtr> args;
    args.insert(most_used);
    args.insert(getOrAddGate(type, std::move(yes)));
    term = gate->term;
    // if there isn't other inputs into no, we don't need another gate
    if (no.empty()) {
        new_type = inv_type;
        inputs = args;
    } else {
        no.insert(getOrAddGate(inv_type, std::move(args)));
        new_type = type;
        inputs = no;
    }
    return true;
}

/* transform the gates in two inputs gates */
bool GateFactory::twoInputs() {
    bool transformed = false;
    // copy the gates (we can not iterate through the set while we update it)
    std::vector<GatePtr> to_split;
    to_split.reserve(gatesSet.size());
    to_split.assign(gatesSet.begin(), gatesSet.end());
    // iterate through the gates
    for (GatePtr gate : to_split) {
        Gate::Type type;
        TermPtr term;
        SetGatePtr inputs;
        SetGatePtr to_clean;
        if (twoInputs(gate, type, term, inputs, to_clean)) {
            transformed = true;
            replace(gate, type, term, std::move(inputs));
            for (GatePtr g : to_clean)
                clean(g);
        }
    }
    return transformed;
}
/* transform the gate in two inputs gates */
bool GateFactory::twoInputs(GatePtr gate, Gate::Type& type, TermPtr& term, SetGatePtr& inputs, SetGatePtr& to_clean) {
    if(gate->inputs.size() <= 2 || (gate->type != Gate::AND && gate->type != Gate::OR))
        return false;
    inputs = gate->inputs;
    while(inputs.size() > 2) {
        set<GatePtr> new_inputs;
        size_t i;
        auto it = inputs.begin();
        for(i = 0; i + 1 < inputs.size(); i += 2) {
            set<GatePtr> args;
            args.insert(*it);
            ++it;
            args.insert(*it);
            ++it;
            new_inputs.insert(getOrAddGate(gate->type, std::move(args)));
        }
        if(i < inputs.size())
            new_inputs.insert(*it);
        std::swap(inputs, new_inputs);
    }
    assert(inputs.size() == 2);
    type = gate->type;
    term = gate->term;
    return true;
}

/* debug */
bool GateFactory::twoSimilarGates(std::unordered_set<GatePtr, Hash, Equal> gateSet) {
    for (auto it1 = gateSet.begin(); it1 != gateSet.end(); ++it1) {
        auto it2 = it1;
        ++it2;
        for (; it2 != gateSet.end(); ++it2) {
            if (**it1 == **it2) {
                cout << (*it1)->getHash() << " " << **it1 << endl;
                cout << " idem to " << endl;
                cout << (*it2)->getHash() << " " << **it2 << endl;
                return true;
            }
        }
    }
    return false;
}

/* debug & test */
bool GateFactory::negationsAtInputs() {
    bool something = false;
    // copy the gates (we can not iterate through the set while we update it)
    std::vector<GatePtr> to_explore;
    to_explore.reserve(gatesSet.size());
    to_explore.assign(gatesSet.begin(), gatesSet.end());
    // iterate through the gates
    for (GatePtr gate : to_explore) {
        Gate::Type type;
        TermPtr term;
        SetGatePtr inputs;
        SetGatePtr to_clean;
        SetGatePtr to_merge;
        if (negationsAtInputs(gate, type, term, inputs, to_clean, to_merge)) {
            something = true;
            replace(gate, type, term, std::move(inputs));
            for (GatePtr g : to_clean)
                clean(g);
            // merge with inputs if the new gate "to_merge" as the same type
            for (GatePtr g_to_merge : to_merge) {
                Gate::Type type2;
                TermPtr term2;
                SetGatePtr inputs2;
                SetGatePtr to_clean2;
                if (merge(g_to_merge, type2, term2, inputs2, to_clean2)) {
                    replace(g_to_merge, type2, term2, std::move(inputs2));
                    for (GatePtr g : to_clean2)
                        clean(g);
                }
            }
        }
    }
    return something;
}
bool GateFactory::negationsAtInputs(GatePtr gate, Gate::Type& type, TermPtr& term,
                       SetGatePtr& inputs, SetGatePtr& to_clean, SetGatePtr& to_merge) {
    // transform only NOT gate with something else than a fact at input
    if (gate->type != Gate::NOT || (*gate->inputs.begin())->type == Gate::FACT)
        return false;
    
    to_merge = gate->outputs;
    to_clean.insert(*gate->inputs.begin());
    // double negation
    if ((*gate->inputs.begin())->type == Gate::NOT) {
        GatePtr replacement = (*(*gate->inputs.begin())->inputs.begin());
        type = replacement->getType();
        term = replacement->getTerm();
        inputs = replacement->getInputs();
    }
    // NOT-AND or NOT-OR
    else {
        type = ((*gate->inputs.begin())->type == Gate::AND)? Gate::OR : Gate::AND;
        term = nullptr;
        for (GatePtr in : (*gate->inputs.begin())->inputs) {
            SetGatePtr new_inputs;
            new_inputs.insert(in);
            inputs.insert(getOrAddGate(Gate::NOT, std::move(new_inputs)));
        }
    }
    return true;
}


/*******************************************************************************
 *      Debug
 ******************************************************************************/

size_t GateFactory::countInputs() const {
    size_t count = 0;
    for(GatePtr e : gatesSet)
        count += e->inputs.size();
    return count;
}

size_t GateFactory::countOutputs() const {
    size_t count = 0;
    for(GatePtr e : gatesSet)
        count += e->outputs.size();
    return count;
}

/*******************************************************************************
 *      Print all gates or Create a graphviz file for debug
 ******************************************************************************/

void GateFactory::printGatesSet() const {
    for (GatePtr g : gatesSet)
        cout << *g << endl;
}

/* create a graphViz file repesenting the propnet */
void GateFactory::createGraphvizFile(const string& name) const {
    // create a graphviz .gv file into log/ directory
    string gv_file = log_dir + name + ".gv";
    string ps_file = log_dir + name + ".ps";
    
    ofstream fp(gv_file);
    if (!fp) {
        cerr << "Error in createGraphvizFile: unable to open file ";
        cerr << gv_file << endl;
        exit(EXIT_FAILURE);
    }
    fp << graphvizRepr(name);
    fp.close();
    cout << "graphviz .gv file created, can't create the .ps file ";
    cout << "myself, use this to create .ps file :" << endl;
    cout << "dot -Tps ";
    cout << "-l graphviz-shapes/AndShape.ps ";
    cout << "-l graphviz-shapes/OrShape.ps ";
    cout << "-l graphviz-shapes/NotShape.ps ";
    cout << gv_file << " -o " << ps_file << endl;
}
/* give the graphViz representation of the propnet */
string GateFactory::graphvizRepr(const string& name) const {
    stringstream nodes;
    stringstream links;
    stringstream in_nodes;
    stringstream out_nodes;
    
    for (GatePtr gate : gatesSet) {
        nodes << "    " << graphvizRepr(gate) << endl;
        for (GatePtr in : gate->inputs) {
            links << "    " << graphvizId(in) << " -> ";
            links << graphvizId(gate) << ";" << endl;
        }
        for (GatePtr out : gate->outputs) {
            links << "    " << graphvizId(gate) << " -> ";
            links  << graphvizId(out) << " [color=\"red\" arrowhead=\"inv\"];" << endl;
        }
        if (gate->type == Gate::FACT || (gate->inputs.size() == 0 && (gate->type == Gate::OR || gate->type == Gate::AND)))
            in_nodes << graphvizId(gate) << "; ";
        else if (gate->type == Gate::WIRE)
            out_nodes << graphvizId(gate) << "; ";
    }
    
    stringstream text;
    text << "/*--------------- graphe " << name << " ---------------*/" << endl;
    text << "digraph propnet {" << endl;
    text << "    rankdir=LR;" << endl;
    text << "    size=\"4, 10\"; " << endl;
    text << "    ranksep=\"3 equally\";" << endl;
    text << "    { node [shape=plaintext, fontsize=16];" << endl;
    text << "        \"inputs\" -> \"outputs\";" << endl;
    text << "    }" << endl;
    text << "    { rank = same; \"inputs\";" << endl;
    text << "    " << in_nodes.str();
    text << "    }" << endl;
    text << "    { rank = same; \"outputs\";" << endl;
    text << "    " << out_nodes.str();
    text << "    }" << endl;
    text << nodes.str();
    text << links.str();
    text << "}";
    
    return text.str();
}
/* give the graphViz representation of one gate */
string GateFactory::graphvizRepr(GatePtr gate) const {
    stringstream adresse;
    stringstream node;
    switch(gate->type) {
        case Gate::WIRE:
            node << graphvizId(gate) << " [label=\"" << *gate->term << "\"";
            node << ",shape=hexagon,color=green";
//            if (gate->isWithNot()) node << ",peripheries=3";
//            else
//            if (gate->isMajorSubExpr()) node << ",peripheries=1";
//            else if (gate->isSubExpr()) node << ",style=filled";
            node << "];";
            break;
        case Gate::FACT:
            node << graphvizId(gate) << " [label=\"" << *gate->term << "\",color=darkorange";
//            if (gate->isWithNot()) node << ",peripheries=3";
//            else
//            if (gate->isMajorSubExpr()) node << ",peripheries=1";
//            else if (gate->isSubExpr()) node << ",style=filled";
            node << "];";
            break;
        case Gate::NOT:
            node << graphvizId(gate) << " [label=\"";
            if (gate->term != nullptr) node << *gate->term;
            else if ((*gate->inputs.begin())->term != nullptr)
                node << "not " << *(*gate->inputs.begin())->term;
            else node << gate;
            node << "\",shape=not,peripheries=0,color=purple";
//            if (gate->isWithNot()) node << ",peripheries=3";
//            else
//            if (gate->isMajorSubExpr()) node << ",peripheries=1";
//            else if (gate->isSubExpr()) node << ",style=filled";
            node << "];";
            break;
        case Gate::OR:
            node << graphvizId(gate) << " [label=\"";
            if (gate->term != nullptr) node << *gate->term;
            else node << gate;
            node << "\",shape=or,peripheries=0,color=blue";
//            if (gate->isWithNot()) node << ",peripheries=3";
//            else
//            if (gate->isMajorSubExpr()) node << ",peripheries=1";
//            else if (gate->isSubExpr()) node << ",style=filled";
            node << "];";
            break;
        case Gate::AND:
            node << graphvizId(gate) << " [label=\"";
            if (gate->term != nullptr) node << *gate->term;
            else node << gate;
            node << "\",shape=and,peripheries=0,color=red";
//            if (gate->isWithNot()) node << ",peripheries=3";
//            else
//            if (gate->isMajorSubExpr()) node << ",peripheries=1";
//            else if (gate->isSubExpr()) node << ",style=filled";
            node << "];";
            break;
        default:
            break;
    }
    return node.str();
}
string GateFactory::graphvizId(GatePtr gate) const {
    return std::to_string((long) gate);
}
