//
//  UctSinglePlayerNodeLink.cpp
//  playerGGP
//
//  Created by AlineHuf on 05/03/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#include "Term.h"
#include "UctSinglePlayerNodeLink.hpp"

using std::cout;
using std::cerr;
using std::endl;

/*******************************************************************************
 *      Node
 ******************************************************************************/

Node::Node(const VectorTermPtr& p, size_t h, const VectorTermPtr& moves) : pos(p), hash(h), visits(0), nb_fully_explored(0) {
    // initialiser les liens
    childs.resize(moves.size());
    for (size_t i = 0; i < childs.size(); i++)
        childs[i].move = moves[i];
}

size_t Node::computeHash(VectorTermPtr p) {
    std::sort(p.begin(), p.end(), [](TermPtr x, TermPtr y){ return *x < *y; });
    size_t h = 0;
    int i = 1;
    for (TermPtr e : p) {
        h ^= e->getHash() * i;
        i += 2;
    }
    return h * 11;
}

// méthode pour imprimer un noeud
std::ostream& operator<<(std::ostream& out, const Node& n) {
    out << "position (" << n.hash << "): ";
    for (TermPtr e : n.pos)
        out << *e << " ";
    out << endl;
    out << "visits: " << n.visits << " (fully_explored=" << n.nb_fully_explored << ")" << endl;
    out << "childs:" << endl;
    for (const Link& l : n.childs) {
        out << "move: ";
        if (l.move != nullptr) out << *l.move;
        else out << "-";
        out << " (visits=" << l.visits << " sum_score=" << l.sum_score << " fully_explored=" << l.fully_explored << ")";
        out << endl;
    }
    return out;
}
