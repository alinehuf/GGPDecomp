//
//  PropnetReasoner.cpp
//  playerGGP
//
//  Created by Dexter on 17/03/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#include "PropnetReasoner.h"

PropnetReasoner::PropnetReasoner(Circuit& circ) : circuit(circ) {
    values.resize(circuit.getInfos().values_size);
}

//PropnetReasoner::PropnetReasoner(const PropnetReasoner& prop_res) : circuit(prop_res.circuit.copy()) {
//    values.resize(circuit.getInfos().values_size);
//}
//
//Reasoner* PropnetReasoner::copy() const {
//    return new PropnetReasoner(*this);
//}

const VectorTermPtr& PropnetReasoner::getRoles() const { return circuit.getRoles(); }
const VectorTermPtr& PropnetReasoner::getInitPos() const { return circuit.getInitPos(); }

void PropnetReasoner::setPosition(const VectorTermPtr& pos) {
    circuit.setPosition(values, pos);
    circuit.terminal_legal_goal(values);
}
const VectorTermPtr& PropnetReasoner::getPosition() const { return circuit.getPosition(values); }
bool PropnetReasoner::isTerminal() const { return circuit.isTerminal(values); }
const VectorTermPtr& PropnetReasoner::getLegals(TermPtr role) const { return circuit.getLegals(values, role); }
void PropnetReasoner::setMove(TermPtr move) { circuit.setMove(values, move); }

void PropnetReasoner::next() {
    circuit.next(values);
}
void PropnetReasoner::playout() { circuit.playout(values); }
Score PropnetReasoner::getGoal(TermPtr role) const { return circuit.getGoal(values, role); }

