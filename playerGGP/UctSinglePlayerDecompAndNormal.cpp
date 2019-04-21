//
//  UctSinglePlayerDecompAndNormal.cpp
//  playerGGP
//
//  Created by AlineHuf on 18/04/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#include "test_globals_extern.h"
#include "UctSinglePlayerDecompAndNormal.hpp"
#include <sys/time.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <queue>

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
 *      Link4
 ******************************************************************************/

// méthode pour imprimer un lien
ostream& operator<<(ostream& out, const Link4& l) {
    out << "\tNi=" << l.visits << " \tWi=" << l.sum_score << " \tnext_node" << l.next_node << " \tmoves=[ ";
    for (TermPtr m : l.moves)
        out << *m << " ";
    out << "]" << endl;
    return out;
}

/*******************************************************************************
 *      Node4
 ******************************************************************************/

// méthode pour imprimer un noeud
ostream& operator<<(ostream& out, const Node4& n) {
    out << "N=" << n.visits << " D=" << n.depth << " node=" << &n << " [ ";
    for (TermPtr t : n.pos)
        out << *t << " ";
    out << "]" << endl;
    for (Link4Ptr c : n.childs) {
        out << *c;
    }
    return out;
}

size_t Node4::computeHash(VectorTermPtr p, size_t depth) {
    std::sort(p.begin(), p.end(), [](TermPtr x, TermPtr y){ return *x < *y; });
    size_t h = depth;
    int i = 1;
    for (TermPtr e : p) {
        h ^= e->getHash() * i;
        i += 2;
    }
    return h * 11;
}

Link4Ptr Node4::getChild(TermPtr move) const {
    for (Link4Ptr c : childs)
        if (c->moves.find(move) != c->moves.end()) return c;
    cerr << "[getChild] enfant non trouvé !" << endl;
    exit(1);
}

SetTermPtr Node4::getUnexplored(const VectorTermPtr& legals) {
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


void Node4::backpropagateNotFullyExplored() {
    this->terminal = false;
    for (auto& entry : this->parent_nodes) {
        if (entry.second->fully_explored) {
            entry.second->fully_explored = false;
            assert(entry.first->nb_fully_explored > 0);
            entry.first->nb_fully_explored--;
            entry.first->backpropagateNotFullyExplored();
        }
    }
}

Node4Ptr Node4::noteMoveResult(TermPtr move, VectorTermPtr&& pos, size_t depth, bool term, std::unordered_map<size_t, Node4Ptr>& transpo) {
    childs_moves.insert(move);
    for (Link4Ptr c : childs) {
        if (pos == c->next_node->pos) {
            // cette position s'avère ne pas être toujours terminale finalement
            if (c->next_node->terminal && !term)
                c->next_node->backpropagateNotFullyExplored();
            c->moves.insert(move);
            return nullptr; // new position already known
        }
    }
    // transition inconnue
    Node4Ptr n;
    Link4Ptr c;
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
            n->backpropagateNotFullyExplored();
    } else {
        n = new Node4(std::move(pos), depth, computeHash(pos, depth), term);
        transpo[hash] = n;
        // la position courante était complètement explorée
        if (nb_fully_explored > 0 && nb_fully_explored == childs.size())
            this->backpropagateNotFullyExplored();
    }
    c = new Link4(move, n);
    // memorise new parent of this (new) node
    childs.push_back(c);
    n->parent_nodes.insert(pair<Node4Ptr, Link4Ptr>(this, c));
    // si terminal on indique que le lien est déjà complètement exploré
    if (term) {
        c->fully_explored = true;
        nb_fully_explored++;
    }
    return n;
}

/*******************************************************************************
 *      UctSinglePlayerDecompAndNormal - Constructeur
 ******************************************************************************/

UctSinglePlayerDecompAndNormal::UctSinglePlayerDecompAndNormal(Circuit& circ, vector<unordered_set<TermPtr>>&& sg_trues, float c)
: circuit(circ), subgames_trues(std::forward<vector<unordered_set<TermPtr>>>(sg_trues)), transpo(subgames_trues.size()), uct_const(c),
role(circuit.getRoles()[0]), rand_gen((std::random_device()).operator()()), subgame_terminal(subgames_trues.size(), false),
subgame_fully_explored(subgames_trues.size(), false), nb_fully_explored(0), solution_found(false)
//subgame_solved(subgames_trues.size(), false),  nb_solved(0), found(0),
{
    // recupère la position initiale
    const VectorTermPtr& init_pos = circuit.getInitPos(); // position initiale
    init_vals.resize(circuit.getInfos().values_size, BOOL_F);
    circuit.setPosition(init_vals, init_pos); // vecteur de booleen correspondant
    // initialise chaque sous-jeu avec les infos du noeud global initial
    subgame_mask.resize(subgames_trues.size());
    size_t hash;

    // racine globale
    hash = Node4::computeHash(init_pos, 0);
    VectorTermPtr current_pos(init_pos);
    global_root = new Node4(std::move(current_pos), 0, hash); // creation du noeud racine
    global_transpo[hash] = global_root;

    // racine des sous-arbres
    for (int s = 0; s < subgames_trues.size(); s++) {
        // masque pour chaque sous-jeu
        subgame_mask[s].resize(circuit.getInfos().id_does.front(), false);
        for (FactId id_t = circuit.getInfos().id_true; id_t < circuit.getInfos().id_does.front(); id_t++)
            if (subgames_trues[s].find(circuit.getInfos().vars[id_t]) != subgames_trues[s].end())
                subgame_mask[s][id_t] = true;
        // position initiale
        VectorTermPtr lpos = getSubPos(init_pos, s);
        hash = Node4::computeHash(lpos, 0);
        root.push_back(new Node4(std::move(lpos), 0, hash));
        root.back()->global_nodes.insert(global_root);
        transpo[s][hash] = root.back();
    }

}

/*******************************************************************************
 *      Methodes auxiliaires
 ******************************************************************************/

VectorTermPtr UctSinglePlayerDecompAndNormal::getSubPos(const VectorTermPtr& global_pos, int i) {
    VectorTermPtr sub_pos;
    for (TermPtr t : global_pos) {
        if (subgames_trues[i].find(t) != subgames_trues[i].end())
            sub_pos.push_back(t);
    }
    return sub_pos;
}
vector<Link4Ptr> UctSinglePlayerDecompAndNormal::getBestMoves(Node4Ptr n) {
    vector<Link4Ptr> best_moves;
    double best_score = -1;
    for (Link4Ptr c : n->childs) {
        // si ce noeud est terminal ou completement exploré mais pas les autres, on l'ignore
        if (n->nb_fully_explored < n->childs.size() && (c->next_node->terminal || c->fully_explored)) continue;
        double a = (double) c->sum_score / c->visits;
        double b = sqrt(log((double) n->visits) / c->visits);
        double score = a + 100 * uct_const * b;
        if (score > best_score) {
            best_score = score;
            best_moves.clear();
            best_moves = vector<Link4Ptr>(1, c);
        } else if (score == best_score) {
            best_moves.push_back(c);
        }
    }
    // on a trouvé un gagnant ?
    if (best_moves.empty()) {
        cerr << "[selection] pas d'enfant selectionné" << endl;
        exit(1);
    }

//    // debug
//    cout << "best local score : " << best_score << endl;
//    cout << "best moves: ";
//    for (Link4Ptr l : best_moves)
//        for (TermPtr t : l->moves)
//            cout << *t << " ";
//    cout << endl;


    return best_moves;
}
void UctSinglePlayerDecompAndNormal::intersect(SetTermPtr& s1, const SetTermPtr& s2) {
    auto it1 = s1.begin();
    auto it2 = s2.begin();
    while (it1 != s1.end() && it2 != s2.end()) {
        if (*it1 < *it2) it1 = s1.erase(it1);
        else if (*it1 > *it2) it2++;
        else { it1++; it2++; }
    }
    s1.erase(it1, s1.end());
}

/*******************************************************************************
 *      UctSinglePlayerDecompAndNormal  UCT
 ******************************************************************************/

pair<bool, int> UctSinglePlayerDecompAndNormal::run(int itermax) {
    // lance uct
    for(int i = 1; i <= itermax; i++) {
        selection_expansion();
        simul_backprop();

#if PLAYERGGP_DEBUG == 2
        if (i % 10000 == 0) cout << "#";
#endif
#if PLAYERGGP_DEBUG == 3
        if (i % 1000 == 0 || solution_found) {
//            cout << "RUN =======================================" << i << endl;
//            // selection réalisée
//            cout << "-------------------------SELECTION-EXPANSION: " << endl;
//            for (int d = 0; d < descent_move.size(); d++) {
//                for (Link4Ptr c : descent_move[d]) {
//                    cout << "[ ";
//                    auto it = c->moves.begin();
//                    for (int i = 0; i < 2 && it != c->moves.end(); i++, it++) {
//                        cout << **it << " ";
//                    }
//                    if (it != c->moves.end()) cout << "...";
//                    cout << "] ";
//                }
//                cout << endl;
//            }
//            cout << endl;
//        }
//        if (i % 1000 == 0 || solution_found) {
//            cout << "RUN =======================================" << i << endl;
//            for (size_t s = 0; s < root.size(); s++) {
//                cout << "-------------------------sugame " << s << " best moves :" << endl;
//                Node4Ptr n = root[s];
//                while (n->childs.size() > 0) {
//                    Link4Ptr best = nullptr;
//                    float best_eval = -1;
//                    for (Link4Ptr c : n->childs) {
//                        float eval = (float) c->sum_score / c->visits;
//                        //                        cout << "(Wi=" << c->sum_score << "\tNi=" << c->visits << "\tu=" << eval << ") ";
//                        //                        if (c->moves.size() == 1) cout << **c->moves.begin() << " ";
//                        //                        else cout << "(...) ";
//                        //                        cout << endl;
//                        if (eval > best_eval) { best_eval = eval; best = c; }
//                    }
//                    assert(best != nullptr);
//                    //                    cout << " ===> ";
//                    if (best->moves.size() == 1) cout << **best->moves.begin() << " ";
//                    else cout << "(...) ";
//                    //                    cout << endl;
//                    n = best->next_node;
//                }
//                cout << endl;
//            }
//        }
//        if (i % 10000 == 0 || solution_found) {
//            for (int s = 0; s < transpo.size(); s++)
//                createGraphvizFile("uct_subtree_" + std::to_string(s) + "_" + std::to_string(i), s);
//        }
//        if (i % 1 == 0 || solution_found) {
//            cout << "RUN =======================================" << i << endl;
//            // etat de l'arbre
//            if (i % 1 == 0 || solution_found) {
//                for (int s = 0; s < subgames_trues.size(); s++) {
//                    cout << "subgame" << s << endl;
//                    printSubTree(cout, root[s], 40);
//                }
//            }
        }
#endif
        // solution trouvée
        if(solution_found)
            return pair<bool, size_t>(true, i);
    }
    return pair<bool, size_t>(false, itermax);
}

void UctSinglePlayerDecompAndNormal::selection_expansion() {
    int i = 0;   // a virer
    while(true) {
        //        cout << i << " ";
        current = init_vals;
        circuit.terminal_legal_goal(current);
        descent_node.clear();
        descent_move.clear();
        global_descent_node.clear();
        global_descent_move.clear();
        if (selection(root, global_root, i)) break;
        //        i++;
    }
}
bool UctSinglePlayerDecompAndNormal::selection(vector<Node4Ptr> cnode, Node4Ptr node, int i) {  // troisième argument à virer
    while(true) {
//        if (descent_move.size() > 0 && i > 5) {
//            for (Link4Ptr l : descent_move.back()) {
//                if (l->moves.size() == 1) cout << **l->moves.begin() << " ";
//                else cout << "(...) ";
//            }
//            cout << endl;
//        }
        // on ajoute le noeud courant dans la descente
        descent_node.push_back(cnode);
        global_descent_node.push_back(node);
        // on cherche les legals au niveau global
        const VectorTermPtr clegals = circuit.getLegals(current, role);
        // un noeud enfant n'est pas exploré : on selectionne le noeud courant
        SetTermPtr unexplored;
        for (Node4Ptr n : cnode) {
            SetTermPtr u = n->getUnexplored(clegals);
            unexplored.insert(u.begin(), u.end());
        }
        SetTermPtr global_unexplored = node->getUnexplored(clegals);
//        cout << "-------------------UNEXPLORED" << endl;
//        for (TermPtr t : unexplored)
//            cout << *t << " ";
//        cout << endl;
        for (TermPtr t : global_unexplored) {
            if (unexplored.find(t) == unexplored.end()) {
//                cout << *t << " explored in subgames not in global game" << endl;
                // ajouter au jeu global
                // ou amène-t-elle ?
                VectorBool next = current;
                circuit.setMove(next, t);
                circuit.next(next);
                VectorTermPtr next_pos = circuit.getPosition(next);
                Node4Ptr gn = node->noteMoveResult(t, std::move(next_pos), global_descent_node.size(), circuit.isTerminal(next), global_transpo);
                for (int s = 0; s < cnode.size(); s++) {
                    auto& sc = cnode[s]->getChild(t)->next_node->global_nodes;
                    if (gn != nullptr) {
                        sc.insert(gn);
                        gn->nb_childs = circuit.getLegals(next, role).size();
                    } else {
                        gn = node->getChild(t)->next_node;
                        assert(sc.find(gn) != sc.end());
                    }
                }
            }
        }

        // l'expansion a reussi on arrête là
        if (expansion(cnode, node, unexplored)) return true;
        // pas d'expansion, on connaissait en fait toutes les transitions possibles
        // completement exploré au niveau global
        bool global_fully_explored = false;
        if (node->nb_fully_explored == node->childs.size()) {

            cout << "----------------------POSITIONS" << endl;
            cout << "-----------GLOBAL GAME " << endl;
            cout << *node << endl;

            if (node != global_root && !global_descent_move.back()->fully_explored) {
                int count = 0;
                for (Link4Ptr c : node->childs)
                    count += c->moves.size();
                assert(count == node->nb_childs);
                global_descent_move.back()->fully_explored = true;
                global_descent_node[global_descent_node.size() - 2]->nb_fully_explored++;
                global_fully_explored = true;
            }
        }
        // est-ce que toutes ces transitions sont totalement explorées au niveau local ?
        bool all_fully_explored = true;
        for (int s = 0; s < cnode.size(); s++) {
            if (cnode[s]->nb_fully_explored < cnode[s]->childs.size())
                all_fully_explored = false;
            else if (cnode != root && !descent_move.back()[s]->fully_explored) {

                cout << "----------------------POSITIONS" << endl;
                    cout << "-----------SUBGAME " << s << endl;
                    cout << *cnode[s] << endl;

                // vérification graphique par rapport au jeu complet
                bool fe = false;
                for (Node4Ptr gn : cnode[s]->global_nodes) {
                    int count = 0;
                    for (Link4Ptr c : gn->childs)
                        count += c->moves.size();
                    if (count == gn->nb_childs) fe = true;
                }
                if (!fe) {
                    createGraphvizFileFromPos("subnode_fully_explored_" + std::to_string(s), cnode[s]);
                    int i = 0;
                    for (Node4Ptr gn : cnode[s]->global_nodes) {
                        createGraphvizFileFromPos("including_subnode_" + std::to_string(s) + "_global_node_" + std::to_string(i), gn);
                        i++;
                    }
                    cout << "pas de noeud global completement exploré" << endl;
                }
                // marquage localement
                descent_move.back()[s]->fully_explored = true;
                descent_node[descent_node.size() - 2][s]->nb_fully_explored++;
            }
            else if (cnode == root && !subgame_fully_explored[s]) {
                cerr << "[selection] subgame " << s << " fully explored" << endl;
                subgame_fully_explored[s] = true;
                nb_fully_explored++;
            }
        }
        // une position totalement explorée globalement doit aussi l'être localement ?
        if (global_fully_explored && !all_fully_explored) {
            cout << "position totalement explorée globalement mais pas localement !" << endl;
            ;
        }
        // completement exploré localement
        if (all_fully_explored) {
            if (cnode == root) {
                cerr << "[selection] tous les sous-arbres completement explorés" << endl;
                exit(1);
            }
            return false;
        }
        // collecte des meilleurs coups/meilleures transitions
        SetTermPtr set_legals;
        set_legals.insert(clegals.begin(), clegals.end());
        std::map<TermPtr, vector<Link4Ptr>> votes;
        SetTermPtr best_moves;
        size_t best_vote = 0;
        for (int s = 0; s < cnode.size(); s++) {
//            cout << "best moves for subgame " << s << endl;
            vector<Link4Ptr> selected =  getBestMoves(cnode[s]);
            // voter pour les meilleurs coups du sous-jeu si ils sont légaux
            bool one_legal = false;
            for (Link4Ptr l : selected) {
                for (TermPtr t : l->moves) {
                    if (set_legals.find(t) != set_legals.end()) {
                        one_legal = true;
                        votes[t].push_back(l);
                        if (votes[t].size() > best_vote) {
                            best_vote = votes[t].size();
                            best_moves.clear();
                            best_moves.insert(t);
                        } else if (votes[t].size() == best_vote) {
                            best_moves.insert(t);
                        }
                    }
                }
            }
            if (!one_legal) {
                cerr << "[selection] no legal best move in subgame " << s << endl;
                exit(1);
            }
        }
        if (best_moves.empty()) {
            cerr << "[selection] empty set of best moves" << endl;
            exit(1);
        }

//        for (auto& entry : votes)
//            cout << *entry.first << " = " << entry.second.size() << " votes" << endl;

        // choisir une transition qui soit non terminale ou bien qui apporte le score max
        TermPtr choice;
        if (best_moves.size() > 1) {
            SetTermPtr best_choice;
            float best_proba = -1; // probabilité de gain annoncée par les sous-jeux qui ont recommandé l'action
            SetTermPtr best_terminal;
            float best_term_score = -1;
            for (TermPtr t : best_moves) {
                // est-ce que cette action amène à une position terminale ? pas completement explorée ? et quelle est la proabilité de gain ?
                bool term = false;
                bool fully_explored = true;
                float proba = 1;
                for (Link4Ptr l : votes[t]) {
                    if (l->next_node->terminal) term = true;
                    if (!l->fully_explored) fully_explored = false;
                    proba *= ((float) l->sum_score / l->visits) / 100;
                }
                // si la position suivante n'est pas terminale et pas totalement explorée
                if (!term && !fully_explored) {
                    if (proba > best_proba) {
                        best_proba = proba;
                        best_choice.clear();
                        best_choice.insert(t);
                    } else if (proba == best_proba) {
                        best_choice.insert(t);
                    }
                }
                // cette action amène à une position terminale
                else if (term) {
                    VectorBool next = current;
                    circuit.setMove(next, t);
                    circuit.next(next);
                    assert(circuit.isTerminal(next));
                    Score s = circuit.getGoal(next, role);
                    if (s > best_term_score) {
                        best_term_score = s;
                        best_terminal.clear();
                        best_terminal.insert(t);
                    } else if (s == best_term_score) {
                        best_terminal.insert(t);
                    }
                }
            }
            // il y a une transition terminale de score 100 ?  sinon il y a une transition non terminale ? sinon on prend ce qui reste...
            SetTermPtr * list = &best_terminal;
            if (best_term_score < 100 && !best_choice.empty()) list = &best_choice;



//            if (list == &best_terminal) {
//                cout << "meilleures actions terminales (score=" << best_term_score << ") ";
//                for (TermPtr t : best_terminal) cout << *t << " ";
//                cout << endl;
//            } else {
//                cout << "meilleures actions non terminales (proba=" << best_proba << ") ";
//                for (TermPtr t : best_choice) cout << *t << " ";
//                cout << endl;
//            }


            if (list->empty()) {
                cout << "[selection] aucune action restante après le choix" << endl;
                exit(1);
            }
            // on en tire un au hasard parmi les meilleurs
            auto it = list->begin();
            std::advance(it, rand_gen() % list->size());
            choice = *it;
//            cout << "action choisie " << *choice << endl;
        }
        // si il n'y a qu'une seule action c'est tout choisi
        else {
            choice = *best_moves.begin();
//            cout << "une seule action recommandée " << *choice << endl;
        }

        // choix du meilleur enfant globalement
        set<Link4Ptr> best;
        double best_score = -1;
        for (Link4Ptr c : node->childs) {
            // si ce noeud est terminal ou completement exploré => on passe
            if (c->next_node->terminal || c->fully_explored) continue;
            double score = 0;
            if (c->visits == 0)
                score = 100000;
            else {
                double a = (double) c->sum_score / c->visits;
                double b = sqrt(log((double) node->visits) / c->visits);
                score = a + 100 * uct_const * b;
            }
            if (score > best_score) {
                best.clear();
                best.insert(c);
                best_score = score;
            } else if (score == best_score) {
                best.insert(c);
            }
        }
        // on a trouvé un gagnant ?
        if (best.empty()) {
            cerr << "[selection] pas d'enfant selectionné globalement" << endl;
            exit(0);
        }
//        else {
//            cout << "bests globalement: ";
//            for (Link4Ptr l : best)
//                for (TermPtr t : l->moves)
//                    cout << *t << " ";
//            cout << endl;
//            if (best_score == 100000) {
//                ;
//            }
//        }



        // on note l'enfant selectionné et on l'ajoute à la descente
        // on note aussi la position suivante
        vector<Link4Ptr> subleg;
        vector<Node4Ptr> next_node;
        bool not_all = false;                                  // a virer
        for (int s = 0; s < cnode.size(); s++) {
            Link4Ptr child = cnode[s]->getChild(choice);
            if (!child->fully_explored) not_all = true;        // a virer
            subleg.push_back(child);
            next_node.push_back(child->next_node);
        }
        assert(not_all);                                       // a virer
        descent_move.push_back(subleg);
        global_descent_move.push_back(node->getChild(choice));
        // on joue le coup pour passer à la position suivante
        circuit.setMove(current, choice);
        circuit.next(current);
        // on verifie si on a pas atteint une position terminale
        if (circuit.isTerminal(current)) {
            return true;
        }
        // sinon on continue la descente
        cnode = next_node;
        node = node->getChild(choice)->next_node;
    }
}

bool UctSinglePlayerDecompAndNormal::expansion(vector<Node4Ptr>& cnode, Node4Ptr& node, SetTermPtr& unexplored) {
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
            Node4Ptr n = cnode[s]->noteMoveResult(*it, getSubPos(next_pos, s), descent_node.size(), circuit.isTerminal(next), transpo[s]);
            if (n != nullptr)
                already_known = false;
        }
        // note global node
        Node4Ptr gn = node->noteMoveResult(*it, std::move(next_pos), global_descent_node.size(), circuit.isTerminal(next), global_transpo);
        for (int s = 0; s < cnode.size(); s++) {
            auto& sc = cnode[s]->getChild(*it)->next_node->global_nodes;
            if (gn != nullptr) {
                sc.insert(gn);
                gn->nb_childs = circuit.getLegals(next, role).size();
            }
            else {
                gn = node->getChild(*it)->next_node;
                assert(sc.find(gn) != sc.end());
            }
        }

        // si elle amène à une position connue de tous on recommence
        if (already_known) {
            unexplored.erase(it);
        } else {
            // on note l'enfant selectionné et on l'ajoute à la descente
            vector<Link4Ptr> subleg;
            for (int i = 0; i < cnode.size(); i++) {
                Link4Ptr child = cnode[i]->getChild(*it);
                subleg.push_back(child);
            }
            descent_move.push_back(subleg);
            Link4Ptr child = node->getChild(*it);
            global_descent_move.push_back(child);
            // on joue le coup
            current = next;
            return true;
        }
    }
    return false;
}

void UctSinglePlayerDecompAndNormal::simul_backprop() {
    // si la position n'est pas terminale, on fait un playout
    bool last_terminal = false;
    if (!circuit.isTerminal(current))
        circuit.playout(current);
    else last_terminal = true;
    // on récupère le score
    Score score = circuit.getGoal(current, role);

    // pour chaque sous-jeu
    for (int s = 0; s < (int) subgames_trues.size(); s++) {
        // on remonte chaque nœud
        for(int i = (int) descent_node.size()-1; i >= 0; i--) {
            // on récupère la position selectionnée et l'enfant correspondant au coup joué
            Node4Ptr n = descent_node[i][s];
            Link4Ptr c = descent_move[i][s];
            // on augmente le nombre de visites du parent
            n->visits++;
            // on augmente le nombre de visites et on note le score de enfant
            c->visits++;
            // contribution du sous-jeu
            c->sum_score += score;
        }
    }

    // on remonte chaque nœud du jeu global
    for(int i = (int) global_descent_node.size()-1; i >= 0; i--) {
        // on augmente le nombre de visites du parent et enfant
        global_descent_node[i]->visits++;
        global_descent_move[i]->visits++;
        // on note le score
        global_descent_move[i]->sum_score += score;
    }

    // si on a trouvé le score max, on le note et on le signale globalement
    if (score == 100) solution_found = true;
}

/*******************************************************************************
 *      Debuggage
 ******************************************************************************/

void UctSinglePlayerDecompAndNormal::printCurrent() {
    cout << "current position:" << endl;
    for (const TermPtr t : circuit.getPosition(current))
        cout << *t << " ";
    cout << endl;
}

void UctSinglePlayerDecompAndNormal::printSubTree(ostream& out, Node4Ptr n, int max_level) {
    set<Node4Ptr> done;
    done.insert(n);
    printSubTree(out, n, 0, max_level, done);
    cout << endl;
}
void UctSinglePlayerDecompAndNormal::printSubTree(ostream& out, Node4Ptr n, int level, int max_level, set<Node4Ptr>& done) {
    if (level > max_level) return;
    string indent;
    for (int i = 0; i < level; i++) indent += "\t";

    out << indent << "N=" << n->visits;
    out << " node=" << n << " [ ";
    for (TermPtr t : n->pos)
        out << *t << " ";
    out << "]" << endl;
    out << indent << "childs:" << endl;
    for (Link4Ptr c : n->childs) {
        out << indent << " - Ni=" << c->visits << " \tWi=" << c->sum_score << " \tnext_node=" << c->next_node << " \tmove=";
        auto it = c->moves.begin();
        for (int i = 0; i < 3 && it != c->moves.end(); i++, it++) {
            out << **it << " ";
        }
        if (it != c->moves.end()) out << "...";
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
void UctSinglePlayerDecompAndNormal::createGraphvizFile(const string& name, int subgame) const {
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
string UctSinglePlayerDecompAndNormal::graphvizRepr(const string& name, int subgame) const {
    stringstream nodes;
    stringstream links;

    for (auto& entry : transpo[subgame]) {
        Node4Ptr n = entry.second;
        nodes << "    " << (long long) n << " [label=\"N=" << n->visits << "\"";
        if (n->childs.size() > 0 && n->nb_fully_explored > 0) {
            int color = ((float) n->nb_fully_explored / n->childs.size() * 6) + 1;
            nodes << ", style=filled, fillcolor=" << color;
        }
        else if (n->terminal) nodes << ", style=filled, fillcolor=7";
        nodes << "]" << endl;
        for (Link4Ptr c : n->childs) {
            links << "    " << (long long) n << " -> " << (long long) c->next_node;
            links << " [label=\"Ni=" << c->visits << " Wi=" << c->sum_score;
            links << " µ=" << std::fixed << std::setprecision(2) << ((float) c->sum_score/c->visits) << " ";
            auto it = c->moves.begin();
            for (int i = 0; i < 3 && it != c->moves.end(); i++, it++) {
                links << **it << " ";
            }
            if (it != c->moves.end()) links << "...";
            links << "\"";
            if (c->fully_explored) links << ", color=red";
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


/*******************************************************************************
 *      GraphViz file
 ******************************************************************************/

/* create a graphViz file repesenting the subtrees */
void UctSinglePlayerDecompAndNormal::createGraphvizFileFromPos(const string& name, Node4Ptr pos) const {
    // create a graphviz .gv file into log/ directory
    string gv_file = log_dir + name + ".gv";
    string ps_file = log_dir + name + ".ps";

    ofstream fp(gv_file);
    if (!fp) {
        cerr << "Error in createGraphvizFile: unable to open file ";
        cerr << gv_file << endl;
        exit(EXIT_FAILURE);
    }
    fp << graphvizReprFromPos(name, pos);
    fp.close();

    cout << "#graphviz .gv file created, can't create the .ps file ";
    cout << "myself, use this to create .ps file :" << endl;
    cout << "dot -Tps ";
    cout << gv_file << " -o " << ps_file << endl;
}
/* give the graphViz representation of the propnet */
string UctSinglePlayerDecompAndNormal::graphvizReprFromPos(const string& name, Node4Ptr pos) const {
    stringstream nodes;
    stringstream links;

    std::queue<Node4Ptr> to_graph;
    to_graph.push(pos);

    while (!to_graph.empty()) {
        Node4Ptr n = to_graph.front();
        to_graph.pop();
        nodes << "    " << (long long) n << " [label=\"N=" << n->visits << "\"";
        if (n->childs.size() > 0 && n->nb_fully_explored > 0) {
            int color = ((float) n->nb_fully_explored / n->childs.size() * 6) + 1;
            nodes << ", style=filled, fillcolor=" << color;
        }
        else if (n->terminal) nodes << ", style=filled, fillcolor=7";
        nodes << "]" << endl;
        for (Link4Ptr c : n->childs) {
            to_graph.push(c->next_node);
            links << "    " << (long long) n << " -> " << (long long) c->next_node;
            links << " [label=\"Ni=" << c->visits << " Wi=" << c->sum_score;
            links << " µ=" << std::fixed << std::setprecision(2) << ((float) c->sum_score/c->visits) << " ";
            auto it = c->moves.begin();
            for (int i = 0; i < 3 && it != c->moves.end(); i++, it++) {
                links << **it << " ";
            }
            if (it != c->moves.end()) links << "...";
            links << "\"";
            if (c->fully_explored) links << ", color=red";
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

