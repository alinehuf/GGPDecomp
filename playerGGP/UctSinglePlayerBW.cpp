//
//  UctSinglePlayer.cpp
//  playerGGP
//
//  Created by AlineHuf on 25/02/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#include "test_globals_extern.h"
#include "UctSinglePlayerBW.hpp"
#include <sys/time.h>
#include <algorithm>

using std::cout;
using std::cerr;
using std::endl;
using std::pair;

/*******************************************************************************
 *      UctSinglePlayerBW
 ******************************************************************************/

UctSinglePlayerBW::UctSinglePlayerBW(Circuit& circ, std::unordered_set<TermPtr>& bl, std::unordered_set<TermPtr>& wl, float c)
: circuit(circ), role(circuit.getRoles()[0]), uct_const(c), rand_gen((std::random_device()).operator()()), black_list(bl), white_list(wl), solution_found(false)
{
#if PLAYERGGP_DEBUG == 2
    cout << "white_list : ";
    for (TermPtr t : white_list)
        cout << *t << " ";
    cout << endl;
    cout << "black_list : ";
    for (TermPtr t : black_list)
        cout << *t << " ";
    cout << endl;
#endif
    // initialise la racine
    const VectorTermPtr& init_pos = circuit.getInitPos(); // position initiale
    current.resize(circuit.getInfos().values_size, BOOL_F);
    circuit.setPosition(current, init_pos); // vecteur de booleen correspondant
    circuit.terminal_legal_goal(current);   // propage le signal
    assert(!circuit.isTerminal(current));   // position initiale non terminale...

    // joue les coups de la white_liste
    bool terminal = false;
    for (TermPtr t : white_list) {
        // vérifie que l'action est légale
        auto & infos = circuit.getInfos();
        FactId id_l = infos.inv_vars.at(t) - infos.id_does.front() + infos.id_legal.front();
        assert(current[id_l] == BOOL_T);
        // joue l'action
        circuit.setMove(current, t);
        circuit.next(current);
        if (circuit.isTerminal(current)) { terminal = true; break; } // attention on atteint une position terminale !
    }
    // si on a atteint une position terminale...
    if (terminal) {
        Score s = circuit.getGoal(current, role);
        if (s < 100) { // on a pas le score max ?? c'est pas normal...
            cerr << "jouer toutes les actions de la white liste dans l'ordre donné mène à une position terminale sub-optimale" << endl;
            //            circuit.setPosition(current, init_pos); // vecteur de booleen correspondant
            //            circuit.terminal_legal_goal(current);   // propage le signal
            //            break;
            exit(0);
        } else {
            solution_found = true;
        }
    } else { // sinon on a joué les actions de la white_list, on la vide
        white_list.clear();
        VectorTermPtr legal = getFilteredLegals(current, role); // actions legales filtrées
        VectorTermPtr current_pos = circuit.getPosition(current);
        root = new Node(current_pos, Node::computeHash(current_pos), legal); // creation du noeud racine
        transpo[root->hash] = root;
        //        cout << "root: " << *root << endl;
    }
}




pair<bool, int> UctSinglePlayerBW::run(int itermax) {
    // solution trouvée
    if(solution_found) return pair<bool, size_t>(true, 0);

    // lance uct
    for(int i = 1; i <= itermax; i++) {
#if PLAYERGGP_DEBUG == 2
        if (i % 100000 == 0) cout << "#";
#endif
        selection();
        expansion();
        int score = simulation();
        backpropagate(score);
        // solution trouvée
        if(solution_found)
            return pair<bool, size_t>(true, i);
    }
    return pair<bool, size_t>(false, itermax);
}

void UctSinglePlayerBW::printCurrent() {
    cout << "current position:" << endl;
    for (const TermPtr t : circuit.getPosition(current))
        cout << *t << " ";
    cout << endl;
}

VectorTermPtr UctSinglePlayerBW::getFilteredLegals(const VectorBool& values, TermPtr role) {
    const VectorTermPtr& legals = circuit.getLegals(current, role); // actions legales
    VectorTermPtr legal_white;
    VectorTermPtr legal_grey;
    for (TermPtr t : legals) {
        if (white_list.find(t) != white_list.end()) legal_white.push_back(t);
        else if (black_list.find(t) == black_list.end()) legal_grey.push_back(t);
    }
    if (!legal_white.empty()) return legal_white;
    if (!legal_grey.empty()) return legal_grey;
    return legals;
}


void UctSinglePlayerBW::selection() {
    while(true) {
        circuit.setPosition(current, root->pos);
        descent_nptr.clear();
        descent_mid.clear();
        if (selection(root)) break;
    }
}
bool UctSinglePlayerBW::selection(NodePtr cnode) {
    // si la branche a été complètement explorée en passant par un autre chemin
    if (cnode->nb_fully_explored == cnode->childs.size()) {
        if(descent_nptr.size() == 0) {
            cerr << "[selection] arbre completement exploré" << endl;
            exit(0);
        }
        NodePtr previous = descent_nptr.back();
        previous->childs[descent_mid.back()].fully_explored = true;
        previous->nb_fully_explored++;
        // NOTE : revenir en arrière d'un niveau pour selectionner un autre enfant : mauvaise idée à cause de l'empilement des appels
        return false;
        // TODO : on peut remonter la somme des scores et des visites comme résultat de la descente ?
    }
    // on ajoute le noeud courant dans la descente
    descent_nptr.push_back(cnode);
    // un noeud enfant n'est pas exploré : on selectionne le noeud courant
    if (cnode->visits < cnode->childs.size()) return true;
    // sinon selection du meilleurs enfant
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
    circuit.setMove(current, cnode->childs[best_id].move);
    circuit.next(current);
    VectorTermPtr new_pos =  circuit.getPosition(current);
    size_t new_hash = Node::computeHash(new_pos);
    auto it = transpo.find(new_hash);
    if (it != transpo.end()) {
        return selection(it->second);
    } else {
        NodePtr new_node = new Node(new_pos, new_hash, getFilteredLegals(current, role));
        descent_nptr.push_back(new_node); // nouveau nœud sélectionné
        transpo[new_hash] = new_node;     // on place le nœud dans la table de transpo
    }
    return true;
}

void UctSinglePlayerBW::expansion() {
    NodePtr cnode = descent_nptr.back();
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
    circuit.next(current);
}

Score UctSinglePlayerBW::simulation() {
    // si la position est terminale, on note directement (pas de nœud créé)
    if(circuit.isTerminal(current)) {
        descent_nptr.back()->nb_fully_explored++;
        descent_nptr.back()->childs[descent_mid.back()].fully_explored = true;
        //return circuit.getGoal(current, role);
    }
    // sinon on fait un playout
    else {
        circuit.playout(current);
        // on faitr le playout ici pour filtrer les action de la blacklist pendant la descente
        //        auto& infos = circuit.getInfos();
        //        int diff = (int) (infos.id_does[0] - infos.id_legal[0]);
        //        Score goal = 0;
        //        for(;;) {
        //            circuit.compute(Circuit::Info::TERMINAL, current);
        //            if (current[infos.id_terminal]) break;
        //            circuit.compute(Circuit::Info::LEGAL, current);
        //            for(size_t i = 0; i < infos.id_legal.size() - 1; ++i) {
        //                VectorTermPtr legals = getFilteredLegals(current, role);
        //                if(legals.empty()) {
        //                    cerr << "No legal move during playout" << endl;
        //                    exit(0);
        //                }
        //                current[diff + infos.inv_vars.at(legals[rand_gen() % legals.size()])] = BOOL_T;
        //            }
        //            circuit.compute(Circuit::Info::NEXT, current);
        //            memcpy(current.data() + infos.id_true, current.data() + infos.id_next, (infos.id_does.front() - infos.id_true) * sizeof(current.front()));
        //            memset(current.data() + infos.id_does.front(), 0, (infos.id_does.back() - infos.id_does.front()) * sizeof(current.front()));
        //        }
        //        circuit.compute(Circuit::Info::GOAL, current);
        //
        //        RoleId r = infos.inv_roles.at(role);
        //        for(size_t j = infos.id_goal[r]; j < infos.id_goal[r+1]; ++j) {
        //            if(current[j]) {
        //                Score s = stoi(infos.vars[j]->getArgs().back()->getFunctor()->getName());
        //                if (goal != 0) cout << "MORE THAN ONE GOAL FOR ROLE " << *role;
        //                if (goal < s) goal = s;
        //            }
        //        }
        //        return goal;
    }
    return circuit.getGoal(current, role);
}

void UctSinglePlayerBW::backpropagate(Score score) {
    // on remonte chaque nœud
    for(int i = (int) descent_nptr.size()-1; i >= 0; i--) {
        // on augmente le nombre de visites du parent et enfant
        descent_nptr[i]->visits++;
        descent_nptr[i]->childs[descent_mid[i]].visits++;
        // si tous les enfants sont terminaux, on remonte l'info
        if(descent_nptr[i]->nb_fully_explored == descent_nptr[i]->childs.size()) {
            if(i == 0) {
                cout << "[backpropagate] arbre complètement exploré.\n" << endl;
                exit(0);
            }
            descent_nptr[i-1]->nb_fully_explored++;
            descent_nptr[i-1]->childs[descent_mid[i-1]].fully_explored = true;
        }
        // on note le score
        descent_nptr[i]->childs[descent_mid[i]].sum_score += score;
    }
    // si on a trouvé le score max, on le note et on le signale globalement
    if(score == 100) solution_found = true;
}
