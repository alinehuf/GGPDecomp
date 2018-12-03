//
//  UctSinglePlayerDecomp2.cpp
//  playerGGP
//
//  Created by AlineHuf on 05/03/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#include "test_globals_extern.h"
#include "UctSinglePlayerDecomp2.hpp"
#include <sys/time.h>
#include <algorithm>
#include <fstream>
#include <sstream>

using std::cout;
using std::cerr;
using std::endl;
using std::pair;
using std::vector;
using std::set;
using std::unordered_set;
using std::string;
using std::ostream;
using std::stringstream;
using std::ofstream;

/*******************************************************************************
 *      SubLink
 ******************************************************************************/

// méthode pour imprimer un lien
ostream& operator<<(ostream& out, const SubLink& l) {
    out << "\tNi=" << l.visits << " \tWi=" << l.sum_score << " \tWi_min=" << l.sum_min << " \tWi_max=" << l.sum_max << " \tnext_node" << l.next_node << " \tmoves=[ ";
    for (const auto& entry : l.moves_explored)
        out << *entry.first << (((entry.second & SubLink::EXPLORED) == SubLink::EXPLORED)?"* ":" ");
    out << "]" << endl;
    return out;
}

bool SubLink::fullyExplored(SetTermPtr legal) {
    for (auto& entry : this->moves_explored)
        if (legal.find(entry.first) != legal.end() && (entry.second & SubLink::EXPLORED) != SubLink::EXPLORED)
            return false;
    return true;
}

int SubLink::nbFullyExplored() {
    int count = 0;
    for (auto& entry : this->moves_explored)
        if ((entry.second & SubLink::EXPLORED) == SubLink::EXPLORED)
            ++count;
    return count;
}

/*******************************************************************************
 *      SubNode
 ******************************************************************************/

// méthode pour imprimer un noeud
ostream& operator<<(ostream& out, const SubNode& n) {
    out << "N=" << n.visits << " node=" << &n << " [ ";
    for (TermPtr t : n.pos)
        out << *t << " ";
    out << "]" << endl;
    for (SubLinkPtr c : n.childs) {
        out << *c;
    }
    return out;
}

bool SubNode::fullyExplored(SetTermPtr legal) {
    for (SubLinkPtr l : childs)
        if (!l->fullyExplored(legal))
            return false;
    return true;
}

int SubNode::nbFullyExplored() {
    int count = 0;
    for (SubLinkPtr l : childs)
        count += l->nbFullyExplored();
    return count;
}

size_t SubNode::computeHash(SetTermPtr p, size_t depth) {
    size_t h = depth;
    int i = 1;
    for (TermPtr e : p) {
        h ^= e->getHash() * i;
        i += 2;
    }
    return h * 11;
}

SubLinkPtr SubNode::getChild(TermPtr move) const {
    for (SubLinkPtr c : childs)
        if (c->moves_explored.find(move) != c->moves_explored.end()) return c;
    cerr << "[getChild] enfant non trouvé !" << endl;
    assert(false);
}

SetTermPtr SubNode::getUnexplored(const VectorTermPtr& legals) {
    SetTermPtr unexplored;

    // deux fois plus rapide (d'après banc de test), mais utilise plus de mémoire pour childs_moves
    for (TermPtr l : legals)
        if (childs_moves.find(l) == childs_moves.end())
            unexplored.insert(l);

    return unexplored;
}

SubNodePtr SubNode::noteMoveResult(TermPtr move, SetTermPtr&& pos, size_t depth, bool term, std::unordered_map<size_t, SubNodePtr>& transpo) {
    childs_moves.insert(move);
    // next position already known ?
    for (SubLinkPtr c : childs) {
        if (pos == c->next_node->pos) {
            auto it = c->moves_explored.find(move);
            if (term) { // action menant à une position terminale
                // si cette actions est terminale et qu'on l'explore pour la première fois, on le note
                if (it == c->moves_explored.end()) c->moves_explored[move] = SubLink::TERMINAL;
                // si cette actions est connue et qu'elle est terminale cette fois: on ajoute le flag "parfois terminal"
                // (ça change rien si elle était déjà toujours ou parfois terminale)
                else it->second |= SubLink::SOMETIME_TERMINAL;
            }
            // l'action n'est pas terminale ici
            else {
                // la position suivante était notée terminale (pour cette action ou une autre) mais elle ne l'est pas cette fois: on l'indique
                if (c->next_node->terminal) {
                    c->next_node->terminal = false;
                    //update_d = depth-1;
                }
                if (it == c->moves_explored.end()) {
                    // si la transition était totalement explorée, on rétropropage la mise à jour (à faire avant d'ajouter l'action)
                    // if (c->nbFullyExplored() == c->moves_explored.size())
                    //    update_d = depth-1;
                    c->moves_explored[move] = SubLink::TO_EXPLORE;
                } else if ((it->second & SubLink::TERMINAL) == SubLink::TERMINAL) {
                    // si la transition était totalement explorée, on rétropropage la mise à jour du marquage (à faire avant de modifier le marquage de l'action)
                    // if (c->nbFullyExplored() == c->moves_explored.size())
                    //    update_d = depth-1;
                    c->moves_explored[move] = SubLink::SOMETIME_TERMINAL;
                }
            }
            return nullptr;
        }
    }
    // transition inconnue
    SubNodePtr n;
    SubLinkPtr c;
    size_t hash = computeHash(pos, depth);
    auto it = transpo.find(hash);
    // si la position existe déjà dans la table de tranposition
    if (it != transpo.end()) {
        if (it->second->pos != pos || it->second->depth != depth) {
            cerr << "[noteMoveResult] different positions with same hash" << endl;
            assert(it->second->pos == pos && it->second->depth == depth);
        }
        n = it->second;
        // cette position s'avère ne pas être toujours terminale finalement
        if (!term) n->terminal = false;
    } else {
        n = new SubNode(std::move(pos), depth, hash, term); // position terminale si term
        transpo[hash] = n;
    }
    c = new SubLink(move, n);
    childs.push_back(c);
    // update_d = depth-1;
    // si terminal on indique que l'action est terminale
    if (term) c->moves_explored[move] = SubLink::TERMINAL;

    return n;
}

/*******************************************************************************
 *      UctSinglePlayerDecomp2 - Constructeur
 ******************************************************************************/

UctSinglePlayerDecomp2::UctSinglePlayerDecomp2(Circuit& circ, vector<unordered_set<TermPtr>>&& sg_trues, float c)
: circuit(circ), subgames_trues(std::forward<vector<unordered_set<TermPtr>>>(sg_trues)), transpo(subgames_trues.size()), uct_const(c),
  role(circuit.getRoles()[0]), rand_gen((std::random_device()).operator()()), subgame_terminal(subgames_trues.size(), false),
  subgame_fully_explored(subgames_trues.size(), false), nb_fully_explored(0), solution_found(false), found(0)
  //subgame_solved(subgames_trues.size(), false),  nb_solved(0), found(0),
{
#if PLAYERGGP_DEBUG == 2
    cout << "-------------------------PLAY WITH DECOMPOSITION" << endl;
    for (int i = 0; i < subgames_trues.size(); i++) {
        cout << "subgame" << i+1 << ":" << endl << "\t";
        for (TermPtr t : subgames_trues[i])
            cout << *t << " ";
        cout << endl;
    }
//    cout << "-------------------------ROOT NODES" << endl;
#endif
    // recupère la position initiale
    const VectorTermPtr& init_pos = circuit.getInitPos(); // position initiale
    init_vals.resize(circuit.getInfos().values_size, BOOL_F);
    circuit.setPosition(init_vals, init_pos); // vecteur de booleen correspondant
    // initialise chaque sous-jeu avec les infos du noeud global initial
    subgame_mask.resize(subgames_trues.size());
    for (int s = 0; s < subgames_trues.size(); s++) {
        // masque pour chaque sous-jeu
        subgame_mask[s].resize(circuit.getInfos().id_does.front(), false);
        for (FactId id_t = circuit.getInfos().id_true; id_t < circuit.getInfos().id_does.front(); id_t++)
            if (subgames_trues[s].find(circuit.getInfos().vars[id_t]) != subgames_trues[s].end())
                subgame_mask[s][id_t] = true;
        // position initiale
        SetTermPtr lpos = getSubPos(init_pos, s);
        size_t hash = SubNode::computeHash(lpos, 0);
        root.push_back(new SubNode(std::move(lpos), 0, hash));
        transpo[s][hash] = root.back();
#if PLAYERGGP_DEBUG == 2
//        cout << "root " << s+1 << ": " << *root.back() << endl;
#endif
    }
}

/*******************************************************************************
 *      Methodes auxiliaires
 ******************************************************************************/

SetTermPtr UctSinglePlayerDecomp2::getSubPos(const VectorTermPtr& global_pos, int i) {
    SetTermPtr sub_pos;
    for (TermPtr t : global_pos) {
        if (subgames_trues[i].find(t) != subgames_trues[i].end())
            sub_pos.insert(t);
    }
    return sub_pos;
}

/*******************************************************************************
 *      UctSinglePlayerDecomp2  UCT
 ******************************************************************************/

int iter_now = 0;

pair<bool, int> UctSinglePlayerDecomp2::run(int itermax) {
    // lance uct
    for(iter_now = 1; iter_now <= itermax; iter_now++) {
        std::vector<std::vector<float>> scores = selection_expansion();
        if (scores.empty()) simul_backprop();
        else backprop(scores);

#if PLAYERGGP_DEBUG == 2
        if (iter_now % 1000 == 0) cout << "#";
#endif
#if PLAYERGGP_DEBUG == 2
        if (iter_now % 1000 == 0 || solution_found) {
            cout << "RUN =======================================" << iter_now << endl;
            // selection réalisée
            cout << "-------------------------SELECTION-EXPANSION: " << endl;
            for (int d = 0; d < descent_link.size(); d++) {
                for (SubLinkPtr c : descent_link[d]) {
                    if (c->moves_explored.size() == 1) cout << *c->moves_explored.begin()->first << " ";
                    else cout << "(...) ";
//                    cout << "[ ";
//                    auto it = c->moves_explored.begin();
//                    for (int i = 0; i < 2 && it != c->moves_explored.end(); i++, it++) {
//                        cout << *it->first << " ";
//                    }
//                    if (it != c->moves_explored.end()) cout << "...";
//                    cout << "] ";
                }
                cout << endl;
            }
            cout << endl;
        }
//        if (iter_now % 1000 == 0 || solution_found) {
//            cout << "RUN =======================================" << iter_now << endl;
//            for (size_t s = 0; s < root.size(); s++) {
//                cout << "-------------------------sugame " << s << " best moves :" << endl;
//                SubNodePtr n = root[s];
//                while (n->childs.size() > 0) {
//                    SubLinkPtr best = nullptr;
//                    float best_eval = -1;
//                    for (SubLinkPtr c : n->childs) {
//                        float eval = (float) c->sum_score / c->visits;
////                        cout << "(Wi=" << c->sum_score << "\tNi=" << c->visits << "\tu=" << eval << ") ";
////                        if (c->moves.size() == 1) cout << **c->moves.begin() << " ";
////                        else cout << "(...) ";
////                        cout << endl;
//                        if (eval > best_eval) { best_eval = eval; best = c; }
//                    }
//                    assert(best != nullptr);
////                    cout << " ===> ";
//                    if (best->moves_explored.size() == 1) cout << *best->moves_explored.begin()->first << " ";
//                    else cout << "(...) ";
////                    cout << endl;
//                    n = best->next_node;
//                }
//                cout << endl;
//            }
//        }
        if (iter_now % 1000 == 0 || solution_found) {
            cout << "RUN =======================================" << iter_now << endl;
            for (int s = 0; s < transpo.size(); s++)
                createGraphvizFile("uct_subtree_" + std::to_string(s) + "_" + std::to_string(iter_now), s);
        }
//        if (iter_now % 1 == 0 || solution_found) {
//            cout << "RUN =======================================" << iter_now << endl;
//            // etat de l'arbre
//            for (int s = 0; s < subgames_trues.size(); s++) {
//                cout << "subgame" << s << endl;
//                printSubTree(cout, root[s], 40);
//            }
//        }
#endif
        // solution trouvée
        if(solution_found)
            return pair<bool, size_t>(true, iter_now);
    }
    return pair<bool, size_t>(false, itermax);
}

vector<vector<float>> UctSinglePlayerDecomp2::selection_expansion() {
    while(true) {
        current = init_vals;
        circuit.terminal_legal_goal(current);
        descent_node.clear();
        descent_link.clear();
        descent_move.clear();
        return selection(root);
    }
}
vector<vector<float>> UctSinglePlayerDecomp2::selection(vector<SubNodePtr> cnode) {
    while(true) {
        // on ajoute le noeud courant dans la descente
        descent_node.push_back(cnode);
        // on cherche les legals au niveau global, on en fait un set pour les recherches
        const VectorTermPtr clegals = circuit.getLegals(current, role);
        SetTermPtr set_legals;
        set_legals.insert(clegals.begin(), clegals.end());

        // un noeud enfant n'est pas exploré : on selectionne le noeud courant
        SetTermPtr unexplored;
        for (SubNodePtr n : cnode) {
            SetTermPtr u = n->getUnexplored(clegals);
            unexplored.insert(u.begin(), u.end());
        }
        // l'expansion a reussi on arrête là
        if (expansion(cnode, unexplored)) return vector<vector<float>>();

        // pas d'expansion, on connaissait en fait toutes les transitions possibles
        // collecter les actions terminales pour les distinguer des autres
        // TODO : et si on a marqué terminale une action qui ne l'est pas ? on peut marquer la branche totalement explorée et retourner un score moyen érroné...
        SetTermPtr terminal_moves; // actions terminales
        for (int s = 0; s < cnode.size(); s++)
            for (SubLinkPtr c : cnode[s]->childs)
                if (c->next_node->terminal)
                    for (const auto& entry : c->moves_explored)
                        if (entry.second == SubLink::TERMINAL && set_legals.find(entry.first) != set_legals.end())
                            terminal_moves.insert(entry.first);
        // collecter les actions légales non-terminales
        SetTermPtr legal_moves;    // actions legales non terminales
        for (TermPtr t : set_legals)
            if (terminal_moves.find(t) == terminal_moves.end())
                legal_moves.insert(t);
        // est-ce que toutes les transitions sont totalement explorées ?
        bool all_fully_explored = true;
        for (int s = 0; s < cnode.size(); s++) {
            if (!cnode[s]->fullyExplored(legal_moves))
                all_fully_explored = false;
            else if (cnode != root)
                descent_link.back()[s]->moves_explored[descent_move.back()] |= SubLink::EXPLORED;
            else if (cnode == root && !subgame_fully_explored[s]) {
                cerr << "[selection] subgame " << s << " fully explored" << endl;
                subgame_fully_explored[s] = true;
                nb_fully_explored++;
            }
        }
        if (all_fully_explored) {
            if (cnode == root) {
                cerr << "[selection] tous les sous-arbres completement explorés" << endl;
                assert(cnode != root);
            }
            // estimation du score exact moyen
            vector<vector<float>> scores;
            for (int s = 0; s < cnode.size(); s++) {
                vector<float> scores_local;
                float mean = 0;
                float mean_min = 0;
                float mean_max = 0;
                for (SubLinkPtr l : cnode[s]->childs) {
                    mean += l->sum_score;
                    mean_min += l->sum_min;
                    mean_max += l->sum_max;
                }
                mean /= cnode[s]->visits;
                mean_min /= cnode[s]->visits;
                mean_max /= cnode[s]->visits;
                scores_local.push_back(mean);
                scores_local.push_back(mean_min);
                scores_local.push_back(mean_max);
                scores.push_back(scores_local);
            }
            return scores;
            //return vector<vector<float>>();
        }

        // on va collecter les meilleures actions légales pour chaque sous-jeu
        // collecter les meilleures actions terminales
        TermPtr best_term = nullptr;
        Score best_score_term = -1;
        for (auto it = terminal_moves.begin(); it != terminal_moves.end(); /* pas d'incrementation */) {
            VectorBool next = current;
            circuit.setMove(next, *it);
            circuit.next(next);
            //assert(circuit.isTerminal(next));
            // TODO : A REVOIR, UN PEU MERDIQUE
            // cette action a été marquée terminale, mais elle ne l'est pas toujours.
            if (!circuit.isTerminal(next)) {
                for (int s = 0; s < cnode.size(); s++) {
                    SubLinkPtr c = cnode[s]->getChild(*it);
                    c->next_node->terminal = false;
                    c->moves_explored[*it] = SubLink::SOMETIME_TERMINAL;
                }
                legal_moves.insert(*it);
                it = terminal_moves.erase(it);
                continue;
            }
            // récupérer le score
            Score s = circuit.getGoal(next, role);
            if (s > best_score_term) {
                best_score_term = s;
                best_term = *it;
            }
            if (s == 100) break;
            it++;
        }
        // si aucune action terminale ne rapporte 100 points il faut choisir la meilleure non terminale si il y en a une
        SetTermPtr best_of_all;
        if (best_score_term < 100 && !legal_moves.empty()) {
            // collecter les meilleures actions pour chaque sous-jeu
            SetTermPtr best_moves;
            std::map<TermPtr, float> votes;
            float best_vote = 0;
            for (int s = 0; s < cnode.size(); s++) {
            label:
                SetTermPtr best_local_moves;
                double best_score = -1;
                bool fully_explored = cnode[s]->fullyExplored(legal_moves);
                // pour chaque transition voir si elle possède les meilleures actions légales
                for (SubLinkPtr c : cnode[s]->childs) {
                    //double a = (double) c->sum_score / c->visits;
                    //double a = (double) c->sum_min / c->visits;
                    //double a = (double) c->sum_max / c->visits;
                    //double a = (((double) c->sum_min + (float) c->sum_max) / 2 ) / c->visits;
                    double a = (((double) c->sum_score + (float) c->sum_max) / 2 ) / c->visits;
                    //double a = ((3 * (double) c->sum_score + (float) c->sum_max) / 4 ) / c->visits;
                    //double a = (((double) c->sum_score + 3 * (float) c->sum_max) / 4 ) / c->visits;
                    double b = sqrt(log((double) cnode[s]->visits) / c->visits);
                    double score = a + 100 * uct_const * b;
                    // garder les meilleurs actions si elles sont légales
                    if (score > best_score) { // si meilleur score
                        SetTermPtr best_legal;
                        // collecte des actions actions légales
                        // si l'action est legale et pas totalement explorée ou si elles le sont toutes
                        for (const auto& entry : c->moves_explored)
                            if (legal_moves.find(entry.first) != legal_moves.end() && (fully_explored || (entry.second & SubLink::EXPLORED) != SubLink::EXPLORED))
                               best_legal.insert(entry.first);
                        // on remplace les meilleures si il y a des actions légales
                        if (!best_legal.empty()) {
                            best_score = score;
                            best_local_moves.swap(best_legal);
                        }
                    } else if (score == best_score) { // si score équivalent
                        // on ajoute les actions aux meilleures si elles sont légales
                        for (const auto& entry : c->moves_explored)
                            if (legal_moves.find(entry.first) != legal_moves.end() && (fully_explored || (entry.second & SubLink::EXPLORED) != SubLink::EXPLORED))
                                best_local_moves.insert(entry.first);
                    }
                }
                // on vérifie qu'on a trouvé au moins une action légale
                if (best_local_moves.empty()) {
                    cerr << "[selection] no legal best move in subgame " << s << endl;
                    cout << "selection: ";
                    for (size_t i = 0; i < descent_move.size(); i++)
                        cout << *descent_move[i] << " ";
                    cout << endl;
                    cout << "position courante: ";
                    for (TermPtr t : circuit.getPosition(current))
                        cout << *t << " ";
                    cout << endl;
                    cout << "actions legales: " << endl;
                    for (TermPtr l : clegals) {
                        SubLinkPtr c = cnode[s]->getChild(l);
                        cout << *l << " status = " << (int) c->moves_explored[l] << endl;
                    }
                    assert(!best_local_moves.empty());
                }
                // ajouter ces meilleures actions au vote global
                for (TermPtr t : best_local_moves) {
                    // original, un vote standard : un point par action, chaque sous-jeu a un vote de même poids
                    votes[t]++;

                    // effet inverse de celui recherché.  Comme il y a plus d'actions dans block que maze,
                    // dans maze la transition correspondant au fait de jouer ailleurs est plus souvent visitée.
                    // c->visits / cnode[s]->visits est donc important pour les actions qui consistent à jouer ailleurs.
                    // On ne veut pas donner un vote différent pour les différentes actions du sous-jeu mais donner plus de poids au choix du sous-jeu.
                    // Il faut donc donner juste un score qui dépend du facteur de branchement et du nombre de visites du noeud
                    // (pour qu'un noeud avec un fort facteur de branchement mais souvent visité, donc bien évalué,
                    //  permette de dire que le choix du sous-jeu est fiable)
                    // SubLinkPtr c = cnode[s]->getChild(t);
                    // votes[t] += c->visits / cnode[s]->visits;

                    // pour éviter les problèmes dans Incredible (mouvements dans maze favorisés même quand le jeu est à une action de la fin)
                    // si une action terminale de score non nul est écartée du choix, les votes ont tous le même poids.
                    // if (best_score_term > 0)
                    //    votes[t]++;
                    // else
                    //    votes[t] += cnode[s]->visits / cnode[s]->childs.size();

                    if (votes[t] > best_vote) {
                        best_vote = votes[t];
                        best_moves.clear();
                        best_moves.insert(t);
                    } else if (votes[t] == best_vote) {
                        best_moves.insert(t);
                    }
                }
            }
            // vérification qu'on a bien trouvé les meilleures actions
            if (best_moves.empty()) {
                cerr << "[selection] empty set of best moves" << endl;
                assert(!best_moves.empty());
            }
            // si il y a plusieurs meilleures actions on choisi celle qui apporte le meilleur espoir de gain et qui n'est pas totalement explorée
            float best_proba = -1;
            for (TermPtr t : best_moves) {
                float proba = 1;
                bool fully_explored = true;
                for (int s = 0; s < cnode.size(); s++) {
                    SubLinkPtr c = cnode[s]->getChild(t);
                    if ((c->moves_explored[t] & SubLink::EXPLORED) != SubLink::EXPLORED) fully_explored = false;
                    proba *= ((float) c->sum_score / c->visits) / 100;
                }
                if (fully_explored) continue;
                if (proba > best_proba) {
                    best_proba = proba;
                    best_of_all.clear();
                    best_of_all.insert(t);
                } else if (proba == best_proba) {
                    best_of_all.insert(t);
                }
            }
            // toutes les actions non terminales sont totalement explorées du coup best_of_all est vide
            // TODO : remonter l'évaluation exacte plutôt que choisir au hasard.
            if (best_of_all.empty()) best_of_all.swap(best_moves);
        }
        // il y a une transition terminale de score 100 ? sinon il y a une meilleure action non terminale ? sinon on termine avec un score sous-optimal...
        TermPtr choice = best_term;
        if (best_score_term < 100 && !best_of_all.empty()) {
            // on choisi
            auto it = best_of_all.begin();
            std::advance(it, rand_gen() % best_of_all.size());
            choice = *it;
        }
        // on note l'enfant selectionné et on l'ajoute à la descente
        // on note aussi la position suivante
        vector<SubLinkPtr> subleg;
        vector<SubNodePtr> next_node;
        for (int s = 0; s < cnode.size(); s++) {
            SubLinkPtr child = cnode[s]->getChild(choice);
            subleg.push_back(child);
            next_node.push_back(child->next_node);
        }
        descent_link.push_back(subleg);
        descent_move.push_back(choice);
        // on joue le coup pour passer à la position suivante
        circuit.setMove(current, choice);
        circuit.next(current);
        // on verifie si on a pas atteint une position terminale
        if (circuit.isTerminal(current)) return vector<vector<float>>();
        // sinon on continue la descente
        cnode = next_node;
    }
}

bool UctSinglePlayerDecomp2::expansion(vector<SubNodePtr>& cnode, SetTermPtr& unexplored) {
    // en choisir une au hasard qui ne mène pas à une position déjà connue de tous
   while (!unexplored.empty()) {
        // en choisir une au hasard
        auto it = unexplored.begin();
        std::advance(it, rand_gen() % unexplored.size());
        // ou amène-t-elle ?
        VectorBool next = current;
        circuit.setMove(next, *it);
        circuit.next(next);
        VectorTermPtr next_pos = circuit.getPosition(next);
        // on informe chaque enfant
        bool already_known = true;
        for (int s = 0; s < cnode.size(); s++) {
            SubNodePtr n = cnode[s]->noteMoveResult(*it, getSubPos(next_pos, s), descent_node.size(), circuit.isTerminal(next), transpo[s]);
            if (n != nullptr)
                already_known = false;
        }
        // si elle amène à une position connue de tous on recommence
        if (already_known) {
            unexplored.erase(it);
        } else {
            // on note l'enfant selectionné et on l'ajoute à la descente
            vector<SubLinkPtr> subleg;
            for (int i = 0; i < cnode.size(); i++) {
                SubLinkPtr child = cnode[i]->getChild(*it);
                subleg.push_back(child);
            }
            descent_link.push_back(subleg);
            descent_move.push_back(*it);
            // on joue le coup
            current = next;
            return true;
        }
    }
    return false;
}

void UctSinglePlayerDecomp2::simul_backprop() {
    // si la position n'est pas terminale, on fait un playout
    bool last_terminal = false;
    if (!circuit.isTerminal(current))
        circuit.playout(current);
    else last_terminal = true;
    // on récupère le score
    Score score = circuit.getGoal(current, role);
    // récupérer la position terminale
    const VectorTermPtr& pos = circuit.getPosition(current);
    // récupérer les infos du circuit
    const Circuit::Info& infos = circuit.getInfos();

    // pour chaque sous-jeu
    for (int s = 0; s < (int) subgames_trues.size(); s++) {
        // estimer l'espérance de gain lmin/lmax dans la sous-position correspondant au terminal
        Score lmin = 100, lmax = 0;
        SubLinkPtr c = descent_link.back()[s]; // enfant du dernier noeud selectionné
        // la sous-position existe dans l'arbre et elle est évaluée
        if (last_terminal && c->max != -1) {
            lmax = c->max;
            lmin = c->min;
        } else {
            // sinon on evalue
            SetTermPtr lpos = getSubPos(pos, s);
            VectorBool values(infos.values_size, UNDEF);
            values[0] = BOOL_T;
            for (TermPtr t : subgames_trues[s]) values[infos.inv_vars.at(t)] = BOOL_F;
            for (TermPtr t : lpos) values[infos.inv_vars.at(t)] = BOOL_T;
            circuit.propagate3StatesStrict(values);
            for (FactId g = infos.id_goal.front(); g < infos.id_end; g++) {
                if (values[g] != BOOL_F) {
                    Score gs = stoi(infos.vars[g]->getArgs().back()->getFunctor()->getName());
                    if (gs > lmax) lmax = gs;
                    if (gs < lmin) lmin = gs;
                }
            }
            if (values[infos.id_terminal] == BOOL_T) subgame_terminal[s] = true;
            if (!(lmin <= score && score <= lmax)) {
                for (TermPtr t : pos) cout << *t << " ";
                cout << endl;
                for (int i = 0; i < current.size(); i++)
                    cout << "[" << i << "] " << current[i] << endl;
                cout << endl;
            }
            assert(lmin <= score && score <= lmax);
            if (last_terminal) {
                c->max = lmax;
                c->min = lmin;
            }
        }

        // on remonte chaque nœud
        for(int i = (int) descent_link.size()-1; i >= 0; i--) {
            // on récupère la position selectionnée et l'enfant correspondant au coup joué
            SubNodePtr n = descent_node[i][s];
            SubLinkPtr c = descent_link[i][s];
            // on augmente le nombre de visites du parent
            n->visits++;
            // on augmente le nombre de visites et on note le score de enfant
            c->visits++;
            // contribution du sous-jeu
            c->sum_score += score;
            c->sum_min += lmin;
            c->sum_max += lmax;
        }
    }
    // si on a trouvé le score max, on le note et on le signale globalement
    if (score > found) {
        found = score;
        cout << "iter: " << iter_now << " - best score found: " << found << endl;
    }
    if (score == 100) solution_found = true;
}

void UctSinglePlayerDecomp2::backprop(vector<vector<float>> scores) {
    // pour chaque sous-jeu
    for (int s = 0; s < (int) subgames_trues.size(); s++) {
        // on remonte chaque nœud
        for(int i = (int) descent_link.size()-1; i >= 0; i--) {    // remarque : N liens, N+1 noeuds, dernier noeud totalement exploré pas mis à jour
            // on récupère la position selectionnée et l'enfant correspondant au coup joué
            SubNodePtr n = descent_node[i][s];
            SubLinkPtr c = descent_link[i][s];
            // on augmente le nombre de visites du parent
            n->visits++;
            // on augmente le nombre de visites et on note le score de enfant
            c->visits++;
            // contribution du sous-jeu
            c->sum_score += scores[s][0];
            c->sum_min += scores[s][1];
            c->sum_max += scores[s][2];
        }
    }
}

/*******************************************************************************
 *      Debuggage
 ******************************************************************************/

void UctSinglePlayerDecomp2::printCurrent() {
    cout << "current position:" << endl;
    for (const TermPtr t : circuit.getPosition(current))
        cout << *t << " ";
    cout << endl;
}

void UctSinglePlayerDecomp2::printSubTree(ostream& out, SubNodePtr n, int max_level) {
    set<SubNodePtr> done;
    done.insert(n);
    printSubTree(out, n, 0, max_level, done);
    cout << endl;
}
void UctSinglePlayerDecomp2::printSubTree(ostream& out, SubNodePtr n, int level, int max_level, set<SubNodePtr>& done) {
    if (level > max_level) return;
    string indent;
    for (int i = 0; i < level; i++) indent += "\t";

    out << indent << "N=" << n->visits;
    out << " node=" << n << " [ ";
    for (TermPtr t : n->pos)
        out << *t << " ";
    out << "]" << endl;
    out << indent << "childs:" << endl;
    for (SubLinkPtr c : n->childs) {
        out << indent << " - Ni=" << c->visits << " \tWi=" << c->sum_score << " Wi_min=" << c->sum_min << " Wi_max=" << c->sum_max << " \tnext_node=" << c->next_node << " \tmove=";
        auto it = c->moves_explored.begin();
        for (int i = 0; i < 3 && it != c->moves_explored.end(); i++, it++) {
            out << *it->first << " ";
        }
        if (it != c->moves_explored.end()) out << "...";
        out << endl;

        // affiche le noeud enfant
        auto r = done.insert(c->next_node);
        if (r.second) printSubTree(out, c->next_node, level+1, max_level, done);
    }
}

/*******************************************************************************
 *      GraphViz file
 ******************************************************************************/

/* create a graphViz file repesenting the subtrees */
void UctSinglePlayerDecomp2::createGraphvizFile(const string& name, int subgame) const {
    // create a graphviz .gv file into log/ directory
    string gv_file = log_dir + name + ".gv";
    string ps_file = log_dir + name + ".ps";

    ofstream fp(gv_file);
    if (!fp) {
        cerr << "Error in createGraphvizFile: unable to open file ";
        cerr << gv_file << endl;
        exit(EXIT_FAILURE);
    }
    fp << graphvizRepr(name, subgame);
    fp.close();

    cout << "#graphviz .gv file created, can't create the .ps file ";
    cout << "myself, use this to create .ps file :" << endl;
    cout << "dot -Tps ";
    cout << gv_file << " -o " << ps_file << endl;
}
/* give the graphViz representation of the propnet */
string UctSinglePlayerDecomp2::graphvizRepr(const string& name, int subgame) const {
    stringstream nodes;
    stringstream links;

    for (auto& entry : transpo[subgame]) {
        SubNodePtr n = entry.second;
        nodes << "    " << (long long) n << " [label=\"" << n << " N=" << n->visits << "\"";
        if (n->childs_moves.size() > 0 && n->nbFullyExplored() > 0) {
            int color = ((float) n->nbFullyExplored() / n->childs_moves.size() * 6) + 1;
            nodes << ", style=filled, fillcolor=" << color;
        }
        else if (n->terminal) nodes << ", style=filled, fillcolor=7, color=red";
        nodes << "]" << endl;
        for (SubLinkPtr c : n->childs) {
            links << "    " << (long long) n << " -> " << (long long) c->next_node;
            links << " [label=\"Ni=" << c->visits << " Wi=" << c->sum_score << " Wi_min=" << c->sum_min << " Wi_max=" << c->sum_max;
            links << " µ=" << std::fixed << std::setprecision(2) << ((float) c->sum_score/c->visits) << " ";
            links << " µ2=" << std::fixed << std::setprecision(2) << (((double) c->sum_score + (float) c->sum_max) / 2 ) / c->visits << " ";
            links << " f=" << c->nbFullyExplored() << "/" << c->moves_explored.size() << " ";
            auto it = c->moves_explored.begin();
            for (int i = 0; i < 3 && it != c->moves_explored.end(); i++, it++) {
                links << *it->first << " ";
            }
            if (it != c->moves_explored.end()) links << "...";
            links << "\"";
            if (c->nbFullyExplored() == c->moves_explored.size()) links << ", color=red";
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
