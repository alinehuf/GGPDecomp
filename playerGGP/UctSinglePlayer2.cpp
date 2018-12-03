//
//  UctSinglePlayer2.cpp
//  playerGGP
//
//  Created by AlineHuf on 25/02/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#include "test_globals_extern.h"
#include "UctSinglePlayer2.hpp"
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
 *      Node2
 ******************************************************************************/

SetTermPtr Node2::getUnexplored(const VectorTermPtr& legals) {
    SetTermPtr unexplored;
    unexplored.insert(legals.begin(), legals.end());
    for (const auto& c : childs)
        for (TermPtr m : c.moves)
            unexplored.erase(m);
    return unexplored;
}

Link2Ptr Node2::getChild(TermPtr move) {
    for (auto& c : childs) {
        auto it = c.moves.find(move);
        if (it != c.moves.end()) return &c;
    }
    cerr << "[getChild] unknown child " << endl;
    exit(1);
}

Node2Ptr Node2::noteMoveResult(TermPtr move, VectorTermPtr&& pos, bool term) {
    for (Link2& c : childs) {
        if (pos == c.next_node->pos) {
            // cette position s'avère ne pas être toujours terminale finalement
            if (c.next_node->terminal && !term) {
                c.next_node->terminal = false;
                c.fully_explored = false;
                nb_fully_explored--;
            }
            c.moves.insert(move);
            return nullptr; // new position already known
        }
    }
    Node2Ptr n = new Node2(std::move(pos), term);
    childs.push_back(Link2(move, n));
    if (term) { childs.back().fully_explored = true; nb_fully_explored++; }
    return n;
}

// méthode pour imprimer un noeud
std::ostream& operator<<(std::ostream& out, const Node2& n) {
    out << "N=" << n.visits << " node=" << &n << " [";
    for (TermPtr e : n.pos)
        out << *e << " ";
    out << "]" << endl;
    for (const Link2& l : n.childs) {
        out << "Ni=" << l.visits << " Wi=" << l.sum_score << " move=";
        if (l.moves.empty()) out << "-";
        else out << **l.moves.begin();
        out << endl;
    }
    return out;
}

/*******************************************************************************
 *      UctSinglePlayer2
 ******************************************************************************/

UctSinglePlayer2::UctSinglePlayer2(Circuit& circ, float c)
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
    root = new Node2(current_pos, term); // creation du noeud racine
    root->nb_childs = legal.size();
    all_nodes.push_back(root);
}

pair<bool, int> UctSinglePlayer2::run(int itermax) {
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
//            for (Node2Ptr n : all_nodes)
//                cout << *n << endl;
//        }
#endif

        // solution trouvée
        if(solution_found)
            return pair<bool, size_t>(true, i);
    }
    return pair<bool, size_t>(false, itermax);
}

void UctSinglePlayer2::printCurrent() {
    cout << "current position:" << endl;
    for (const TermPtr t : circuit.getPosition(current))
        cout << *t << " ";
    cout << endl;
}

void UctSinglePlayer2::selection_expansion() {
    while(true) {
        circuit.setPosition(current, root->pos);
        circuit.terminal_legal_goal(current);
        descent_node.clear();
        descent_move.clear();
        if (selection(root)) break;
    }
}
bool UctSinglePlayer2::selection(Node2Ptr cnode) {
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
            for (const Link2& c : cnode->childs)
                count += c.moves.size();
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
    Link2Ptr best = nullptr;
    double best_score = -1;
    for (Link2& c : cnode->childs) {
        // si ce noeud est terminal ou completement exploré => on passe
        if (c.next_node->terminal || c.fully_explored) continue;
        double a = (double) c.sum_score / c.visits;
        double b = sqrt(log((double) cnode->visits) / c.visits);
        double score = a + 100 * uct_const * b;
        if(score > best_score) {
            best = &c;
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

bool UctSinglePlayer2::expansion(Node2Ptr cnode, SetTermPtr& unexplored) {
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
        Node2Ptr n = cnode->noteMoveResult(*it, std::move(next_pos), circuit.isTerminal(next));
        if (n == nullptr) {
            unexplored.erase(it);
        } else {
            all_nodes.push_back(n);
            n->nb_childs = circuit.getLegals(next, role).size();
            // on note l'enfant selectionné et on l'ajoute à la descente
            Link2Ptr child = cnode->getChild(*it);
            descent_move.push_back(child);
            //cout << "joué " << **it << endl;
            // on joue le coup
            current = next;
            return true;
        }
    }
    return false;
}

//void UctSinglePlayer2::expansion() {
//    Node2Ptr cnode = descent_nptr.back();
//    // on choisi un des coups pas explorés
//    size_t nb_moves = cnode->childs.size() - cnode->visits;
//    int rmove = rand_gen() % nb_moves;
//    int expansion_id = 0;
//    for(size_t i = 0; i < cnode->childs.size(); i++) {
//        if(cnode->childs[i].visits == 0) {
//            if(rmove == 0) {
//                expansion_id = (int) i;
//                break;
//            }
//            rmove--;
//        }
//    }
//    // on note l'enfant selectionné
//    descent_mid.push_back(expansion_id);
//    // on joue le coup et on l'ajoute à la descente
//    circuit.setMove(current, cnode->childs[expansion_id].move);
//    circuit.next(current);
//}

void UctSinglePlayer2::simul_backprop() {
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
void UctSinglePlayer2::createGraphvizFile(const string& name) const {
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
string UctSinglePlayer2::graphvizRepr(const string& name) const {
    stringstream nodes;
    stringstream links;

    for (Node2Ptr n : all_nodes) {
        nodes << "    " << (long long) n << " [label=\"N=" << n->visits << "\"";
        if (n->childs.size() > 0 && n->nb_fully_explored > 0) {
            int color = ((float) n->nb_fully_explored / n->childs.size() * 6) + 1;
            nodes << ", style=filled, fillcolor=" << color;
        }
        else if (n->terminal) nodes << ", style=filled, fillcolor=7, color=\"#FF0000\"";
        nodes << "]" << endl;
        for (Link2 c : n->childs) {
            links << "    " << (long long) n << " -> " << (long long) c.next_node;
            links << " [label=\"Ni=" << c.visits << " Wi=" << c.sum_score;
            links << " µ=" << std::fixed << std::setprecision(2) << ((float) c.sum_score/c.visits) << " ";
            if (c.moves.size() == 1)
                links << **c.moves.begin();
            else
                links << "(...)";
            links << "\"";
            if (c.fully_explored) links << " color=\"#FF0000\"";
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


