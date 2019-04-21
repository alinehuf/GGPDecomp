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

Node2::Node2(const VectorTermPtr& p, size_t h, const VectorTermPtr& moves) : pos(p), hash(h), visits(0), nb_fully_explored(0) {
    // initialiser les liens
    childs.resize(moves.size());
    for (size_t i = 0; i < childs.size(); i++)
        childs[i].move = moves[i];
}

size_t Node2::computeHash(VectorTermPtr p) {
    std::sort(p.begin(), p.end(), [](TermPtr x, TermPtr y){ return *x < *y; });
    size_t h = 0;
    int i = 1;
    for (TermPtr e : p) {
        h ^= e->getHash() * i;
        i += 2;
    }
    return h * 11;
}

/*******************************************************************************
 *      Debuggage
 ******************************************************************************/

// méthode pour imprimer un noeud
std::ostream& operator<<(std::ostream& out, const Node2& n) {
    out << "position (" << n.hash << "): ";
    for (TermPtr e : n.pos)
        out << *e << " ";
    out << endl;
    out << "visits: " << n.visits << " (fully_explored=" << n.nb_fully_explored << ")" << endl;
    out << "childs:" << endl;
    for (const Link2& l : n.childs) {
        out << "move: ";
        if (l.move != nullptr) out << *l.move;
        else out << "-";
        out << " (visits=" << l.visits << " sum_score=" << l.sum_score << " fully_explored=" << l.fully_explored << ")";
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
    assert(!circuit.isTerminal(current));   // position initiale non terminale...
    VectorTermPtr legal = circuit.getLegals(current, role); // actions legales filtrées
    VectorTermPtr current_pos = circuit.getPosition(current);
    root = new Node2(current_pos, Node2::computeHash(current_pos), legal); // creation du noeud racine
    transpo[root->hash] = root;
}


int current_iter2 = 0;
pair<bool, int> UctSinglePlayer2::run(int itermax) {
    // lance uct
    for(current_iter2 = 1; current_iter2 <= itermax; current_iter2++) {

#if PLAYERGGP_DEBUG == 2
        if (current_iter2 % 1000 == 0 || solution_found) {
            cout << "RUN =======================================" << current_iter2 << endl;
        }
#endif

        float score = selection_expansion();

#if PLAYERGGP_DEBUG == 2
        if (current_iter2 % 1000 == 0 || solution_found) {
            cout << "descente :" << endl;
            for (int i = 0; i < descent_mid.size(); i++) {
                cout << *descent_nptr[i]->childs[descent_mid[i]].move << endl;
            }
            createGraphvizFile("uct_tree_" + std::to_string(current_iter2));
        }
#endif

        if (score == -1) score = simulation();
        backpropagate(score);

#if PLAYERGGP_DEBUG == 3
        if (current_iter2 % 1000 == 0 || solution_found) {
            cout << "RUN =======================================" << current_iter2 << endl;
            cout << "-------------------------best moves :" << endl;
            Node2Ptr n = root;
            circuit.setPosition(current, root->pos);
            while (n->childs.size() > 0) {
                TermPtr best = nullptr;
                float best_eval = -1;
                for (Link2& c : n->childs) {
                    float eval = (float) c.sum_score / c.visits;
                    if (eval > best_eval) { best_eval = eval; best = c.move; }
                }
                assert(best != nullptr);
                cout << *best;
                circuit.setMove(current, best);
                circuit.next(current);
                size_t new_hash = Node2::computeHash(circuit.getPosition(current));
                auto it = transpo.find(new_hash);
                if (it != transpo.end()) n = it->second;
                else break;
            }
            cout << endl;
        }
#endif
        // solution trouvée
        if(solution_found)
            return pair<bool, size_t>(true, current_iter2);
    }
    return pair<bool, size_t>(false, itermax);
}

float UctSinglePlayer2::selection_expansion() {
    circuit.setPosition(current, root->pos);
    circuit.terminal_legal_goal(current);
    descent_nptr.clear();
    descent_mid.clear();
    return selection(root);
}
float UctSinglePlayer2::selection(Node2Ptr cnode) {
    while(true) {
//        printCurrent();
        // si la branche a été complètement explorée en passant par un autre chemin
        if (cnode->nb_fully_explored == cnode->childs.size()) {
//            cout << *cnode << endl;
//            cout << "descente (noeud completement exploré) :";
//            for (int i : descent_mid) cout << " " << i;
//            cout << endl;
//            if(descent_nptr.size() == 0) {
//                cerr << "[selection] arbre completement exploré" << endl;
//                exit(0);
//            }
            Node2Ptr previous = descent_nptr.back();
            previous->childs[descent_mid.back()].fully_explored = true;
            previous->nb_fully_explored++;
            // remonter le score moyen comme résultat du playout
            float score = 0;
            for (size_t i = 0; i < cnode->childs.size(); i++)
                score += cnode->childs[i].sum_score;
            return score / cnode->visits;
        }
        // on ajoute le noeud courant dans la descente
        descent_nptr.push_back(cnode);
        // un noeud enfant n'est pas exploré : on selectionne le noeud courant
        if (cnode->visits < cnode->childs.size()) {
            cnode = expansion();
            if (cnode == NULL) {
//                cout << "expansion terminée" << endl;
                return -1;
            }
            else continue;
        }
//        cout << "on continue la selection" << endl;
        // sinon selection du meilleurs enfant
        //    cout << "choix du meilleur enfant" << endl;
        int best_id = -1;
        double best_score = -1;
        for (int i = 0; i < (int) cnode->childs.size(); i++) {
            if(!cnode->childs[i].fully_explored) {
                double a = (double) cnode->childs[i].sum_score / cnode->childs[i].visits;
                double b = sqrt(log((double) cnode->visits) / cnode->childs[i].visits);
                double score = a + 100 * uct_const * b;
                if(score > best_score) {
                    best_id = i;
                    best_score = score;
                }
            }
        }
        // on a trouvé un gagnant ?
        if (best_id == -1) {
            cerr << "[selection] pas d'enfant selectionné" << endl;
            exit(0);
        }
        // on note l'enfant selectionné
        descent_mid.push_back(best_id);
        // on joue le coup pour passer à la position suivante
        //    cout << "meilleurs coup: " << *cnode->childs[best_id].move << endl;
        circuit.setMove(current, cnode->childs[best_id].move);
//        cout << "selection play " << *cnode->childs[best_id].move << endl;
        circuit.next(current);
        if (circuit.isTerminal(current)) return -1;
        VectorTermPtr new_pos =  circuit.getPosition(current);
        size_t new_hash = Node2::computeHash(new_pos);
//        cout << "nouvelle position atteinte - hash=" << new_hash << endl;
        auto it = transpo.find(new_hash);
        assert(it != transpo.end());
        cnode = it->second;
    }
}

Node2Ptr UctSinglePlayer2::expansion() {
    Node2Ptr cnode = descent_nptr.back();
    // on choisi un des coups pas explorés
    size_t nb_moves = cnode->childs.size() - cnode->visits;
    int rmove = rand_gen() % nb_moves;
    int expansion_id = 0;
    for(size_t i = 0; i < cnode->childs.size(); i++) {
        if(cnode->childs[i].visits == 0) {
            if(rmove == 0) {
                expansion_id = (int) i;
                break;
            }
            rmove--;
        }
    }
    // on note l'enfant selectionné
    descent_mid.push_back(expansion_id);
    // on joue le coup et on l'ajoute à la descente
    circuit.setMove(current, cnode->childs[expansion_id].move);
//    cout << "expansion play " << *cnode->childs[expansion_id].move << endl;
    circuit.next(current);
    // si terminal, stop là
    if (circuit.isTerminal(current)) return NULL;
    // sinon on crée la nouvelle position
    VectorTermPtr new_pos =  circuit.getPosition(current);
    size_t new_hash = Node2::computeHash(new_pos);
    auto it = transpo.find(new_hash);
    if (it != transpo.end()) {
//        cout << "transposition trouvée !" << endl;
        return it->second;
    } else {
        Node2Ptr new_node = new Node2(new_pos, new_hash, circuit.getLegals(current, role));
        transpo[new_hash] = new_node;     // on place le nœud dans la table de transpo
//        cout << "nouvelle position créée - pos= " << new_node << "- hash=" << new_hash << endl;
        return NULL;
    }
}

Score UctSinglePlayer2::simulation() {
    // si la position est terminale, on note directement (pas de nœud créé)
    if(circuit.isTerminal(current)) {
        descent_nptr.back()->nb_fully_explored++;
        descent_nptr.back()->childs[descent_mid.back()].fully_explored = true;
    }
    // sinon on fait un playout
    else circuit.playout(current);
    return circuit.getGoal(current, role);
}

void UctSinglePlayer2::backpropagate(Score score) {
    // on remonte chaque nœud
    for(int i = (int) descent_nptr.size()-1; i >= 0; i--) {
        // on augmente le nombre de visites du parent et enfant
        descent_nptr[i]->visits++;
        descent_nptr[i]->childs[descent_mid[i]].visits++;
        // si tous les enfants sont terminaux, on remonte l'info
        if(descent_nptr[i]->nb_fully_explored == descent_nptr[i]->childs.size()) {
            if(i == 0) {
                cout << "[backpropagate] arbre complètement exploré.\n" << endl;
                createGraphvizFile("uct_tree_fully_explored" + std::to_string(current_iter2));
                exit(0);
            }
            descent_nptr[i-1]->nb_fully_explored++;
            descent_nptr[i-1]->childs[descent_mid[i-1]].fully_explored = true;
        }
        // on note le score
        descent_nptr[i]->childs[descent_mid[i]].sum_score += score;
    }
    // indiquer le meilleur score trouvé
    if (score > best_game_score) {
        best_game_score = score;
        cout << "iter: " << current_iter2 << " - best score found: " << best_game_score << endl;
    }
    // si on a trouvé le score max, on le note et on le signale globalement
    if(score == 100) solution_found = true;
}

/*******************************************************************************
 *      Debuggage
 ******************************************************************************/

void UctSinglePlayer2::printCurrent() {
    cout << "current position:" << endl;
    for (const TermPtr t : circuit.getPosition(current))
        cout << *t << " ";
    cout << endl;
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

    for (auto& entry : transpo) {
        Node2Ptr n = entry.second;
        nodes << "    " << n->hash << " [label=\"N=" << n->visits << "\"";
        if (n->childs.size() > 0 && n->nb_fully_explored > 0) {
            int color = ((float) n->nb_fully_explored / n->childs.size() * 6) + 1;
            nodes << ", style=filled, fillcolor=" << color;
        }
        nodes << "]" << endl;
        for (Link2 c : n->childs) {
            VectorBool values(circuit.getInfos().values_size, BOOL_F);
            circuit.setPosition(values, n->pos);
            circuit.terminal_legal_goal(values);
            circuit.setMove(values, c.move);
            circuit.next(values);
            size_t hash = Node2::computeHash(circuit.getPosition(values));
            auto it = transpo.find(hash);
            if (it == transpo.end())
                nodes << "    " << hash << endl;
            links << "    " << n->hash << " -> " << hash;
            links << " [label=\"Ni=" << c.visits << " Wi=" << c.sum_score;
            links << " µ=" << std::fixed << std::setprecision(2) << ((float) c.sum_score/c.visits) << " " << *c.move << "\"";
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


