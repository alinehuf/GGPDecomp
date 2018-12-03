//
//  Term.cpp
//  playerGGP
//
//  Created by Dexter on 18/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#include <sstream>
#include "Term.h"

/*******************************************************************************
 *      Term
 ******************************************************************************/

/* create an atom Term */
Term::Term(AtomPtr atom) : functor(atom), args(0), hash(atom->getHash()) {}

/* create a Term : functor + arguments */
Term::Term(AtomPtr atom, VectorTermPtr&& tail) : functor(atom), args(std::forward<VectorTermPtr>(tail)) {
    hash = computeHash(functor, args);
}

bool Term::operator<(Term const& to) const {
    return (this != &to && (hash < to.hash || (hash == to.hash && (functor < to.functor || (functor == to.functor && args < to.args)))));
}

/* compute a hash key for a term (functor + arguments) */
size_t Term::computeHash(AtomPtr atom, const VectorTermPtr& args) {
    size_t hash = atom->getHash();
    int i = 1;
    for(TermPtr t : args) {
        hash ^= t->getHash() * i;
        i += 2;
    }
    if(!args.empty()) hash *= 11;
    return hash;
}

/*******************************************************************************
 *      Print functions for debug
 ******************************************************************************/

/* for debugging, print recursively a Term */
std::ostream& operator<<(std::ostream& out, const Term& t) {
    if (t.args.size() == 0) { // atom
        out << *(t.functor);
    } else {                  // term
        // functor
        out << "(" << *(t.functor);
        // arguments
        for(const TermPtr& arg : t.args) {
            out << " " << *arg;
        }
        out << ")";
    }
    return out;
}

///* for debugging, print recurssively a Term with indentation */
//std::ostream& operator<<(std::ostream& out, const Term& t) {
//    static unsigned int indent;
//    if (t.args.size() == 0) { // atom
//        out << *(t.functor) << " ";
//    } else {                  // term
//        out << std::endl;
//        for (int i = 0; i < indent; ++i) out << "    ";
//        // functor
//        out << "( " << *(t.functor) << " ";
//        // arguments
//        ++indent;
//        for(TermPtr arg : t.args) {
//            out << *arg;
//        }
//        indent--;
//        out << ") ";
//    }
//    return out;
//}



