//
//  UctSinglePlayer3.cpp
//  playerGGP
//
//  Created by AlineHuf on 25/02/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#include "test_globals_extern.h"
#include "UctSinglePlayer3.hpp"
#include <sys/time.h>
#include <algorithm>
#include <fstream>
#include <sstream>

using std::cout;
using std::cerr;
using std::endl;
using std::pair;
using std::string;
using std::ostream;
using std::stringstream;
using std::ofstream;

/*******************************************************************************
 *      Link3
 ******************************************************************************/

// méthode pour imprimer un lien
ostream& operator<<(ostream& out, const Link3& l) {
    out << "\tNi=" << l.visits << " \tWi=" << l.sum_score << " \tnext_node" << l.next_node << " \tmoves=[ ";
    for (TermPtr m : l.moves)
        out << *m << " ";
    out << "]" << endl;
    return out;
}


/*******************************************************************************
 *      Node3
 ******************************************************************************/

SetTermPtr Node3::getUnexplored(const VectorTermPtr& legals) {
    SetTermPtr unexplored;

    // deux fois plus rapide (d'après banc de test), mais utilise plus de mémoire pour childs_moves
    for (TermPtr l : legals)
        if (childs_moves.find(l) == childs_moves.end())
            unexplored.insert(l);

//    // deux fois plus lent, mais pas de mémoire suplémentaire utilisée
//    unexplored.insert(legals.begin(), legals.end());
//    for (const auto& c : childs)
//        for (TermPtr m : c.moves)
//            unexplored.erase(m);
    return unexplored;
}

Link3Ptr Node3::getChild(TermPtr move) {
    for (Link3Ptr c : childs) {
        auto it = c->moves.find(move);
        if (it != c->moves.end()) return c;
    }
    cerr << "[getChild] unknown child " << endl;
    exit(1);
}

void Node3::backpropagateNotFullyExplored(int level) {
//    cout << level; // pour constater qu'on n'a jamais besoin de retropropager
    this->terminal = false;
    for (auto& entry : this->parent_nodes) {
        if (entry.second->fully_explored) {
            entry.second->fully_explored = false;
            assert(entry.first->nb_fully_explored > 0);
            entry.first->nb_fully_explored--;
            entry.first->backpropagateNotFullyExplored(level+1);
        }
    }
}

Node3Ptr Node3::noteMoveResult(TermPtr move, VectorTermPtr&& pos, size_t depth, bool term, std::unordered_map<size_t, Node3Ptr>& transpo) {
    childs_moves.insert(move);
    for (Link3Ptr c : childs) {
        if (pos == c->next_node->pos) {
            // cette position s'avère ne pas être toujours terminale finalement
            if (c->next_node->terminal && !term)
                c->next_node->backpropagateNotFullyExplored(0);
            c->moves.insert(move);
            return nullptr; // new position already known
        }
    }
    // transition inconnue
    Node3Ptr n;
    Link3Ptr c;
    size_t hash = computeHash(pos, depth);
    auto it = transpo.find(hash);
    // si la position existe déjà dans la table de tranposition
    if (it != transpo.end()) {
        if (it->second->pos != pos || it->second->depth != depth) {
            cerr << "[noteMoveResult] different positions with same hash" << endl;
            exit(1);
        }
        n = it->second;
        // cette position s'avère ne pas être toujours terminale finalement
        if (n->terminal && !term)
            n->backpropagateNotFullyExplored(0);
    } else {
        n = new Node3(std::move(pos), depth, computeHash(pos, depth), term);
        transpo[hash] = n;
        // la position courante était complètement explorée
        if (nb_fully_explored > 0 && nb_fully_explored == childs.size())
            this->backpropagateNotFullyExplored(0);
    }
    c = new Link3(move, n);
    // memorise new parent of this (new) node
    childs.push_back(c);
    n->parent_nodes.insert(pair<Node3Ptr, Link3Ptr>(this, c));
    // si terminal on indique que le lien est déjà complètement exploré
    if (term) {
        c->fully_explored = true;
        nb_fully_explored++;
    }
    return n;
}

size_t Node3::computeHash(VectorTermPtr p, size_t depth) {
    std::sort(p.begin(), p.end(), [](TermPtr x, TermPtr y){ return *x < *y; });
    size_t h = depth;
    int i = 1;
    for (TermPtr e : p) {
        h ^= e->getHash() * i;
        i += 2;
    }
    return h * 11;
}

// méthode pour imprimer un noeud
std::ostream& operator<<(std::ostream& out, const Node3& n) {
    out << "N=" << n.visits << " node=" << &n << " [";
    for (TermPtr e : n.pos)
        out << *e << " ";
    out << "]" << endl;
    for (Link3Ptr l : n.childs) {
        out << *l;
    }
    return out;
}

/*******************************************************************************
 *      UctSinglePlayer3
 ******************************************************************************/

UctSinglePlayer3::UctSinglePlayer3(Circuit& circ, float c)
: circuit(circ), role(circuit.getRoles()[0]), uct_const(c), rand_gen((std::random_device()).operator()()), solution_found(false)
{
    // initialise la racine
    const VectorTermPtr& init_pos = circuit.getInitPos(); // position initiale
    current.resize(circuit.getInfos().values_size, BOOL_F);
    circuit.setPosition(current, init_pos); // vecteur de booleen correspondant
    circuit.terminal_legal_goal(current);   // propage le signal
    bool term = circuit.isTerminal(current);
    assert(!term);   // position initiale non terminale...
    VectorTermPtr legal = circuit.getLegals(current, role); // actions legales filtrées
    VectorTermPtr current_pos = circuit.getPosition(current);
    size_t hash = Node3::computeHash(current_pos, 0);
    root = new Node3(std::move(current_pos), 0, hash, term); // creation du noeud racine
    root->nb_childs = legal.size();
    transpo[hash] = root;
}

pair<bool, int> UctSinglePlayer3::run(int itermax) {
    // lance uct
    for(int i = 1; i <= itermax; i++) {
        selection_expansion();
        simul_backprop();
#if PLAYERGGP_DEBUG == 2
        if (i % 10000 == 0) {
            cout << "#";
        }
#endif
#if PLAYERGGP_DEBUG == 3
        if (i % 1000 == 0 || solution_found) {
            cout << "RUN =======================================" << i << endl;
            cout << "selection :" << endl;
            for (int i = 0; i < descent_node.size(); i++) {
                cout << **descent_move[i]->moves.begin() << " ";
            }
            cout << endl;
            createGraphvizFile("uct_tree_" + std::to_string(i));
        }
        //        if (i % 1 == 0) {
        //            cout << "RUN =======================================" << i << endl;
        //            // etat de l'arbre
        //            for (Node3Ptr n : all_nodes)
        //                cout << *n << endl;
        //        }
#endif

        // solution trouvée
        if(solution_found)
            return pair<bool, size_t>(true, i);
    }
    return pair<bool, size_t>(false, itermax);
}

void UctSinglePlayer3::printCurrent() {
    cout << "current position:" << endl;
    for (const TermPtr t : circuit.getPosition(current))
        cout << *t << " ";
    cout << endl;
}

void UctSinglePlayer3::selection_expansion() {
    while(true) {
        circuit.setPosition(current, root->pos);
        circuit.terminal_legal_goal(current);
        descent_node.clear();
        descent_move.clear();
        if (selection(root)) break;
    }
}
bool UctSinglePlayer3::selection(Node3Ptr cnode) {
    // on ajoute le noeud courant dans la descente
    descent_node.push_back(cnode);
    // un noeud enfant n'est pas exploré : on selectionne le noeud courant
    const VectorTermPtr clegals = circuit.getLegals(current, role);
    SetTermPtr unexplored = cnode->getUnexplored(clegals);
    // l'expansion a reussi on arrête là
    if (expansion(cnode, unexplored)) return true;
    // pas d'expansion, on connaissait en fait toutes les transitions possibles
    // est-ce que toutes ces transitions sont totalement explorées ?
    if (cnode->nb_fully_explored == cnode->childs.size()) {
        if (cnode != root && !descent_move.back()->fully_explored) {
            int count = 0;
            for (Link3Ptr c : cnode->childs)
                count += c->moves.size();
//            if (count != cnode->nb_childs) {
//                // DEBUG : position
//                VectorTermPtr pos = circuit.getPosition(current);
//                cout << "-----------------POSITION" << endl;
//                for (TermPtr t : pos)
//                    cout << *t << " ";
//                cout << endl;
//                // DEBUG : coups légaux
//                cout << "-----------------LEGAL MOVES" << endl;
//                for (TermPtr t : clegals)
//                    cout << *t << " ";
//                cout << endl;
//                // DEBUG : unexplored
//                cout << "-----------------UNEXPLORED" << endl;
//                for (TermPtr t : unexplored)
//                    cout << *t << " ";
//                cout << endl;
//            }
            assert(count == cnode->nb_childs);
            descent_move.back()->fully_explored = true;
            descent_node[descent_node.size() - 2]->nb_fully_explored++;
        }
        if (cnode == root) {
            cerr << "[selection] arbre completement exploré" << endl;
            exit(1);
        }
        return false;
    }
    // sinon selection du meilleurs enfant
    //    cout << "choix du meilleur enfant" << endl;
    Link3Ptr best = nullptr;
    double best_score = -1;
    for (Link3Ptr c : cnode->childs) {
        // si ce noeud est terminal ou completement exploré => on passe
        if (c->next_node->terminal || c->fully_explored) continue;
        double a = (double) c->sum_score / c->visits;
        double b = sqrt(log((double) cnode->visits) / c->visits);
        double score = a + 100 * uct_const * b;
        if(score > best_score) {
            best = c;
            best_score = score;
        }
    }
    // on a trouvé un gagnant ?
    if (best == nullptr) {
        cerr << "[selection] pas d'enfant selectionné" << endl;
        exit(0);
    }
    // on note l'enfant selectionné
    descent_move.push_back(best);
    // on joue le coup pour passer à la position suivante
    //    cout << "meilleurs coup: " << *cnode->childs[best_id].move << endl;
    circuit.setMove(current, *best->moves.begin());
    circuit.next(current);
    // on verifie si on a pas atteint une position terminale
    if (circuit.isTerminal(current)) {
        return true;
    }
    // sinon on continue la descente
    return selection(best->next_node);
}

bool UctSinglePlayer3::expansion(Node3Ptr cnode, SetTermPtr& unexplored) {
    // en choisir une au hasard qui ne mène pas à une position déjà connue de tous
    while (!unexplored.empty()) {
        // en choisir une au hasard
        auto it = unexplored.begin();
        std::advance(it, rand_gen() % unexplored.size());
        //cout << "choisi : " << **it << endl;
        // ou amène-t-elle ?
        VectorBool next = current;
        circuit.setMove(next, *it);
        circuit.next(next);
        VectorTermPtr next_pos = circuit.getPosition(next);
        // si elle amène à une position déjà connue on recommence
        Node3Ptr n = cnode->noteMoveResult(*it, std::move(next_pos), descent_node.size(), circuit.isTerminal(next), transpo);
        if (n == nullptr) {
            unexplored.erase(it);
        } else {
            n->nb_childs = circuit.getLegals(next, role).size();
            // on note l'enfant selectionné et on l'ajoute à la descente
            Link3Ptr child = cnode->getChild(*it);
            descent_move.push_back(child);
            //cout << "joué " << **it << endl;
            // on joue le coup
            current = next;
            return true;
        }
    }
    return false;
}

void UctSinglePlayer3::simul_backprop() {
    // si la position n'est pas terminale, on fait un playout
    if (!circuit.isTerminal(current))
        circuit.playout(current);
    // on récupère le score
    Score score = circuit.getGoal(current, role);
    // on remonte chaque nœud
    for(int i = (int) descent_node.size()-1; i >= 0; i--) {
        // on augmente le nombre de visites du parent et enfant
        descent_node[i]->visits++;
        descent_move[i]->visits++;
        // on note le score
        descent_move[i]->sum_score += score;
    }
    // si on a trouvé le score max, on le note et on le signale globalement
    if(score == 100) solution_found = true;
}

/*******************************************************************************
 *      GraphViz file
 ******************************************************************************/

/* create a graphViz file repesenting the subtrees */
void UctSinglePlayer3::createGraphvizFile(const string& name) const {
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
    cout << gv_file << " -o " << ps_file << endl;
}
/* give the graphViz representation of the propnet */
string UctSinglePlayer3::graphvizRepr(const string& name) const {
    stringstream nodes;
    stringstream links;

    for (const auto& entry : transpo) {
        Node3Ptr n = entry.second;
        nodes << "    " << (long long) n << " [label=\"N=" << n->visits << "\"";
        if (n->childs.size() > 0 && n->nb_fully_explored > 0) {
            int color = ((float) n->nb_fully_explored / n->childs.size() * 6) + 1;
            nodes << ", style=filled, fillcolor=" << color;
        }
        else if (n->terminal) nodes << ", style=filled, fillcolor=7, color=\"#FF0000\"";
        nodes << "]" << endl;
        for (Link3Ptr c : n->childs) {
            links << "    " << (long long) n << " -> " << (long long) c->next_node;
            links << " [label=\"Ni=" << c->visits << " Wi=" << c->sum_score;
            links << " µ=" << std::fixed << std::setprecision(2) << ((float) c->sum_score/c->visits) << " ";
            if (c->moves.size() == 1)
                links << **c->moves.begin();
            else
                links << "(...)";
            links << "\"";
            if (c->fully_explored) links << " color=\"#FF0000\"";
            links << "];" << endl;
        }
    }
    stringstream text;
    text << "/*--------------- graphe " << name << " ---------------*/" << endl;
    text << "digraph propnet {" << endl;
    text << "    node [ colorscheme=\"bugn9\"] " << endl;
    text << "    rankdir=LR; " << endl;
    text << "    size=\"8,10\"; " << endl;
    text << "    fontsize=\"20\"; " << endl;
    text << "    ranksep=\"3 equally\"; " << endl;
    text << nodes.str();
    text << links.str();
    text << "}";

    return text.str();
}


