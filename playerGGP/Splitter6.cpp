//
//  Splitter6.c
//  playerGGP
//
//  Created by Dexter on 03/03/2017.
//  Copyright © 2017 dex. All rights reserved.
//

#include "Splitter6.h"
#include <assert.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <queue>
#include <cstring>
#include "test_globals_extern.h"

using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::array;
using std::deque;
using std::queue;
using std::set;
using std::map;
using std::pair;
using std::move;
using std::string;
using std::stringstream;
using std::ofstream;


/*******************************************************************************
 *     Splitter : main
 ******************************************************************************/

Splitter6::Splitter6(Circuit& circ, Propnet& prop) : circuit(circ), c_infos(circ.getInfos()), factory(circ.getGdlFactory()), propnet(prop) {
    nb_inputs = c_infos.id_does.back() - c_infos.id_does.front();
    nb_bases = c_infos.id_does.front() - c_infos.id_true;

    NUM_PLAYOUT = 10000;

    // get start position
    init_values.resize(c_infos.values_size, 0);
    const VectorTermPtr init_pos = circuit.getInitPos();
    circuit.setPosition(init_values, init_pos);
    
    std::default_random_engine rand_gen((std::random_device()).operator()());

    // fluents indépendants des actions
    actionDependencies();     // check action independent fluents, noop actions and which action is in which fluent NEXT rule
    //printActionIndependent(cout);
    actionIndependentPrems(); // find links between action independant fluents and their premisses
    findLegalPrecond();       // find preconditions to actions legality
    //printLegalPrecond(cout);
    findDoesPrecondInNext();
    //printDoesPrecondInNext(cout);
    getCondVictory();         // find subgoals and goals
    //printCondVictory(*out_dcmp);

    // initialiser les structures pour collecter les données sur chaque joint move
    in_joint_moves.resize(nb_inputs);
    in_nb_jmoves.resize(nb_inputs);
    // pour stocker les positions pendant les playouts
    VectorBool previous(c_infos.values_size, 0);
    VectorBool values(c_infos.values_size, 0);
    // initialiser les structures pour collecter les effets positifs et négatifs analysés : pour chaque action, la force du lien avec chacun des fluents
    actions_efflinks[POS].resize(nb_inputs, vector<float>(nb_bases, 0));
    actions_efflinks[NEG].resize(nb_inputs, vector<float>(nb_bases, 0));

    // playouts pour collecter des infos
    for (int p = 0; p < NUM_PLAYOUT; ++p) {
        // position initiale
        circuit.setPosition(values, init_pos);

        // on propage le signal dans le circuit
        circuit.terminal_legal_goal(values);

        // boucle du playout
        int step = 0;
        while(!circuit.isTerminal(values)) {
            ++step;

            // pour chaque role : jouer un coup légal
            vector<int> nb_jmoves(c_infos.roles.size(), 1);
            for (TermPtr role : c_infos.roles) {
                VectorTermPtr legals = circuit.getLegals(values, role); // coups légaux
                if (legals.empty()) {
                    step = -1;
                    cout << "Erreur : " << *role << " n'a pas de coup légal" << endl;
                    break;
                }

                for (size_t id_r = 0; id_r < c_infos.roles.size(); id_r++)
                    if (c_infos.roles[id_r] != role)
                        nb_jmoves[id_r] *= legals.size();

                TermPtr move = legals[rand_gen() % legals.size()];  // on en choisi un au hasard
                circuit.setMove(values, move); // on le joue
            }
            if (step == -1) { break; }
            // copie de la position avec les coups choisis
            memcpy(previous.data(), values.data(), values.size() * sizeof(values.front())); // copie de la position précédente

            // position suivante
            circuit.next(values);
            // collecte des infos entre les deux positions
            getTransitionInfos(previous, values, nb_jmoves, step);
        }
    }

    //printJmoveEffects(cout);    // effets collectés pour chaque mouvement joint
    //printInJmoveEffects(cout);  // effets de chaque actions dans divers mouvements joints
    //printWhatChangeTogether(cout);
    //printInNbJmove(cout);

    auto actions_efflinks_copy = actions_efflinks;
    analyseTransitionInfos();
    updateDoesPrecondInNext(actions_efflinks_copy);
    //printDoesPrecondInNext(cout);

    //printActionsLinks(cout);
    //printActionEffectOnFluents(cout);

    findMetaActions(); // JEUX PARALLELES

    //printMeta2Actions(meta_actions, string(" DEF"));
    //printMeta2Links(meta_actions, string("LINKS WITH META-ACTIONS DEF"));

    findCrossPoint(); // JEUX EN SERIE - methode plus fiable.
//    printCrosspoints(*out_dcmp);
    splitSerialGames();

    findSubgames();
    //printSubgames(cout);
    //createGraphvizFile("subgames_before"); // effets collectés pour les mouvements joints
    //createGraphvizFile("subgames_strong_before", true);

    checkWithSubGoals(); // unir les sous-jeux trop décomposés (rare : décomposer les sous-jeux sous-décomposés)
//    printSubgames(*out_dcmp);

#if PLAYERGGP_DEBUG == 2
        createGraphvizFile("subgames"); // effets collectés pour les mouvements joints
        createGraphvizFile("subgames_strong", true);
#endif

    // compile les informations pour jouer avec
    getWhiteAndBlackLists();
    getSubgamesTruesAndLegals();

}

/*******************************************************************************
 *     Informations sur les sous-jeux
 ******************************************************************************/

void Splitter6::getWhiteAndBlackLists() {
    // traitement des sous-jeux contenant une seule action
    for (auto& sub : subgames) {
        auto it = sub.second.rbegin();
        auto it2 = it; it2++;
        if (*it2 < c_infos.id_does.front() && *it >= c_infos.id_does.front() && meta_actions.meta_to_actions_[*it].size() == 1) {
            FactId d = *meta_actions.meta_to_actions_[*it].begin();
            // faut-il jouer cette action ou pas ?
            // pour chaque sous-but ou à défaut condition de victoire
            auto cond = cond_goalterm_sets[PART_VICTORY];
            if (cond.size() < 1) cond = cond_goalterm_sets[VICTORY];
            for (auto c : cond) {
                bool white = false; // l'action amène un fluent de cette condition
                bool black = false; // l'action enlève un fluent de cette condition
                // pour chaque fluent de la condition
                for (Fact f : c) {
                    // ce fluent n'est pas dans ce sous-jeu, on passe à la condition suivante
                    if (sub.second.find(f.id) == sub.second.end()) { white = black = true; break; }
                    // la méta-action amène ce fluent
                    if (actions_efflinks[(f.value)?POS:NEG][d - c_infos.id_does.front()][f.id - c_infos.id_true] > 0)
                        white = true;
                    // la méta-action supprime ce fluent
                    if (actions_efflinks[(f.value)?NEG:POS][d - c_infos.id_does.front()][f.id - c_infos.id_true] > 0)
                        black = true;
                    // ni l'un ni l'autre ?
                    if (!white && !black) {
                        cerr << "ERREUR : la méta-action n'a pas d'effet sur le fluent";
                        printTermOrId(f, cerr);
                        cerr << "qui est une condition de victoire" << endl;
                    }
                }
                if (white != black) {
                    if (white) white_list.insert(c_infos.vars[d]);
                    else black_list.insert(c_infos.vars[d]);
                }
            }
        }
    }
    // affichage de la whiteList et blackList
    //    cout << "WHITE : ";
    //    for (auto term : whiteList) cout << *term;
    //    cout << endl << "BLACK : ";
    //    for (auto term : blackList) cout << *term;
    //    cout << endl;
}

void Splitter6::getSubgamesTruesAndLegals() {
    std::unordered_set<TermPtr> trues;
    std::unordered_set<TermPtr> legals;
    for (const auto& entry : subgames) {
        for (FactId f_id : entry.second) {
            if (f_id < c_infos.id_does.front())
                trues.insert(c_infos.vars[f_id]);
            else {
                for (FactId d_id : meta_actions.meta_to_actions_[f_id])
                    legals.insert(c_infos.vars[d_id]);
            }
        }
        subgames_trues.push_back(trues);
        subgames_legals.push_back(legals);
        trues.clear();
        legals.clear();
    }

    // ajouter les actions et les fluents non testés
    // fluents dans aucun sous-jeu
    for (FactId id = c_infos.id_true; id < c_infos.id_does.front(); id++)
        if (fact_to_subgame[id] == 0)
            trues.insert(c_infos.vars[id]);
    // actions jamais testées
    for (FactId d_id = c_infos.id_does.front(); d_id < c_infos.id_does.back(); d_id++) {
        FactId d_num = d_id - c_infos.id_does.front();
        if (noop_actions[d_num] && in_nb_jmoves[d_num] == 0) {
            assert(hard_linked.size() > d_num && !hard_linked[d_num]);
            legals.insert(c_infos.vars[d_id]);
        }
    }
    if (!trues.empty() || !legals.empty()) {
        for (int i = 0; i < subgames_trues.size(); i++) {
            subgames_trues[i].insert(trues.begin(), trues.end());
            subgames_legals[i].insert(legals.begin(), legals.end());
        }
        trues.clear();
        legals.clear();
    }
    // ajouter les actions sans effet
    for (FactId d_id = c_infos.id_does.front(); d_id < c_infos.id_does.back(); d_id++) {
        FactId d_num = d_id - c_infos.id_does.front();
        if (noop_actions[d_num] && in_nb_jmoves[d_num] != 0)
            legals.insert(c_infos.vars[d_id]);
    }
    if (!legals.empty()) {
        useless_actions = (int) subgames_trues.size();
        subgames_trues.push_back(trues);
        subgames_legals.push_back(legals);
        legals.clear();
    }
}

/*******************************************************************************
 *     Fluents indépendants des actions ou noop d'après les règles
 ******************************************************************************/

void Splitter6::actionDependencies() {
    next_without_does.resize(nb_bases, true); // vrai par defaut
    next_with_does.resize(nb_bases);
    does_in_next.resize(nb_inputs);
    
    for (FactId d_id = c_infos.id_does.front(); d_id < c_infos.id_does.back(); d_id++) {
        // set action to true
        VectorBool values(c_infos.values_size, NOT_USED);
        values[d_id] = USED_TRUE;
        // propagate the signal without taking care of the gate type
        circuit.propagateNot(values);
    
        // outputs that are true maybe influenced by the action
        for (FactId n_id = c_infos.id_next; n_id < c_infos.id_goal.front(); n_id++) {
            if ((values[n_id] & USED_TRUE) == USED_TRUE) {
                next_without_does[n_id - c_infos.id_next] = false;
                next_with_does[n_id - c_infos.id_next].insert(Fact(d_id,true));
                does_in_next[d_id - c_infos.id_does.front()].insert(Fact(n_id - c_infos.id_next + c_infos.id_true,true));
            }
            if ((values[n_id] & USED_FALSE) == USED_FALSE) {
                next_without_does[n_id - c_infos.id_next] = false;
                next_with_does[n_id - c_infos.id_next].insert(Fact(d_id,false));
                does_in_next[d_id - c_infos.id_does.front()].insert(Fact(n_id - c_infos.id_next + c_infos.id_true,false));
            }
        }
    }
}

// premisses des fluents actions indépendants
void Splitter6::actionIndependentPrems() {
    for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) {
        if (next_without_does[t_id - c_infos.id_true]) {
            actionIndependentPrems_aux(t_id);
        }
    }
   
//    cout << "-------------------------ACTION INDEPENDENT LINKS" << endl;
//    for (auto entry : fluents_prems) {
//        printTermOrId(entry.first, cout);
//        cout << " : ";
//        for (FactId id : entry.second)
//            printTermOrId(id, cout);
//        cout << endl;
//    }
}

void Splitter6::actionIndependentPrems_aux(FactId t_id) {
    VectorBool values(c_infos.values_size, NOT_USED);
    values[t_id - c_infos.id_true + c_infos.id_next] = USED_TRUE;
    // back propagate the signal without taking care of the gate type
    circuit.backPropagateNot(values);
    for (FactId t_id2 = c_infos.id_true; t_id2 < c_infos.id_does.front(); t_id2++)
        if (values[t_id2] != NOT_USED)
            fluents_prems[t_id].insert(t_id2);
}

/*******************************************************************************
 *     Préconditions dans les NEXT et LEGAL
 ******************************************************************************/

// cherche les préconditions à l'effet des actions dans les NEXT
// toutes les preconditions sont placées dans unsafe_precond_to_does en attendant de savoir si l'action a bien un effet sur la conclusion
void Splitter6::findDoesPrecondInNext() {
    // collect preconditions : pour chaque conclusion d'un next, pour chaque role, pour chaque fluent en premisse, l'action qui apparait en conjonction
    for (RoleId r = 0; r < c_infos.roles.size(); r++) {
        for (FactId d_id = c_infos.id_does[r]; d_id < c_infos.id_does[r+1]; d_id++) {
            VectorBool values(c_infos.values_size, NOT_USED);
            values[d_id] = USED_TRUE;
            circuit.propagateNot(values); // propagate to the output, take care of NOT gates

            // si la factorisation a transformé des règles du type
            // next(f) :- does(r,a), true(f).  next(f) :- does(r,a).
            // en supprimant la première règle, mais pas dans tous les cas,
            // on peut avoir deux groupes d'actions : celles qui sont utilisées seules
            // et celle qui ont true(f) comme précondition. Il faut réunir les deux groupes
            // => pour ça je vérifie si un next peut devenir vrai avec l'action seule
            VectorBool values_testT(c_infos.values_size, UNDEF);
            values_testT[d_id] = BOOL_T;
            circuit.propagate3StatesStrict(values_testT);
            VectorBool values_testF(c_infos.values_size, UNDEF); // je vérifie aussi avec l'action négative au cas où
            values_testF[d_id] = BOOL_F;
            circuit.propagate3StatesStrict(values_testF);

            // get a copy and for each next gate, backprogagate the signal,
            // and get fluents used in conjunction with the action
            for (FactId id_n = c_infos.id_next; id_n < c_infos.id_goal.front(); id_n++) {
                FactId id_num = id_n - c_infos.id_next;
                FactId id_n2t = id_num + c_infos.id_true;
                vector<bool> used_val;
                if ((values[id_n] & USED_TRUE) == USED_TRUE) used_val.push_back(true);
                if ((values[id_n] & USED_FALSE) == USED_FALSE) used_val.push_back(false);

                // si l'action est en premisse du next (utilisée avec ou sans négation)
                for (bool val : used_val) {
                    bool no_precond = true;

                    // si l'action seule est suffisante pour décider de la valeur du NEXT : on oublie les preconditions
                    //if (values_test[id_n] != BOOL_T || !val) {
                    if ((val && values_testT[id_n] != BOOL_T) || (!val && values_testF[id_n] != BOOL_F)) {

                        // on utilise une copie des portes utilisant l'action,
                        // on efface les sorties, on marque juste le NEXT qui nous interesse,
                        // on rétro-propage pour trouver les fluents utilisés en conjonction avec l'action
                        VectorBool values2(values);
                        memset(values2.data() + c_infos.id_terminal, NOT_USED, (c_infos.id_end - c_infos.id_terminal) * sizeof(values2.front()));
                        values2[id_n] = USED_CONJ | USED_NORMAL | ((val)? USED_TRUE:USED_FALSE);
                        //cout << "----------------------------backPropagateConj" << endl;
                        circuit.backPropagateConj(values2);

                        for (FactId id_t = c_infos.id_true; id_t < c_infos.id_does.front(); id_t++) {
                            for (bool with : {false, true}) {
                                if ((values2[id_t] & ((with?WITH_TRUE:WITH_FALSE) | USED_CONJ)) == ((with?WITH_TRUE:WITH_FALSE) | USED_CONJ)) {
                                    no_precond = false;
                                    // on a pas les flags qui sont avant la conjonction (pour inverser les portes en cas de négation : loi de Morgan)
                                    //assert((values2[id_t] & USED_REVERS) != USED_REVERS && (values2[id_t] & USED_NORMAL) != USED_NORMAL);
                                    unsafe_precond_to_does[Fact(id_n2t, val)][r][Fact(id_t, with)].insert(d_id);
                                }
                            }
                        }
                    }
                    // si pas de précondition : on associe une precondition toujours vraie
                    if (no_precond) {
                        unsafe_precond_to_does[Fact(id_n2t, val)][r][Fact(0, true)].insert(d_id);
                    }
                }
            }
        }
    }
}

// si on relance la décomposition après une nouvelle série de playouts
// - relancer la recherche modifie juste le placement des préconditions dans unsafe_precond_to_does ou precond_to_does...
// - version de la fonction qui vérifie juste le placement dans un ensemble ou l'autre en fonction des nouveaux effets identifiés sans tout recalculer
// on peut aussi avoir éliminé un effet détecté de manière érronée avant, il faut faire le transfert dans les deux sens possibles
void Splitter6::updateDoesPrecondInNext(array<vector<vector<float> >, 2> prev_efflinks) {
    // update preconditions : pour chaque conclusion d'un next, pour chaque role, pour chaque fluent en premisse, l'action qui apparait en conjonction avec la précondition
    // si il y a un effet qui n'est plus là, on déplace les infos de precond_to_does à unsafe_precond_to_does
    for (auto it_next = precond_to_does.begin(); it_next != precond_to_does.end(); /* no increment */ ) {
        for (auto it_role = it_next->second.begin(); it_role != it_next->second.end(); /* no increment */ ) {
            for (auto it_precond = it_role->second.begin(); it_precond != it_role->second.end(); /* no increment */ ) {
                for (auto it_action = it_precond->second.begin(); it_action != it_precond->second.end(); /* no increment */ ) {
                    FactId d_num = *it_action - c_infos.id_does.front();
                    FactId t_num = it_next->first.id - c_infos.id_true;
                    auto val = (it_next->first.value)?POS:NEG;
                    // effet disparu
                    if (prev_efflinks[val][d_num][t_num] > 0 && actions_efflinks[val][d_num][t_num] == 0) {
                        unsafe_precond_to_does[it_next->first][it_role->first][it_precond->first].insert(*it_action);
                        it_action = it_precond->second.erase(it_action);
                    } else {
                        it_action++;
                    }
                }
                if (it_precond->second.empty()) it_precond = it_role->second.erase(it_precond);
                else it_precond++;
            }
            if (it_role->second.empty()) it_role = it_next->second.erase(it_role);
            else it_role++;
        }
        if (it_next->second.empty()) it_next = precond_to_does.erase(it_next);
        else it_next++;
    }
    // si il y a un nouvel effet qui n'était pas là avant, on déplace les infos de unsafe_precond_to_does à precond_to_does
    for (auto it_next = unsafe_precond_to_does.begin(); it_next != unsafe_precond_to_does.end(); /* no increment */ ) {
        for (auto it_role = it_next->second.begin(); it_role != it_next->second.end(); /* no increment */ ) {
            for (auto it_precond = it_role->second.begin(); it_precond != it_role->second.end(); /* no increment */ ) {
                for (auto it_action = it_precond->second.begin(); it_action != it_precond->second.end(); /* no increment */ ) {
                    FactId d_num = *it_action - c_infos.id_does.front();
                    FactId t_num = it_next->first.id - c_infos.id_true;
                    auto val = (it_next->first.value)?POS:NEG;
                    // nouvel effet détecté
                    if (prev_efflinks[val][d_num][t_num] == 0 && actions_efflinks[val][d_num][t_num] > 0) {
                        precond_to_does[it_next->first][it_role->first][it_precond->first].insert(*it_action);
                        it_action = it_precond->second.erase(it_action);
                    } else {
                        it_action++;
                    }
                }
                if (it_precond->second.empty()) it_precond = it_role->second.erase(it_precond);
                else it_precond++;
            }
            if (it_role->second.empty()) it_role = it_next->second.erase(it_role);
            else it_role++;
        }
        if (it_next->second.empty()) it_next = unsafe_precond_to_does.erase(it_next);
        else {
            // si on a vu f changer un nombre de fois supérieur à GLOBAL_LINKED_OCC
            // et si un next(f) n'est pas présent dans safe c'est qu'il ne change pas avec les actions => il est indépendant des actions
            if (change_together_global_occ[it_next->first] > GLOBAL_LINKED_OCC && precond_to_does.find(it_next->first) == precond_to_does.end()) {
                if (!action_independent[it_next->first.id - c_infos.id_true]) {
                    action_independent[it_next->first.id - c_infos.id_true] = true;
                    actionIndependentPrems_aux(it_next->first.id);
                    printTermOrId(it_next->first.id, *out_dcmp);
                    *out_dcmp << " fluent indépendant des actions détecté ! (next(f) pas present dans safe)" << endl;
                }
            }
            it_next++;
        }
    }
}

// collect preconditions inside LEGAL rules even action independent fluents
void Splitter6::findLegalPrecond() {
    for (RoleId r = 0; r < c_infos.roles.size(); r++) {
        for (FactId d_id = c_infos.id_does[r]; d_id < c_infos.id_does[r+1]; d_id++) {
            VectorBool values(c_infos.values_size, NOT_USED);
            values[d_id - c_infos.id_does.front() + c_infos.id_legal.front()] = USED_TRUE;
            circuit.backPropagateNot(values); // back propagate with 4 states (NOT_USED, USED_FALSE, USED_TRUE, USED_TANDF)

            bool no_precond = true;
            for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) {
                if ((values[t_id] & USED_TRUE) == USED_TRUE) {
                    precond_to_legal[Fact(t_id, true)][r].insert(d_id);
                    no_precond = false;
                }
                if ((values[t_id] & USED_FALSE) == USED_FALSE) {
                    precond_to_legal[Fact(t_id, false)][r].insert(d_id);
                    no_precond = false;
                }
            }
            if (no_precond) {
                precond_to_legal[Fact(0, true)][r].insert(d_id); // on associe une precondition toujours vraie
            }
        }
    }

    // on vérifie que chaque précondition permet bien la légalité des actions collectées
    // [cf : ticTacHeaven => (true (mark_large 1 1 1 1 x)) et (not (true (mark_large 1 1 1 1 x))) comme précondition de (does xplayer (play_large 1 1 1 1 x))]
    for (auto& precond_entry : precond_to_legal) {
        VectorBool values(c_infos.values_size, UNDEF);
        values[precond_entry.first.id] = (precond_entry.first.value)?BOOL_T:BOOL_F;
        // propage le signal avec 3 etats pour voir si le fluent peut bien rendre l'action légale
        circuit.propagate3States(values);
        for (auto& role_entry : precond_entry.second) {
            for (auto it = role_entry.second.begin(); it != role_entry.second.end();  ) {
                if (values[*it - c_infos.id_does.front() + c_infos.id_legal.front()] == BOOL_F)
                    it = role_entry.second.erase(it);
                else it++;
            }
        }
    }
}

/*******************************************************************************
 *     Conditions de victoire, sous-buts et conditions de terminaison
 ******************************************************************************/

void Splitter6::getCondVictory() {
    // recolte le contenu des conditions de victoire et subgoals
    std::array<set<Fact>, 2> cond_victory;
    vector<bool> has_subgoal(c_infos.roles.size(), false); // est-ce qu'il existe des sous-buts pour chaque joueur ?
    vector<FactId> max_goals(c_infos.roles.size());   // id de la sortie GOAL correspondante pour chaque joueur
    vector<FactId> min_goals(c_infos.roles.size());   // id de la sortie GOAL 0 (ou goal min) pour chaque joueur

    // trouver les GOALs de score min et max pour chaque joueur
    for (RoleId r_id = 0; r_id < c_infos.roles.size(); r_id++) {
        Score min = 100;
        Score max = 0;
        max_goals[r_id] = c_infos.id_goal[r_id];
        for (FactId g_id = c_infos.id_goal[r_id]; g_id < c_infos.id_goal[r_id+1]; g_id++) {
            Score s = stoi(c_infos.vars[g_id]->getArgs().back()->getFunctor()->getName());
            if (s > max) {
                max = s;
                max_goals[r_id] = g_id;
            }
            if (s < min) {
                min = s;
                min_goals[r_id] = g_id;
            }
        }
    }

    // noter les portes qui interviennent dans goal   // TODO : UTILISÉ AUSSI POUR TROUVER LES SOUS-JEUX INUTILES => A CALCULER UNE FOIS ET CONSERVER
    VectorBool values_in_goal(c_infos.values_size, NOT_USED);
    for (FactId id_g = c_infos.id_goal.front(); id_g < c_infos.id_goal.back(); id_g++)
        values_in_goal[id_g] = USED_TRUE;
    circuit.backPropagateNot(values_in_goal);

    // garder une trace des portes déjà identifiées comme sous-buts ou condition de victoire et des portes qui les utilisent
    VectorBool values_subg(c_infos.values_size, UNDEF);
    VectorBool values_cvic(c_infos.values_size, UNDEF);


    // examiner chaque porte intervenant dans goal dans l'ordre (des entrées vers les sorties)
    // d'abord les trues, ensuite tout ce qui est après end (portes internes)
    for (FactId id = c_infos.id_true; id < c_infos.values_size; id++) {
        if (id == c_infos.id_does.front()) id = c_infos.id_end; // si on a fini avec les trues, on passe aux portes internes
        if (values_in_goal[id] == NOT_USED) continue; // cette porte n'intervient pas dans goal

        // une condition de victoire a déjà été trouvé en amont
        if (values_cvic[id] == BOOL_T) continue;

        vector<bool> values;
        if ((values_in_goal[id] & USED_TRUE) == USED_TRUE)
            values.push_back(true);
        if ((values_in_goal[id] & USED_FALSE) == USED_FALSE)
            values.push_back(false);

        // teste les sorties en fonction // TODO : propager les deux valeurs vrai et faux en même temps
        array<VectorBool, 2> values_g;
        values_g[POS] = VectorBool(c_infos.values_size, UNDEF);
        values_g[POS][id] = BOOL_T;
        circuit.propagate3StatesStrict(values_g[POS]);
        values_g[NEG] = VectorBool(c_infos.values_size, UNDEF);
        values_g[NEG][id] = BOOL_F;
        circuit.propagate3StatesStrict(values_g[NEG]);

//        // debug
//        cout << endl << endl << "avec ";
//        printTermOrId(id, cout); // un seul fluent
//        cout << " [used : ";
//        for (auto b : values) {
//            cout << b << " ";
//        }
//        cout << "]";
//        set<FactId> vset;
//        if (id > c_infos.id_does.front()) {
//            cout << " : ";
//            // les fluents constituant une expression
//            VectorBool values(c_infos.values_size, BOOL_F);
//            values[id] = BOOL_T;
//            circuit.backPropagate(values);
//            for (FactId id_t = c_infos.id_true; id_t < c_infos.id_does.front(); id_t++)
//                if (values[id_t] == BOOL_T)
//                    printTermOrId(id_t, cout);
//        }
//        cout << endl;
//        for (FactId g_id = c_infos.id_goal.front(); g_id < c_infos.id_goal.back(); g_id++) {
//            cout << "\t\t" << *c_infos.vars[g_id] << " => ";
//            if (values_g[POS][g_id] == BOOL_T) cout << "1";
//            else if (values_g[POS][g_id] == BOOL_F) cout << "0";
//            else cout << "-";
//            cout << "    ";
//            if (values_g[NEG][g_id] == BOOL_T) cout << "1";
//            else if (values_g[NEG][g_id] == BOOL_F) cout << "0";
//            else cout << "-";
//            cout << endl;
//        }
//        cout << "\t\t terminal => ";
//        if (values_g[POS][c_infos.id_terminal] == BOOL_T) cout << "1";
//        else if (values_g[POS][c_infos.id_terminal] == BOOL_F) cout << "0";
//        else cout << "-";
//        cout << "    ";
//        if (values_g[NEG][c_infos.id_terminal] == BOOL_T) cout << "1";
//        else if (values_g[NEG][c_infos.id_terminal] == BOOL_F) cout << "0";
//        else cout << "-";
//        cout << endl;
//        if (values_cvic[id] == BOOL_T) {
//            cout << " => condition de victoire déjà trouvée en amont" << endl;
//            continue;
//        }
//        if (values_subg[id] == BOOL_T) cout << " => sous-but déjà trouvé en amont" << endl;


        // condition de victoire ou sous-but (goal_min impossible si il est vrai ou min < goal < max si il est vrai ou goal_max impossible si il est faux)
        for (bool val : values) {
            for (RoleId r_id = 0; r_id < c_infos.roles.size(); r_id++) {
                bool is_max = values_g[(val)?POS:NEG][max_goals[r_id]] == BOOL_T;
                bool is_not_max = values_g[(val)?NEG:POS][max_goals[r_id]] == BOOL_F;
                bool is_not_min = false;
                // est-ce qu'on trouve un score supérieur à 0 ?
                for (FactId g_id = c_infos.id_goal[r_id]; g_id < c_infos.id_goal[r_id+1]; g_id++) {
                    if (g_id != min_goals[r_id] && values_g[(val)?POS:NEG][g_id] == BOOL_T)
                        is_not_min = true;
                }
                // ou bien on a la garantie que le score n'est pas 0 (et peut potentiellement être 100)
                is_not_min = (is_not_min || values_g[(val)?POS:NEG][min_goals[r_id]] == BOOL_F) && values_g[(val)?POS:NEG][max_goals[r_id]] != BOOL_F;

                // cette expression garantit qu'un joueur a un score maximal
                if (is_max) {
                    cond_victory[VICTORY].insert(Fact(id, val));
                    values_subg[id] = BOOL_T;
                    circuit.spread(values_subg);
                    values_cvic[id] = BOOL_T;
                    circuit.spread(values_cvic);
//                    cout << " => condition de victoire" << endl;
                }
                // cette expression garantit qu'un joueur a un score différent de 0 ou son absence empêche de gagner
                // et elle ne contient pas un sous-but déjà identifié
                else if ((is_not_max || is_not_min) && values_subg[id] != BOOL_T) {
                    has_subgoal[r_id] = true;
                    cond_victory[PART_VICTORY].insert(Fact(id, val));
                    values_subg[id] = BOOL_T;
                    circuit.spread(values_subg);
//                    cout << " => sous-but" << endl;
                }
            }
        }
    }

    // est-ce qu'un des joueurs n'a pas de sous-but ? si oui, on ignore les sous-buts
    bool ignore_subgoals = false;
    for (bool sb : has_subgoal) {
        ignore_subgoals = ignore_subgoals || !sb;
    }

    // recolte des fluents intervenant dans chaque condition de victoire
    for (auto vic : {PART_VICTORY, VICTORY}) {
        if (ignore_subgoals && vic == PART_VICTORY) {
            if (!cond_victory[vic].empty()) {
                *out_dcmp << "WARNING : pas de sous-buts pour tous les joueurs" << endl;
#ifndef PLAYERGGP_DEBUG
                cout << "WARNING : pas de sous-buts pour tous les joueurs" << endl;
#endif
            }
            continue; // ignore les sous-buts si il n'y en a pas pour tout le monde
        }
        for (const Fact& sub : cond_victory[vic]) {
            set<Fact> vset;
            if (sub.id < c_infos.id_does.front()) {
                vset.insert(sub);               // un seul fluent
            } else {                            // les fluents constituant une expression
                VectorBool values(c_infos.values_size, NOT_USED);
                values[sub.id] = (sub.value)? USED_TRUE:USED_FALSE;
                circuit.backPropagateNot(values);
                for (FactId id_t = c_infos.id_true; id_t < c_infos.id_does.front(); id_t++) {
                    if((values[id_t] & USED_TRUE) == USED_TRUE)
                        vset.insert(Fact(id_t, true));
                    if ((values[id_t] & USED_FALSE) == USED_FALSE)
                        vset.insert(Fact(id_t, false));
                }
            }
            if (!vset.empty())
                cond_goalterm_sets[vic].emplace(move(vset));
        }
    }
}

/*******************************************************************************
 *     General : intersection / inclusion  de deux sets
 ******************************************************************************/

//intersection de deux sets
template <typename T>
set<T> Splitter6::intersect(const set<T>& s1, const set<T>& s2) {
    set<T> s3;
    auto it1 = s1.begin();
    auto it2 = s2.begin();
    while (it1 != s1.end() && it2 != s2.end()) {
        if (*it1 < *it2) it1++;
        else if (*it2 < *it1) it2++;
        else {
            s3.insert(*it1);
            it1++; it2++;
        }
    }
    return s3;
}

// compare two sets of facts and split facts : only in s1 / in both sets / only in s2
template<typename T>
Splitter6::threeParts<T> Splitter6::ins1BothIns2(const set<T>& s1, const set<T>& s2) {
    threeParts<T> inside_or_not;
    auto it1 = s1.begin(), it2 = s2.begin();
    // enumerate the two sets of actions (partition's actions and preconditionned actions)
    // and sort the facts preconditioned or not
    while (it1 != s1.end() && it2 != s2.end()) {
        if (*it1 == *it2) {
            inside_or_not.both.insert(*it1);
            ++it1;
            ++it2;
        } else if (*it1 < *it2) {
            inside_or_not.ins1.insert(*it1);
            ++it1;
        } else {
            inside_or_not.ins2.insert(*it2);
            ++it2;
        }
    }
    // add the remaining actions
    inside_or_not.ins1.insert(it1, s1.end());
    inside_or_not.ins2.insert(it2, s2.end());
    
    return inside_or_not;
}

/*******************************************************************************
 *     Informations sur les effets des mouvements joints joués
 ******************************************************************************/

// informations concernant chaque mouvement joint
void Splitter6::getTransitionInfos(const VectorBool& previous, const VectorBool& values, vector<int>& nb_jmoves, int step) {
    // numéros des actions jouées
    vector<FactId> actions(c_infos.roles.size(), 0);
    for (RoleId r = 0; r < c_infos.roles.size(); r++) {
        for (FactId d_id = c_infos.id_does[r]; d_id < c_infos.id_does[r+1]; d_id++) {
            if (previous[d_id] == BOOL_T) {
                actions[r] = d_id;
                // nb_jmoves[r] nombre de combinaisons d'actions possibles pour les autres joueurs
                // == nombre de joint move où peut apparaitre l'action dans cette position
                // in_nb_jmoves indique le nombre max de joint move ou l'action intervient pour une position
                // permettra de savoir si on en a exploré une bonne partie...
                in_nb_jmoves[d_id - c_infos.id_does.front()] = std::max(in_nb_jmoves[d_id - c_infos.id_does.front()], nb_jmoves[r]);
                break;
            }
        }
        assert(actions[r] != 0);
    }
    // cherche si ce joint move a déjà été joué
    auto it = jmoves_effects.find(actions);
    // si ce joint move n'est pas connu, on initialise une structure pour stocker ses effets
    // et on stocke le joint move avec cette structure dans le dico jmoves_effects
    if ( it == jmoves_effects.end()) {
        effects e(c_infos.id_true, c_infos.id_does.front());
        it = jmoves_effects.emplace(actions, e).first;
    }
    it->second.occ_++; // on incrémente les occurences de ce mouvement joint
    
    // on note que chaque action intervient dans ce join move
    for (FactId d : actions)
        in_joint_moves[d - c_infos.id_does.front()].insert(it);
    // on note dans les stats du joint move (structure effects) les effets négatifs et positifs observés
    set<Fact> together;              // ce qui change en même temps (--à l'exclusion des fluents indépendants des actions--)
    //set<Fact> together_with_act_ind; // tout ce qui change en même temps
    for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) {
        FactId t_num = t_id - c_infos.id_true;
        //if (next_without_does[t_num]) continue; // pas de stats pour les fluents action indépendants
        if (previous[t_id] != values[t_id]) {
            Fact f(t_id, values[t_id] == BOOL_T);
            ++change_together_global_occ[f]; // une occurence de plus de cet effet (on va collecter éventuellement ce qui change en même temps)
            change_together_global_step[f] = std::max(change_together_global_step[f], step);
            ++it->second.eff_[(f.value)?POS:NEG][t_num];
            //together_with_act_ind.insert(move(f)); // tous les changements liés
            if (!next_without_does[t_num]) // si c'est pas un fluent indépendant des actions (ou si on le sait pas encore)
                together.insert(move(f));  // on va collecter les effets liés
        }
    }

    for (auto& f : together) {
        set<Fact> tog(together);
        tog.erase(f);

        // collecte des effets simultanés
        it->second.change_together_occ_[f]; // on incrémente le nombre d'occurence ou l'on a testé cet effet et ses effets liés
        // effets simultanés pour le mouvement joint courant
        auto it_change = it->second.change_together_.find(f); // f existe déjà ?
        // première fois qu'on le voit, on note ce qui change en même temps
        if (it_change == it->second.change_together_.end())
            it->second.change_together_[f] = tog;
        else { // on vérifie ce qui changeait en même temps
            auto& old = it->second.change_together_[f];
            for (auto it_old = old.begin(); it_old != old.end(); /* no increment */ ) {
                if (tog.find(*it_old) == tog.end()) { // il a pas changé cette fois ?
                    it_old = old.erase(it_old);       // on l'efface
                } else {
                    ++it_old;  // sinon on passe au suivant
                }
            }
        }
    
        // collecte des effets simultanés globalement
        it_change = change_together_global.find(f); // f existe déjà ?
        // première fois qu'on le voit, on note ce qui change en même temps
        if (it_change == change_together_global.end())
            change_together_global.emplace(f, move(tog));
        else { // on vérifie ce qui changeait en même temps
            auto& old = change_together_global[f];
            for (auto it_old = old.begin(); it_old != old.end(); /* no increment */ ) {
                if (tog.find(*it_old) == tog.end()) { // il a pas changé cette fois ?
                    it_old = old.erase(it_old);       // on l'efface
                } else {
                    ++it_old;  // sinon on passe au suivant
                }
            }
        }
    }
}

void Splitter6::analyseTransitionInfos() {
    noop_actions.clear(); // pour collecter les actions noop
    noop_actions.resize(nb_inputs, false);
    action_independent = next_without_does;  // on copie les fluents action-independants dont on est sûr d'après les règles et on va compléter la liste
    control_fluents.clear();   // pour collecter les fluents control
    control_fluents.resize(nb_bases, false);

    // structures pour stocker les effets liés aux actions : on les réinitialise à chaque recherche
    change_together_per_action.clear();
    change_together_per_action.resize(nb_inputs);
    change_together_per_action_rev.clear();
    change_together_per_action_rev.resize(nb_inputs);
    change_together_per_action_occ.resize(nb_inputs);
    
    // Collecte des probabilités d'effet de chaque action dans les mouvements joints, ainsi que les effets liés
    // on examine chaque action
    alternate_moves = true; // on supose qu'il y a des coups alternés
    vector<FactId> noop_d_ids(c_infos.roles.size());
    for (RoleId r = 0; r < c_infos.roles.size(); r++) {
        int in_several_jmove = 0; // si une seule action est dans plusieurs jmove (c'est un noop, coups alternés)

        for (FactId d_id = c_infos.id_does[r]; d_id < c_infos.id_does[r+1]; d_id++) {
            FactId d_num = d_id - c_infos.id_does.front();
            size_t total = in_joint_moves[d_num].size(); // nombre de joint moves où intervient l'action
            if (total == 0) continue;                    // aucun mouvement joint avec cette action
            if (total > 1) {
                ++in_several_jmove;         // une action qui apparait dans plusieurs joint move
                noop_d_ids[r] = d_id;       // on la memorise au cas ou ce soit la seule
            }
            // pour chaque fluents qui peut être impacté
            for (FactId t_num = 0; t_num < nb_bases; t_num++) {
                for (efftype type : {POS, NEG}) { // pour chaque effet (positif ou negatif)
                    Fact f(t_num + c_infos.id_true, type == POS);
                    float eff_pond = 0;
                    bool first_jm = true; // premier joint move examiné
                    set<Fact> linked;     // effets liés
                    int linked_occ = 0;
                    for (auto& m : in_joint_moves[d_num]) { // pour chaque joint move
                        // proportion d'effet positif ou négatif par rapport au nombre d'occurrences du mouvement joint où intervient l'action
                        eff_pond += (float) m->second.eff_[type][t_num] / m->second.occ_;
                        // effet liés pour cette action : on les examine si c'est le premier joint move ou si la liste existante n'est pas déjà vide
                        if (m->second.eff_[type][t_num] > 0 && (first_jm || !linked.empty())) {
                            auto it = m->second.change_together_.find(f);
                            if (it != m->second.change_together_.end()) {
                                linked_occ += m->second.eff_[type][t_num];
                                if (first_jm) {
                                    linked = it->second;
                                    first_jm = false;
                                } else {
                                    for (auto it_old = linked.begin(); it_old != linked.end(); /* no increment */ ) {
                                        if (it->second.find(*it_old) == it->second.end()) { // il a pas changé cette fois ?
                                            it_old = linked.erase(it_old);              // on l'efface
                                        } else {
                                            ++it_old;  // sinon on passe au suivant
                                        }
                                    }
                                }
                            } else {
                                linked.clear();
                                first_jm = false;
                            }
                        }
                    }
                    actions_efflinks[type][d_num][t_num] = eff_pond / total;
                    if (!linked.empty()) {
                        change_together_per_action[d_num].emplace(f, move(linked));
                        change_together_per_action_occ[d_num][f] = linked_occ;
                    }
                }
            }
        }
        // alors on a une et une seule action qui est dans plusieurs jmove ?
        if (in_several_jmove != 1) alternate_moves = false;
    }

//    cout << "--------------AVANT FILTRAGE DES EFFETS" << endl;
//    printActionsLinks(cout);
//    printActionEffectOnFluents(cout);

    vector<bool> new_action_indep(nb_bases, false);
    // le control est le fluent sur lequel noop a le plus d'effet
    if (alternate_moves) {
        for (RoleId r_id = 0; r_id < c_infos.roles.size(); r_id++) {
            FactId noop_d_num = noop_d_ids[r_id] - c_infos.id_does.front();
            float max_effect = 0;
            set<FactId> maybe_control_num;
            for (FactId t_num = 0; t_num < nb_bases; t_num++) {
                for (auto val : {POS, NEG}) {
                    if (max_effect < actions_efflinks[val][noop_d_num][t_num]) {
                        max_effect = actions_efflinks[val][noop_d_num][t_num];
                        maybe_control_num.clear();
                        maybe_control_num.insert(t_num);
                    }
                    else if (max_effect == actions_efflinks[val][noop_d_num][t_num]) {
                        maybe_control_num.insert(t_num);
                    }
                }
            }
            for (FactId t_num : maybe_control_num) {
                if (!action_independent[t_num]) {
                    new_action_indep[t_num] = true;
                    action_independent[t_num] = true;
                }
                control_fluents[t_num] = true;
                printTermOrId(t_num + c_infos.id_true, *out_dcmp);
                *out_dcmp << " fluent control détecté !" << endl;
            }
        }
    }
    // on prend note des actions qui sont des noop pour les coups alternés
    if (alternate_moves) {
        for (FactId d_id : noop_d_ids) {
            FactId d_num = d_id - c_infos.id_does.front();
            noop_actions[d_num] = true;
            change_together_per_action[d_num].clear();

            printTermOrId(d_id, *out_dcmp);
            *out_dcmp << " action noop pour coups alternés" << endl;

            // on remet à 0 tous les effets (seulement après avoir identifié les controls)
            actions_efflinks[POS][d_num].clear();
            actions_efflinks[POS][d_num].resize(nb_bases, 0);
            actions_efflinks[NEG][d_num].clear();
            actions_efflinks[NEG][d_num].resize(nb_bases, 0);
        }
    }
    // dans le cas d'un fluent indépendant des actions non détecté
    // soit il dépend que des actions noop
    // soit toutes les actions (sauf les noop) de chaque joueur ont le même effet sur ce fluent
    // si tous les effets sont à 0 : on a peut être jamais rencontré ce fluent dans les playouts
    for (FactId t_num = 0; t_num < nb_bases; t_num++) {
        if (action_independent[t_num]) continue;
        // on regarde si toutes les actions dans next sont des noop
        bool all_noop = true;
        for (const Fact& a : next_with_does[t_num]) {
            if (!noop_actions[a.id - c_infos.id_does.front()])
                all_noop = false;
        }
        if (all_noop) {
            new_action_indep[t_num] = true;
            action_independent[t_num] = true;
            printTermOrId(t_num + c_infos.id_true, *out_dcmp);
            *out_dcmp << " fluent indépendant des actions détecté ! (effet que de noop)" << endl;
            continue;
        }
    }
    // si des nouveaux fluent action-indépendants sont découvert, il faut chercher leurs préconditions et ajuster les effets liés
    // pour les autres fluent action-indépendants il faut juste effacer tous les effets des actions
    for (FactId t_num = 0; t_num < nb_bases; t_num++) {
        if (new_action_indep[t_num]) {
            FactId t_id = t_num + c_infos.id_true;
            actionIndependentPrems_aux(t_id); // on cherche les fluents qui le préconditionnent
            // on supprime ce fluent de tous les effets liés globaux
            for (auto& entry : change_together_global) {
                entry.second.erase(Fact(t_id, true));
                entry.second.erase(Fact(t_id, false));
            }
            change_together_global.erase(Fact(t_id, true));
            change_together_global.erase(Fact(t_id, false));
            // on supprime ce fluent des effets des actions et de leurs effets liés
            for (FactId d_num = 0; d_num < nb_inputs; d_num++) {
                change_together_per_action[d_num].erase(Fact(t_id, true));
                change_together_per_action[d_num].erase(Fact(t_id, false));
                actions_efflinks[POS][d_num][t_num] = 0;
                actions_efflinks[NEG][d_num][t_num] = 0;
            }
        } else if (action_independent[t_num]) {
            for (FactId d_num = 0; d_num < nb_inputs; d_num++) {
                actions_efflinks[POS][d_num][t_num] = 0;
                actions_efflinks[NEG][d_num][t_num] = 0;
            }
        }
    }

    // on prépare la map inverse pour les effets liés globaux
    for (const auto& entry : change_together_global)
        for (const auto& f : entry.second)
            change_together_global_rev[f].insert(entry.first);
    // on prépare la map inverse pour les effets liés par action
    for (FactId d_num = 0; d_num < change_together_per_action.size(); d_num++)
        for (const auto& entry : change_together_per_action[d_num])
            for (const auto& f : entry.second)
                change_together_per_action_rev[d_num][f].insert(entry.first);


    // Comparaison des effets liés : attribuer chaque effet à la bonne action
    map<FactId, set<Fact> > effects_to_check; // acttion et liste des effets à vérifier
    for (FactId d_num = 0; d_num < nb_inputs; d_num++) {
        if (noop_actions[d_num]) continue;

        for (FactId t_num = 0; t_num < nb_bases; t_num++) {
            if (action_independent[t_num]) continue;
            for (efftype type : {POS, NEG}) {
                if (actions_efflinks[type][d_num][t_num] > 0) {
                    Fact f(t_num + c_infos.id_true, type == POS);
                    // si cet effet est obsservé systematiquement et uniquement dans la position initiale, l'action n'en est pas responsables
                    // si aucune règle ne décrit un effet explicite ou suppose un effet implicite, l'action ne peut pas avoir cet effet
                    if ((change_together_global_step[f] == 1 && change_together_global_occ[f] == PLAYOUT_DONE)
                        || hasNoImplicitOrExplicitEffect(d_num, f) ) {

//                        // debug
//                        if (change_together_global_step[f] == 1 && change_together_global_occ[f] == PLAYOUT_DONE) {
//                            printTermOrId(d_num + c_infos.id_does.front(), cout);
//                            cout << " pas d'effet (1) sur ";
//                            printTermOrId(f, cout);
//                            cout << endl;
//                        } else {
//                            printTermOrId(d_num + c_infos.id_does.front(), cout);
//                            cout << " pas d'effet (2) sur ";
//                            printTermOrId(f, cout);
//                            cout << endl;
//                        }

                        eraseEffect(d_num, f);
                    } else {
                        effects_to_check[d_num].insert(f);
                    }
                }
            }
        }
    }

//    cout << "--------------APRES FILTRAGE (1) et (2) DES EFFETS" << endl;
//    //printActionsLinks(cout);
//    printActionEffectOnFluents(cout);
//    cout << "-------------------------------------------------" << endl;

//    static int t = 0;
//    testEffect(effects_to_check, "effets_liés" + std::to_string(++t));

    auto it_d = effects_to_check.begin();
    bool first = true;
    while (!effects_to_check.empty()) {
        if (it_d == effects_to_check.end()) {
            it_d = effects_to_check.begin(); // on repart au début pour retester les éléments ajoutés
            first = false;
        }
        FactId d_num = it_d->first;
        auto it_f = effects_to_check[d_num].begin();
        while (!effects_to_check[d_num].empty()) {
            if (it_f == effects_to_check[d_num].end())
                it_f = effects_to_check[d_num].begin(); // on repart au début pour retester les éléments ajoutés
            Fact f = *it_f;

//            //debug
//            cout << "check effet of ";
//            printTermOrId(d_num + c_infos.id_does.front(), cout);
//            cout << "on ";
//            printTermOrId(f, cout);
//            cout << endl;

            if (isNotResponsibleOfCooccurentEffect(d_num, f)) {

//                // debug
//                printTermOrId(d_num + c_infos.id_does.front(), cout);
//                cout << " pas d'effet (3) sur ";
//                printTermOrId(f, cout);
//                cout << endl;

                addEffectsToRecheck(d_num, f, effects_to_check);
                eraseEffect(d_num, f);
                it_f = effects_to_check[d_num].erase(it_f);
                continue;
            }

            auto confirmed_or_not = actionsWithConfirmedEffectOrNot(f);
            if (!confirmed_or_not.first.empty()) { // l'effet de certaines action est confirmé

//                //debug
//                for (FactId a_num : confirmed_or_not.first) {
//                    printTermOrId(a_num + c_infos.id_does.front(), cout);
//                    cout << " effet confirmé (4) sur ";
//                    printTermOrId(f, cout);
//                    cout << endl;
//                }
//                for (FactId a_num : confirmed_or_not.second) {
//                    printTermOrId(a_num + c_infos.id_does.front(), cout);
//                    cout << " pas d'effet (4) sur ";
//                    printTermOrId(f, cout);
//                    cout << endl;
//                }

                for (FactId a_num : confirmed_or_not.second) { // on supprime l'effet des autres
                    addEffectsToRecheck(a_num, f, effects_to_check);

                    if (first) eraseCooccurOnly(a_num, f);
                    else eraseEffect(a_num, f);
                }
                if (!first) {
                    for (FactId a_num : confirmed_or_not.second) { // on sait qu'elles n'ont pas d'effet pas besoin de rechecker
                        if (a_num != d_num) {
                            auto it_a_num = effects_to_check.find(a_num);
                            if (it_a_num != effects_to_check.end())
                                it_a_num->second.erase(f);
                        }
                    }
                }
            }
            it_f = effects_to_check[d_num].erase(it_f);
        }
        it_d = effects_to_check.erase(it_d);
    }


//    cout << "--------------APRES FILTRAGE (3) et (4) DES EFFETS" << endl;
//    //    printActionsLinks(cout);
//    printActionEffectOnFluents(cout);
//    cout << "-------------------------------------------------" << endl;

    // pour les jeux à plusieurs joueurs, voir si une action est vraiment responsable d'un effet
    if (c_infos.roles.size() > 1) {
        // voir si une action est plus probablement responsable d'un effet qu'une autre
        auto new_actions_links(actions_efflinks);
        // pour chaque fluent sur lequel il peut y avoir un effet
        for (FactId t_num = 0; t_num < nb_bases; t_num++) {
            if (action_independent[t_num]) continue; // si ce fluent est indépendant des actions on en tient pas compte
            for (efftype type : {POS, NEG}) {
                // pour chaque action
                for (FactId d_num = 0; d_num < nb_inputs; d_num++) {
                    FactId d_id = d_num + c_infos.id_does.front();
                    // Pour chaque mouvement joint ou l'action intervient et un effet sur ce fluent est present
                    for (const auto& m : in_joint_moves[d_num]) {
                        if (m->second.eff_[type][t_num]  > 0) {
                            // on verifie qui de l'action courante ou d'une autre action est responsable de l'effet
                            for (FactId a : m->first) {
                                if (a == d_id) continue;
                                FactId a_num = a - c_infos.id_does.front();

                                if (actions_efflinks[type][d_num][t_num] > DIFF_RATIO * actions_efflinks[type][a_num][t_num]) {
//                                    //debug
//                                    if (actions_efflinks[type][a_num][t_num] > 0) {
//                                        printTermOrId(d_id, cout);
//                                        cout << "[" << actions_efflinks[type][d_num][t_num] << "] --" << ((type == POS)? "pos":"neg") << "--> ";
//                                        printTermOrId(t_num + c_infos.id_true, cout);
//                                        cout << "\t\t[ pas ";
//                                        printTermOrId(a, cout);
//                                        cout << "][" <<  actions_efflinks[type][a_num][t_num] << "]" << endl;
//                                        cout << endl;
//                                    }

                                    // note : biddingTicTacToe_10coins_v0.kif les actions des deux joueurs sont liées au fluent, mais un seul lien est conservé
                                    new_actions_links[type][a_num][t_num] = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
        actions_efflinks.swap(new_actions_links);
    }

//    cout << "--------------APRES FILTRAGE (5) DES EFFETS" << endl;
//    //    printActionsLinks(cout);
//    printActionEffectOnFluents(cout);
//    cout << "-------------------------------------------------" << endl;

}

void Splitter6::testEffect(map<FactId, set<Fact> >& effects_to_check, const std::string& name) {
    // create a graphviz .gv file into log/ directory
    string gv_file = log_dir + name + ".gv";
    string ps_file = log_dir + name + ".ps";

    ofstream fp(gv_file);
    if (!fp) {
        cerr << "Error in createGraphvizFile: unable to open file ";
        cerr << gv_file << endl;
        exit(EXIT_FAILURE);
    }

    stringstream nodes; set<Fact> nodes_f; set<Fact> nodes_e;
    stringstream links;
    for (auto entry : effects_to_check)
        for (Fact f : entry.second)
            nodes_f.insert(f);
    int count = 0;
    for (auto entry : effects_to_check) {
        FactId d_num = entry.first;
        for (Fact f : entry.second) {
            if (++count == 20) break;
            if (actions_efflinks[(f.value)?POS:NEG][d_num][f.id - c_infos.id_true] == 0) continue;
            bool ok = false;
            // existe t'il un autre effet qui le confirme au niveau global ?
            for (const Fact& e : change_together_global_rev[f]) {
                if (change_together_global_step[e] > 1 // les fluents qui ne changent qu'au premier step ne sont pas fiables : cf.dualHamilton
                    && change_together_global_occ[e] >= GLOBAL_LINKED_OCC) { // on ne considère que les effets liés constatés N fois
                    if (actions_efflinks[(e.value)?POS:NEG][d_num][e.id - c_infos.id_true] > 0) {
                        ok = true;
                        links << ((e.value)?0:c_infos.values_size * 1000) + e.id << " -> " << ((f.value)?0:c_infos.values_size * 1000) + f.id << " [label=\"";
                        printTermOrId(d_num + c_infos.id_does.front(), links);
                        links << "\" color=red constraint=true penwidth=2];" << endl;
                    }
                }
            }
            if (!ok) {
                // l'effet est-il lié à un autre effet au niveau de l'action ?
                for (const Fact& e : change_together_per_action_rev[d_num][f]) {
                    if (change_together_global_step[e] > 1 // les fluents qui ne changent qu'au premier step ne sont pas fiables : cf.dualHamilton
                        && change_together_per_action_occ[d_num][e] >= ACTION_LINKED_OCC) { // on ne considère que les effets liés constatés N fois
                        if (actions_efflinks[(e.value)?POS:NEG][d_num][e.id - c_infos.id_true] > 0) {
                            ok = true;
                            links << ((e.value)?0:c_infos.values_size * 1000) + e.id << " -> " << ((f.value)?0:c_infos.values_size * 1000) + f.id << " [label=\"";
                            printTermOrId(d_num + c_infos.id_does.front(), links);
                            links << "\" color=orange constraint=true penwidth=1];" << endl;
                        }
                    }
                }
            }
            if (!ok) {
                // sinon ajouter un lien négatif
                links << ((f.value)?c_infos.values_size * 10000:c_infos.values_size * 11000) + f.id << " -> " << ((f.value)?0:c_infos.values_size * 1000) + f.id << " [label=\"";
                printTermOrId(d_num + c_infos.id_does.front(), links);
                links << "\" color=black constraint=true penwidth=1];" << endl;
            }
        }
    }

    for (Fact f : nodes_f) {
        nodes << "    " << ((f.value)?0:c_infos.values_size * 1000) + f.id << " [label=\"";
        printTermOrId(f, nodes);
        nodes << "\",color=blue,peripheries=1,penwidth=1];" << endl;
    }
    for (Fact e : nodes_e) {
        if (nodes_f.find(e) == nodes_f.end()) {
            nodes << "    " << ((e.value)?0:c_infos.values_size * 1000) + e.id << " [label=\"";
            printTermOrId(e, nodes);
            nodes << "\",color=blue,peripheries=1,penwidth=3];" << endl;
        }
    }

    fp << "/*--------------- graphe " << name << " ---------------*/" << endl;
    fp << "digraph game {" << endl;
    fp << "    rankdir=LR;" << endl;
    fp << "    overlap=false;" << endl;
    fp << "    size=\"4, 10\"; " << endl;
    fp << "    ranksep=\"3 equally\";" << endl;
    fp << "    compound=true;" << endl;
    fp << nodes.str();
    fp << links.str();
    fp << "}";
    fp.close();

    cout << "graphviz .gv file created, can't create the .ps file ";
    cout << "myself, use this to create .ps file :" << endl;
    cout << "dot -Tps " << gv_file << " -o " << ps_file << endl;
}

pair<set<FactId>,set<FactId> > Splitter6::actionsWithConfirmedEffectOrNot(Fact f) {
    pair<set<FactId>,set<FactId> > confirmed_or_not;
    for (FactId d_num = 0; d_num < nb_inputs; d_num++) {
        if (actions_efflinks[(f.value)?POS:NEG][d_num][f.id - c_infos.id_true] == 0) continue;
        bool ok = false;
        // existe t'il un autre effet qui le confirme au niveau global ?
        for (const Fact& e : change_together_global_rev[f]) {
            if (change_together_global_step[e] > 1 // les fluents qui ne changent qu'au premier step ne sont pas fiables : cf.dualHamilton
                && change_together_global_occ[e] >= GLOBAL_LINKED_OCC) { // on ne considère que les effets liés constatés N fois
                if (actions_efflinks[(e.value)?POS:NEG][d_num][e.id - c_infos.id_true] > 0) {
                    ok = true;
                    break;
                }
            }
        }
        if (!ok) {
            // l'effet est-il lié à un autre effet au niveau de l'action ?
            for (const Fact& e : change_together_per_action_rev[d_num][f]) {
                if (change_together_global_step[e] > 1 // les fluents qui ne changent qu'au premier step ne sont pas fiables : cf.dualHamilton
                    && change_together_per_action_occ[d_num][e] >= ACTION_LINKED_OCC) { // on ne considère que les effets liés constatés N fois
                    if (actions_efflinks[(e.value)?POS:NEG][d_num][e.id - c_infos.id_true] > 0) {
                        ok = true;
                        break;
                    }
                }
            }
        }
        if (ok) confirmed_or_not.first.insert(d_num);
        else confirmed_or_not.second.insert(d_num);
    }
    return confirmed_or_not;
}


// si une action d_num n'a pas d'effet sur f, ajoute dans effects_to_check les effets dont il faut re-vérifier la validité
void Splitter6::addEffectsToRecheck(FactId d_num, Fact f, map<FactId, set<Fact> >& effects_to_check) {
    for (const auto& recheck: {change_together_global[f], change_together_global_rev[f], change_together_per_action[d_num][f]}) {
        for (const Fact& g : recheck) {
            if (actions_efflinks[(g.value)?POS:NEG][d_num][g.id - c_infos.id_true] > 0) {
                effects_to_check[d_num].insert(g);
            }
        }
    }
}

// si un effet s'avère erroné on l'efface et on met à jour les effets liés au niveau de l'action
void Splitter6::eraseEffect(FactId d_num, Fact f) {
    // effacer cet effet
    actions_efflinks[(f.value)?POS:NEG][d_num][f.id - c_infos.id_true] = 0;
    // on supprime les liens de cet effet au niveau de l'action
    // (inutile de modifier les autres effets liés on ne les utilise pas pour éliminer un effet, juste pour le confirmer)
    for (const auto& g : change_together_per_action[d_num][f]) change_together_per_action_rev[d_num][g].erase(f);
    change_together_per_action[d_num].erase(f);
}

// si un effet s'avère erroné on l'efface et on met à jour les effets liés au niveau de l'action
void Splitter6::eraseCooccurOnly(FactId d_num, Fact f) {
    // on supprime les liens de cet effet au niveau de l'action
    // (inutile de modifier les autres effets liés on ne les utilise pas pour éliminer un effet, juste pour le confirmer)
    for (const auto& g : change_together_per_action[d_num][f]) change_together_per_action_rev[d_num][g].erase(f);
    change_together_per_action[d_num].erase(f);
}



// Si cette action n'apparait dans aucune règle qui décrit un effet explicite ou si aucune règle ne sous-entend un effet implicite => pas d'effet
// A n'a pas d'effet positif sur f si  A apparait dans aucne règle next(f) ET s'il n'y a pas une autre action B dans next(f) :- ~B.X avec X != true(f)
// A n'a pas d'effet négatif sur f si ~A apparait dans aucne règle next(f) ET s'il n'y a pas une autre action B dans next(f) :- B.X avec X != ~true(f)
bool Splitter6::hasNoImplicitOrExplicitEffect(FactId d_num, Fact f) {
//    // effet explicite ?
//    bool explicit_effect = false;
//    for (const auto& next_entry : {precond_to_does[f], unsafe_precond_to_does[f]}) {
//        for (const auto& role_entry : next_entry) {
//            for (const auto& precond_entry : role_entry.second) {
//                if (precond_entry.first != f && precond_entry.second.find(d_id) != precond_entry.second.end()) {
//                    explicit_effect = true;
//                    break;
//                }
//            }
//            if (explicit_effect) break;
//        }
//        if (explicit_effect) break;
//    }
//    // pas d'effet explicite !
//    if (!explicit_effect) {
    if (does_in_next[d_num].find(f) == does_in_next[d_num].end()) {
        // effet implicite ?
        bool implicit = false;
        for (const auto& next_entry : {unsafe_precond_to_does[Fact(f.id, !f.value)], precond_to_does[Fact(f.id, !f.value)]}) {
            // on cherche les règles ou la valeur ~f est maintenue
            for (const auto& role_entry : next_entry) {
                // si on a plusieurs préconditions possibles : il y a pas forcement la precond f => f n'est pas maintenu
                // si il y en a une seule on regarde si elle est différente de f
                if (role_entry.second.size() > 1 || role_entry.second.begin()->first != f) {
                    // si il y a une autre action que d_num dans cette entrée (elle maintient la situation, donc d_num peut implicitement la changer)
                    for (const auto& precond_entry : role_entry.second) {
                        if (precond_entry.second.size() > 1 || *precond_entry.second.begin() != d_num + c_infos.id_does.front()) {
                            implicit = true;
                            break;
                        }
                    }
                }
                if (implicit == true) break;
            }
            if (implicit == true) break;
        }
        if (!implicit) return true;  // pas d'effet implicite non plus
    }
    return false;
}

// si une action n'est pas responsable des effets g liés à f au niveau global, elle n'a pas non plus l'effet f
bool Splitter6::isNotResponsibleOfCooccurentEffect(FactId d_num, Fact f) {
    for (const Fact& g : change_together_global[f]) {
        if (actions_efflinks[(g.value)?POS:NEG][d_num][g.id - c_infos.id_true] == 0) {
            return true;
        }
    }
    return false;
}

/*******************************************************************************
 *     Identifier les méta-actions (actions composées)
 ******************************************************************************/

// create a meta-action (group of actions) or split the already existing ones
void Splitter6::MetaMap::addMetaWithLinks(set<FactId> group, vector<set<Fact> >&& links) {
    auto it = actions_to_meta_.find(group); // cherche une meta-action contenant les mêmes actions
    // n'existe pas
    if (it == actions_to_meta_.end()) {
        vector<FactId> updated;
        // compare aux meta-actions existantes
        // search whether the group is included into an existing meta-action or the reverse
        auto it_last = meta_to_actions_.end(); // dernière meta-action à comparer
        for (auto it_meta = meta_to_actions_.begin(); it_meta != it_last;) { // no increment
            set<FactId> inter = intersect(it_meta->second, group);
            // les deux meta-action ont quelque chose en commun
            if (inter.size() > 0) {
                bool in1 = inter.size() == it_meta->second.size(); // existante incluse dans la nouvelle
                bool in2 = inter.size() == group.size();           // nouvelle incluse dans l'existante
                if (in1) { // meta-action existante incluse dans la nouvelle
                    for (FactId a : inter) group.erase(a); // on élimine les actions de la nouvelle
                    for (int i = 0; i < TOTAL_LINKS; i++)  // on donne les liens à l'existante
                        meta_links_[i][it_meta->first].insert(links[i].begin(), links[i].end());
                    if (group.empty()) break; // le groupe réduit peut correspondre exactement et il ne reste rien
                    it_last = it_meta; // on note que les meta-actions precédentes n'ont pas été comparées au groupe réduit
                }
                else if (in2) { // nouvelle meta-action incluse dans l'existante
                    actions_to_meta_.erase(it_meta->second); // la meta-action existante va disparaitre (on va la scinder)
                    for (FactId a : inter) it_meta->second.erase(a); // on élimine les actions de l'existante
                    assert(!it_meta->second.empty());
                    updated.push_back(it_meta->first); // a été modifiée il faut la recomparer aux autres meta-actions
                    for (int i = 0; i < TOTAL_LINKS; i++)  // on donne les liens de l'existante à la nouvelle
                        links[i].insert(meta_links_[i][it_meta->first].begin(), meta_links_[i][it_meta->first].end());
                }
            }
            // dans tous les cas
            it_meta++; // on passe à la suivante
            // si on est à la fin mais qu'il y a des meta-actions au début
            // qui n'ont pas été comparées au groupe depuis sa réduction
            // on reprend au début pour les comparer
            if (it_meta != it_last && it_meta == meta_to_actions_.end()) {
                it_meta = meta_to_actions_.begin();
            }
        }
        // ajoute la meta-action ou ce qu'il en reste
        if (group.size() >= 1) {
            FactId meta = next_free_id_++;
            actions_to_meta_[group] = meta;
            meta_to_actions_.emplace(meta, move(group));
            for (int i = 0; i < TOTAL_LINKS; i++)
                meta_links_[i].emplace(meta, move(links[i]));
        }
        // traite les meta-actions qui ont été modifiées
        for (const auto& meta : updated) {
            auto it_meta = meta_to_actions_.find(meta);       // on cherche cette meta-action
            if (it_meta == meta_to_actions_.end()) continue;  // si elle a été éliminée entre temps, on passe
            group.swap(it_meta->second);                      // on récupère le groupe d'action dont il faut s'occuper
            for (int i = 0; i < TOTAL_LINKS; i++) {
                links[i].swap(meta_links_[i][it_meta->first]); // on récupère les liens de cette méta-action
                meta_links_[i].erase(it_meta->first);          // on efface ces liens de la base
            }
            meta_to_actions_.erase(it_meta);                   // on efface la méta-action de la base
            addMetaWithLinks(move(group), move(links));        // on relance la procédure pour l'ajouter dans la liste (en découpant ce qui doit l'être)
        }
    }
    // existe déjà, on ajoute juste les liens
    else {
        for (int i = 0; i < TOTAL_LINKS; i++)
            meta_links_[i][it->second].insert(links[i].begin(), links[i].end());
    }
}

// visit LEGAL rules to find preconditions
void Splitter6::getMetaActionsFromLegalPreconds(Splitter6::MetaMap& metaMapL) {
    // create action partitions for each group of actions preconditionned by the same fluent
    for (const auto& precond_entry : precond_to_legal) {
        for (const auto& role_entry : precond_entry.second) {
            // create a link between meta-actions and precondition
            vector<set<Fact>> links(TOTAL_LINKS);
            // normalement seul les control doivent être ignorés.
            // (si on peut pas les distinguer des autres on supprime tous les fluents actions indépendants)
            if (precond_entry.first.id != 0 && ((alternate_moves && !control_fluents[precond_entry.first.id - c_infos.id_true]) ||
                                                !action_independent[precond_entry.first.id - c_infos.id_true] ) ) // suppress dummy precondition
                links[LEGAL_PRECONDS].insert(precond_entry.first);
            metaMapL.addMetaWithLinks(role_entry.second, move(links));
        }
    }
}

void Splitter6::getMetaActionsFromNextPreconds(Splitter6::MetaMap& metaMapN) {
    // create action partitions
    for (const auto& next_entry : precond_to_does) {
        // on ne tient pas compte des liens avec les fluents qui changent systématiquement dans la position initiale
        // if (change_together_global_step[next_entry.first] == 1 && change_together_global_occ[next_entry.first] == PLAYOUT_DONE) continue;
        // create a partition for each role action set
        for (const auto& role_entry : next_entry.second) {
            for (const auto& precond_entry : role_entry.second) {
                // create a link between meta-actions and precondition
                vector<set<Fact> > links(TOTAL_LINKS);
                // seul les control peuvent et doivent être ignorés.
                // (si on peut pas les distinguer des autres on prend tout en compte)
                if (precond_entry.first.id != 0 && (!alternate_moves || !control_fluents[precond_entry.first.id - c_infos.id_true])) // suppress dummy precondition
                    links[NEXT_PRECONDS].insert(precond_entry.first);
                metaMapN.addMetaWithLinks(precond_entry.second, move(links));
            }
        }
    }
    // on garde juste le groupe d'actions qui maintient la situation (dans les mêmes circonstances / mêmes préconditions)
    // mais on indique pas de précondition, puisque ce n'est pas ces actions qui modifient la situation.
    for (const auto& next_entry : unsafe_precond_to_does) {
        // create a partition for each role action set
        for (const auto& role_entry : next_entry.second) {
            for (auto precond_entry : role_entry.second)
                metaMapN.addMetaWithLinks(precond_entry.second, vector<set<Fact> >(TOTAL_LINKS));
        }
    }
}

void Splitter6::getMetaActionsFromEffects(Splitter6::MetaMap& metaMapE) {
    map<Fact, map<RoleId, set<FactId> > > effect_to_actions;

    for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) { // pour chaque effet
        if (action_independent[t_id - c_infos.id_true]) continue; // on ignore les fluents independants des actions
        FactId t_num = t_id - c_infos.id_true;
        for (bool v : {true, false}) { // effet positif ou negatif
            Fact f(t_id, v);
            for (RoleId r = 0; r < c_infos.roles.size(); r++) {  // pour chaque role
                // on recceuille chaque groupe d'action d'un même joueur qui a le même effet
                for (FactId d_id = c_infos.id_does[r]; d_id < c_infos.id_does[r+1]; d_id++) { // pour chaque action
                    FactId d_num = d_id - c_infos.id_does.front();
                    if (noop_actions[d_num]) continue; // action sans effet
                    if (actions_efflinks[(v)?POS:NEG][d_num][t_num] > 0) // si l'action a cet effet
                        effect_to_actions[f][r].insert(d_id);
                }
            }
        }
    }

    // create action partitions
    for (auto& effect_entry : effect_to_actions) {
        // create a partition for each role action set
        for (auto& role_entry : effect_entry.second) {
            vector<set<Fact> > links(TOTAL_LINKS);
            links[EFFECTS].insert(effect_entry.first);
            metaMapE.addMetaWithLinks(move(role_entry.second), move(links));
        }
    }
}

// metaMapP = preconditions   metaMapE = effets
// utilise les metaMapP pour scinder les metaMapE
void Splitter6::splitMetaActions(Splitter6::MetaMap& metaMapP, Splitter6::MetaMap& metaMapE, Splitter6::MetaMap& metaMapDef) {
    for (auto& entryE : metaMapE.meta_to_actions_) {
        map<FactId, threeParts<FactId>> partial_match;
        map<FactId, threeParts<FactId>> part_to_split;
        bool found = false;

        for (auto& entryP : metaMapP.meta_to_actions_) {
            threeParts<FactId> inside_or_not = ins1BothIns2(entryP.second, entryE.second);
            // all actions of the partition entryP with the same effect
            if (inside_or_not.ins1.empty()) {
                vector<set<Fact> > links(TOTAL_LINKS);
                links[LEGAL_PRECONDS] = metaMapP.meta_links_[LEGAL_PRECONDS][entryP.first];
                links[EFFECTS] = metaMapE.meta_links_[EFFECTS][entryE.first];
                links[NEXT_PRECONDS] = metaMapE.meta_links_[NEXT_PRECONDS][entryE.first];
                metaMapDef.addMetaWithLinks(move(inside_or_not.both), move(links));
                found = true;
            }
            // all actions with the same effect inside the partition entryP
            else if (inside_or_not.ins2.empty())
                part_to_split.emplace(entryP.first, move(inside_or_not));
            // some actions with the same effect inside : split the partition in two
            else if (!inside_or_not.both.empty())
                partial_match.emplace(entryP.first, move(inside_or_not));
        }
        // split partitions inside part_to_split if not empty, else partitions into partial_match
        if (!found) {
            auto * to_split = &part_to_split;
            if (part_to_split.empty())
                to_split = &partial_match;
            // no precontionned partition match with an effect partition
            if (to_split->empty()) {
                cout << "=> NO PRECONDITION" << endl;
                assert(false);
                vector<set<Fact> > links(TOTAL_LINKS);
                links[EFFECTS] = metaMapE.meta_links_[EFFECTS][entryE.first];
                metaMapDef.addMetaWithLinks(move(entryE.second), move(links));
            } else {
                for (auto& tos : *to_split) {
                    vector<set<Fact> > links(TOTAL_LINKS);
                    links[EFFECTS] = metaMapE.meta_links_[EFFECTS][entryE.first];
                    links[NEXT_PRECONDS] = metaMapE.meta_links_[NEXT_PRECONDS][entryE.first];
                    links[LEGAL_PRECONDS] = metaMapP.meta_links_[LEGAL_PRECONDS][tos.first];
                    metaMapDef.addMetaWithLinks(move(tos.second.both), move(links));
                    
                    vector<set<Fact> > links2(TOTAL_LINKS);
                    links2[LEGAL_PRECONDS] = metaMapP.meta_links_[LEGAL_PRECONDS][tos.first];
                    metaMapDef.addMetaWithLinks(move(tos.second.ins1), move(links2));
                }
            }
        }
    }
}

void Splitter6::findMetaActions() {
    meta_actions = MetaMap(c_infos.values_size);

    // get potential meta-actions from actions effects
    MetaMap metaMapE(c_infos.values_size);
    getMetaActionsFromEffects(metaMapE);
//    printMeta2Actions(metaMapE, " FROM EFFECTS");
//    printMeta2Links(metaMapE, " FROM EFFECTS");

    // get potential meta-actions from NEXT premisses
    getMetaActionsFromNextPreconds(metaMapE);

    // get potential meta-actions from LEGAL premisses
    MetaMap metaMapL(c_infos.values_size);
    getMetaActionsFromLegalPreconds(metaMapL);

//    printMeta2Actions(metaMapL, " FROM LEGAL");
//    printMeta2Links(metaMapL, " FROM LEGAL");
    splitMetaActions(metaMapL, metaMapE, meta_actions);

    // supprime les noop
    for (auto it = meta_actions.meta_to_actions_.begin(); it != meta_actions.meta_to_actions_.end(); ) {
        if (!meta_actions.meta_links_[EFFECTS][it->first].empty())
            it++;
        else {
            meta_actions.actions_to_meta_.erase(it->second);
            for (int i = 0; i < TOTAL_LINKS; i++)
                meta_actions.meta_links_[i].erase(it->first);
            it = meta_actions.meta_to_actions_.erase(it);
        }
    }
}

/*******************************************************************************
 *     Jeux en série : point de passage obligé
 ******************************************************************************/

void Splitter6::addConjCrossPoint(const set<Fact>& conj, Crosspoints_infos& cross_inf, Crosspoints_infos& cross_inf_all) {
    // conj (conjonction de fluents) est un crosspoint potentiel,
    // on vérifie ce qui le préconditionne
    // on collecte les actions dont il préconditionne la légalité
    // on collecte les actions dont il préconditionne l'effet
    // on collecte les fluents indépendants des actions qu'il préconditionne
    // => a chaque étape on vérifie si il est préconditionné par un élément qui le préconditionne : si oui on l'élimine
    // => on ne collecte pas les liens réciproques au cas ou il faille éliminer ce crosspoint potentiel
    // si il passe tous les tests, on le valide comme nouveau crosspoint (avant les dernières vérifications)
    static FactId num = c_infos.values_size; // premier numéro à attribuer

    // on crée un fluent pour représenter la conjonction de fluents
    Fact c(num,true);
    // on élimine ce qui a pu être collecté pour un crosspoint finalement éliminé
    cross_inf_all.to_from[c].clear();
    cross_inf_all.from_to[c].clear();

    // les préconditions de la conjonction c'est l'union des preconditions directes et indirectes de chaque fluent de la conjonction
    for (const Fact& f : conj) // pour chaque fluent de la conjonction
        for (const Fact& p : cross_inf_all.to_from[f]) // chaque précondition directe ou indirecte du fluent
            cross_inf_all.to_from[c].insert(p);

    // trouver les actions préconditionnées par la conjonction dans les LEGALs
    // (les actions toujours légales ne sont préconditionnées par rien)
    bool no_action = true;
    VectorBool values_legal(c_infos.values_size, UNDEF);
    for (const auto& f : conj) values_legal[f.id] = (f.value)? BOOL_T:BOOL_F;
    circuit.propagate3States(values_legal);

    for (FactId l_id = c_infos.id_legal.front(); l_id < c_infos.id_legal.back(); l_id++) {
        if ((values_legal[l_id] & BOOL_T) == BOOL_T) {
            FactId l2d = l_id - c_infos.id_legal.front() + c_infos.id_does.front();
            cross_inf_all.from_to[c].insert(Fact(l2d, true));
            // si il préconditionne et est préconditionné par la même action : ce n'est pas un crosspoint
            if (cross_inf_all.to_from[c].find(Fact(l2d, true)) != cross_inf_all.to_from[c].end()) return;
            // on ajoute les effets indirects des actions
            for (Fact e : cross_inf.from_to[Fact(l2d, true)])
                cross_inf_all.from_to[c].insert(e);
            no_action = false;
        }
    }

    // on cherche les NEXTs qui peuvent toujours être potentiellement vrais avec la conjonction
    // on collecte les actions
    for (const auto& next_entry : precond_to_does) {
        if (conj.find(next_entry.first) != conj.end()) continue; // la conjonction ne peut pas avoir comme effet ce qu'elle contient déjà...
        for (const auto& role_entry : next_entry.second) {
            for (const auto& precond_entry : role_entry.second) {
                if (conj.find(precond_entry.first) == conj.end()) continue; // la conjonction n'a rien à voir avec ces actions...
                FactId n_id = next_entry.first.id - c_infos.id_true + c_infos.id_next;

                // on vérifie que la conjonction n'empêche pas la conclusion de changer (l'action maintient juste la situation ou bien est illégale)
                // et qu'elle n'est pas suffisante pour changer le NEXT sans l'action
                // not(true(f)), précondition vraie, conjonction vraie, on propage le signal => nexf(f) potentiellement vrai ?
                VectorBool values_next(c_infos.values_size, UNDEF);
                values_next[0] = BOOL_T;
                values_next[1] = BOOL_F;
                values_next[precond_entry.first.id] = (precond_entry.first.value)? BOOL_T:BOOL_F;
                for (const auto& f : conj) values_next[f.id] = (f.value)? BOOL_T:BOOL_F;
                values_next[next_entry.first.id] = (next_entry.first.value)? BOOL_F:BOOL_T; // true(f) est faux aura-t'on next(f) ?
                circuit.propagate3StatesStrict(values_next);

                if (next_entry.first == precond_entry.first || values_next[n_id] == UNDEF) {

                    for (const FactId& d_id : precond_entry.second) {
                        if (values_next[d_id - c_infos.id_does.front() + c_infos.id_legal.front()] != BOOL_F) {
                            no_action = false;
                            cross_inf_all.from_to[c].insert(Fact(d_id, true));
                            // si il préconditionne et est préconditionné par la même action : ce n'est pas un crosspoint
                            if (cross_inf_all.to_from[c].find(Fact(d_id, true)) != cross_inf_all.to_from[c].end()) return;
                            // on ajoute les effets indirects des actions
                            for (Fact e : cross_inf.from_to[Fact(d_id, true)])
                                cross_inf_all.from_to[c].insert(e);
                        }
                    }
                }
            }
        }
    }
    if (no_action) return; // ce n'est finalement pas un crosspoint potentiel puisqu'il ne préconditionne aucune action (toutes illégales)


    // on cherche en vrac les fluents indépendants des actions que la conjonction peut preconditionner
    set<FactId> to_check;
    VectorBool values_precond(c_infos.values_size, UNDEF);
    for (const auto& f : conj) // on règle chaque composant de la conjonction
        values_precond[f.id] = (f.value)? BOOL_T : BOOL_F;
    circuit.propagate3States(values_precond);
    for (FactId id_n = c_infos.id_next; id_n < c_infos.id_goal.front(); id_n++) {
        if (!action_independent[id_n - c_infos.id_next]) continue;
        FactId id_n2t = id_n - c_infos.id_next + c_infos.id_true;
        // on garde que ceux qui ont changé de valeur
        // on garde que ceux qui ont changé de valeur
        if (values_precond[id_n] == BOOL_T && values_precond[id_n2t] != BOOL_T) // il faut vérifier qu'il est pas juste maintenu vrai
            to_check.insert(id_n2t);
        if (values_precond[id_n] == BOOL_F && values_precond[id_n2t] != BOOL_F) { // quelque soit sa valeur il devient faux
            cross_inf_all.from_to[c].insert(Fact(id_n2t, false));
            // si il préconditionne et est préconditionné par le même fluent : ce n'est pas un crosspoint
            if (cross_inf_all.to_from[c].find(Fact(id_n2t, false)) != cross_inf_all.to_from[c].end()) return;
            // + preconditionne indirectement d'autres fluents
            for (Fact e : cross_inf_all.from_to[Fact(id_n2t, false)])
                cross_inf_all.from_to[c].insert(e);
        }
    }
    // on vérifie qu'ils sont bien préconditionnés par cette conjonction (ils changent vraiment)
    for (FactId t_id : to_check) {
        // est-ce que la précondition permet vraiment le passage de BOOL_F à BOOL_T (ou MAYB_T) ?
        Fact t_fact(t_id, true);
        FactId t2n_id = t_id + c_infos.id_next - c_infos.id_true;
        VectorBool values2(c_infos.values_size, UNDEF);
        values2[t_id] = BOOL_F;   // est-ce que la précondition peut le faire changer ?
        values2[t2n_id] = BOOL_F; // il aura la même valeur par défaut...
        values2[0] = BOOL_T;
        values2[1] = BOOL_F;
        for (const auto& f : conj) // on règle chaque composant de la conjonction
            values_precond[f.id] = (f.value)? BOOL_T : BOOL_F;

        // propage avec 5 états (UNDEF, BOOL_T, BOOL_F, MAYB_T, MAYB_F), la sortie ne change que si au moins une des entrées est différente de UNDEF
        circuit.propagate3StatesCcl(values2);

        // si t_val : la précondition peut changer sa valeur vers vrai
        if (values2[t2n_id] == BOOL_T || values2[t2n_id] == MAYB_T) {
            cross_inf_all.from_to[c].insert(t_fact);
            // si il préconditionne et est préconditionné par le même fluent : ce n'est pas un crosspoint
            if (cross_inf_all.to_from[c].find(t_fact) != cross_inf_all.to_from[c].end()) { cout << "éliminé 4" << endl; return; }

            // + preconditionne indirectement d'autres fluents
            for (Fact e : cross_inf_all.from_to[t_fact])
                cross_inf_all.from_to[c].insert(e);
        }
    }

    // on enregistre ce nouveau fluent qui représente une conjonction
    fluent_2_conj_crosspoint[c] = conj;
    crosspoint.insert(c);

    num++; // on passe à l'id suivant pour la prochaine conjonction
}

void Splitter6::findCrossPoint() {
    // pour chaque fluent on cherche ce qui l'amène (action ou autre fluent) et ce qu'il engendre (autre fluent ou action)
    Crosspoints_infos cross_inf;
    {
        // trouver les préconditions aux LEGALs y compris les fluents indépendants des actions
        for (const auto& precond_entry : precond_to_legal) {
            for (const auto& role_entry : precond_entry.second) {
                for (FactId d_id : role_entry.second) {
                    if (precond_entry.first.id != 0) {
                        cross_inf.from_to[precond_entry.first].insert(Fact(d_id, true));
                        cross_inf.to_from[Fact(d_id, true)].insert(precond_entry.first);
                    }
                }
            }
        }

        // trouver les préconditions aux NEXTs y compris les fluents indépendants des actions
        for (const auto& next_entry : precond_to_does) {
            for (const auto& role_entry : next_entry.second) {
                for (const auto& precond_entry : role_entry.second) {
                    if (precond_entry.first.id < c_infos.id_true) continue;
                    for (const FactId& d_id : precond_entry.second) {
                       cross_inf.from_to[precond_entry.first].insert(Fact(d_id, true));
                       cross_inf.to_from[Fact(d_id, true)].insert(precond_entry.first);
                    }
                }
            }
        }

        // trouver les effets
        for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) { // pour chaque effet
            if (action_independent[t_id - c_infos.id_true]) continue; // on ignore les fluents independants des actions
            FactId t_num = t_id - c_infos.id_true;
            for (bool t_val : {true, false}) { // effet positif ou negatif
                Fact f(t_id, t_val);
                for (FactId d_id = c_infos.id_does.front(); d_id < c_infos.id_does.back(); d_id++) { // pour chaque action
                    FactId d_num = d_id - c_infos.id_does.front();
                    if (actions_efflinks[(t_val)?POS:NEG][d_num][t_num] > 0) { // si l'action a cet effet
                        cross_inf.to_from[f].insert(Fact(d_id, true));
                        cross_inf.from_to[Fact(d_id, true)].insert(f);
                    }
                }
            }
        }

        // trouver les liens entre les fluents indépendants des actions
        for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) {
            if (!action_independent[t_id - c_infos.id_true]) continue; // ce n'est pas un fluent action_independent
            FactId t2n_id = t_id - c_infos.id_true + c_infos.id_next;

            // on recherche les préconditions
            VectorBool values(c_infos.values_size, NOT_USED);
            values[t2n_id] = USED_TRUE;
            // retro-propage le signal avec 3 etats en inversant juste dans les NOT pour trouver les preconditions
            circuit.backPropagateNot(values);

            // est-que le fluent vrai se maintient lui même ?
            VectorBool values_stay(c_infos.values_size, UNDEF);
            values_stay[0] = BOOL_T;
            values_stay[1] = BOOL_F;
            values_stay[t_id] = BOOL_T;
            // propage le signal avec 3 etats pour voir si le fluent peut se maintenir lui-même
            circuit.propagate3StatesStrict(values_stay);

            // on cherche les précondition pour que le fluent devienne vrai ou faux
            for (bool t_val : {true, false}) {
                if (!t_val && values_stay[t2n_id] == BOOL_T) continue; // ce fluent est un latch qui reste toujours vrai, pas de précondition pour devenir faux

                for (FactId p_id = c_infos.id_true; p_id < c_infos.id_does.front(); p_id++) {
                    if (p_id == t_id || values[p_id] == NOT_USED) continue; // ce n'est pas une précondition ou bien elle est identique à la conclusion
                    // on vérifie quelles préconditions peuvent changer la conclusion
                    set<bool> precond_values;
                    if ((values[p_id] & USED_TRUE) == USED_TRUE) precond_values.insert(true);
                    if ((values[p_id] & USED_FALSE) == USED_FALSE) precond_values.insert(false);
                    for (bool p_val : precond_values) {
                        // si t_val : est-ce que la précondition permet le passage de BOOL_F à BOOL_T (ou MAYB_T) ?
                        // si !t_val : est-ce que la précondition peut forcer le maintient à BOOL_T (si oui, son inverse est une précondition pour BOOL_T -> BOOL_F)
                        Fact t_fact(t_id, t_val);
                        Fact p_fact(p_id, p_val);
                        VectorBool values2(c_infos.values_size, UNDEF);
                        values2[t_id] = (t_val)?BOOL_F:BOOL_T;   // est-ce que la précondition peut le faire changer ?
                        values2[t2n_id] = (t_val)?BOOL_F:BOOL_T; // il aura la même valeur par défaut...
                        values2[0] = BOOL_T;
                        values2[1] = BOOL_F;
                        values2[p_id] = (p_val)? BOOL_T : BOOL_F;

                        // propage avec 5 états (UNDEF, BOOL_T, BOOL_F, MAYB_T, MAYB_F), la sortie ne change que si au moins une des entrées est différente de UNDEF
                        circuit.propagate3StatesCcl(values2);

                        // si t_val : la précondition peut changer sa valeur vers vrai
                        if (t_val && (values2[t2n_id] == BOOL_T || values2[t2n_id] == MAYB_T)) {
                            cross_inf.from_to[p_fact].insert(t_fact);
                            cross_inf.to_from[t_fact].insert(p_fact);
                        }
                        // si !t_val : la précondition peut empêcher de changer la valeur vers faux
                        else if (values2[t2n_id] == BOOL_T) {
                            cross_inf.from_to[Fact(p_id, !p_val)].insert(t_fact);
                            cross_inf.to_from[t_fact].insert(Fact(p_id, !p_val));
                        }
                    }
                }
            }
        }
    }

    // deboggage : affichage
    { /*
        cout << "--------------------------------SEARCH A CROSSPOINT" << endl;
        cout << "--------------------------------DIRECT LINKS" << endl;
        for (FactId f_id = c_infos.id_true; f_id < c_infos.id_does.back(); f_id++) {
            for (bool val : {true, false}) {
                if (!val && f_id >= c_infos.id_does.front()) continue;
                Fact f(f_id, val);
                printTermOrId(f, cout);
                cout << "=> ";
                if (cross_inf.from_to.find(f) != cross_inf.from_to.end()) {
                    for (const Fact& to : cross_inf.from_to[f])
                        printTermOrId(to, cout);
                }
                cout << endl << endl;
                printTermOrId(f, cout);
                cout << "<= ";
                if (cross_inf.from_to.find(f) != cross_inf.from_to.end()) {
                    for (const Fact& to : cross_inf.to_from[f])
                        printTermOrId(to, cout);
                }
                cout << endl << endl;
            }
        }
        cout << endl;
        createGraphvizSerial("serial", cross_inf);
    */ }

    
    // Pour chaque fluent on collecte ce qui l'amène indirectement ou ce qu'il engendre indirectement
    Crosspoints_infos cross_inf_all;
    {
        // pour chaque précondition A, pour chaque élément préconditionné B, on ajoute les éléments C qui sont préconditionnés par B (et la réciproque)
        for (auto& entry : cross_inf.from_to) {
            auto& ref = cross_inf_all.from_to[entry.first];
            deque<Fact> to(entry.second.begin(), entry.second.end());

            while (!to.empty()) {
                auto r = ref.insert(to.back());
                cross_inf_all.to_from[to.back()].insert(entry.first);
                if (r.second) {
                    for (const Fact& to2 : cross_inf.from_to[to.back()])
                        to.push_front(to2);
                }
                to.pop_back();
            }
        }
    }

    // deboggage : affichage
    { /*
        cout << "--------------------------------INDIRECT LINKS" << endl;
        for (FactId f_id = c_infos.id_true; f_id < c_infos.id_does.back(); f_id++) {
            for (bool val : {true, false}) {
                if (!val && f_id >= c_infos.id_does.front()) continue;
                Fact f(f_id, val);
                printTermOrId(f, cout);
                cout << "=> ";
                if (cross_inf_all.from_to.find(f) != cross_inf_all.from_to.end()) {
                    for (const Fact& to : cross_inf_all.from_to[f])
                        printTermOrId(to, cout);
                }
                cout << endl << endl;
                printTermOrId(f, cout);
                cout << "<= ";
                if (cross_inf_all.from_to.find(f) != cross_inf_all.from_to.end()) {
                    for (const Fact& to : cross_inf_all.to_from[f])
                        printTermOrId(to, cout);
                }
                cout << endl << endl;
            }
        }
       cout << endl;
       createGraphvizSerial("serial_indirect", cross_inf_all);
    */ }

    // on filtre les crosspoint potentiels
    std::set<Fact> conj_crosspoint_component;
    {
        // IL DOIT PRECONDITIONNER DES ACTIONS : sinon à quoi bon un second jeu sans action ?
        // pour chaque fluent qui a des préconditions (fluents ou actions)
        //      si (il est pas dans l'état initial ET il préconditionne DIRECTEMENT au moins une action)
        //          on l'ajoute dans les crosspoints potentiels
        for (const auto& entry : cross_inf_all.to_from) {
            if (entry.first.id < c_infos.id_does.front()       // c'est un fluent (pas une action)
                && !cross_inf_all.to_from[entry.first].empty() // il est préconditionné par quelque chose
                && (init_values[entry.first.id] == BOOL_T) != entry.first.value   // il n'est pas dans l'état initial
                && !cross_inf.from_to[entry.first].empty()     // il préconditionne quelque chose
                && cross_inf.from_to[entry.first].rbegin()->id >= c_infos.id_does.front()) { // le dernier élément au moins (id le plus élevé) est une action

                auto intersect = ins1BothIns2(cross_inf_all.to_from[entry.first], cross_inf_all.from_to[entry.first]);
                if (intersect.both.empty()) {        // il préconditionne et est préconditionné par deux groupes d'éléments distincts
                    crosspoint.insert(entry.first);  // c'est un point de croisement
                } else {
                    conj_crosspoint_component.insert(entry.first); // c'est peut-être un composant d'une conjonction constituant un point de passage obligé (crosspoint)
                }
            }
        }
    }

    // deboggage : affichage
    { /*
        cout << "------------------ CROSSPOINTS" << endl;
        for (const auto& f : crosspoint) {
            printTermOrId(f, cout);
        }
        cout << endl;

        cout << "------------------ POTENTIAL CONJ-CROSSPOINT COMPONENTS " << endl;
        for (const auto& f : conj_crosspoint_component) {
            printTermOrId(f, cout);
        }
        cout << endl;
     */ }

    // une conjonction de fluents crosspoint potentiels peut être un crosspoint, on collecte les conjonctions de fluents crosspoint potentiels qui interviennent plusieurs fois
    map<set<Fact>, set<Fact>> potential_conj_crosspoint; // conjonction de fluents -> groupe d'actions et fluents qui sont engendrés
    {
        for (const auto& entry : cross_inf.to_from) {
            auto intersect = ins1BothIns2(conj_crosspoint_component, entry.second);
            if (intersect.both.size() > 1)
                potential_conj_crosspoint[intersect.both].insert(entry.first);
        }
    }

    // deboggage : affichage
    { /*
        // les conjonctions de fluents crosspoints potentiels
        cout << "------------------ POTENTIAL CONJ CROSSPOINTS A TRIER" << endl;
        for (const auto& p : potential_conj_crosspoint) {
            cout << "utilisé " << p.second.size() << " fois : ";
            for (const auto& f : p.first) {
                printTermOrId(f, cout);
            }
            cout << endl << " => ";
            for (const auto& f : p.second) {
                printTermOrId(f, cout);
            }
            cout << endl;
        }
        cout << endl;
     */ }

    // pour chaque conj-cross potentiel,
    // on vérifie si il préconditionne des actions (si elles sont pas toujours légales),
    // si oui on on crée un fluent et on l'ajoute aux crosspoints potentiels à tester
    // on collecte ce qui l'amène et ce qu'il engendre dans cross_inf_all
    {
        for (const auto& p : potential_conj_crosspoint) {
            if (p.second.size() > 1) {
                addConjCrossPoint(p.first, cross_inf, cross_inf_all);
            }
        }
    }

    // deboggage : affichage
    { /*
        cout << "--------------------------------INDIRECT LINKS DES CONJ-CROSSPOINT" << endl;
        for (const auto& entry : fluent_2_conj_crosspoint) {
            const Fact& f = entry.first;

            printTermOrId(f, cout);
            cout << " : ";
            for (Fact c : entry.second) printTermOrId(c, cout);
            cout << endl << endl;

            printTermOrId(f, cout);
            cout << "=> ";
            if (cross_inf_all.from_to.find(f) != cross_inf_all.from_to.end()) {
                for (const Fact& to : cross_inf_all.from_to[f])
                    printTermOrId(to, cout);
            }
            cout << endl << endl;
            printTermOrId(f, cout);
            cout << "<= ";
            if (cross_inf_all.from_to.find(f) != cross_inf_all.from_to.end()) {
                for (const Fact& to : cross_inf_all.to_from[f])
                    printTermOrId(to, cout);
            }
            cout << endl << endl;
        }
        // les conjonctions de fluents crosspoints potentiels
        cout << "------------------ POTENTIAL CONJ CROSSPOINTS" << endl;
        for (const auto& p : fluent_2_conj_crosspoint) {
            printTermOrId(p.first, cout);
            cout << " : ";
            for (const auto& f : p.second) {
                printTermOrId(f, cout);
            }
            cout << endl;
        }
        cout << endl;
    */ }


    {
        // dernière vérification
        set<Fact> valid_crosspoint;
        for (const Fact& c : crosspoint) {
            // si un crosspoint est uniquement engendré par un autre crosspoint ou l'état initial, on l'oublie.
            if (!cross_inf_all.to_from[c].empty()) {
                for (const auto& p : cross_inf_all.to_from[c]) {  // pour chacune de ses préconditions
                    // si c'est une action ou si elle n'est pas dans l'état initial et ce n'est pas un crosspoint
                    if ((p.id >= c_infos.id_does.front() && p.id < c_infos.id_end) || ((init_values[p.id] == BOOL_T) != p.value && crosspoint.find(p) == crosspoint.end())) {
                        valid_crosspoint.insert(c);  // on valide
                        break;
                    }
                }
            }
        }

        for (auto it1 = valid_crosspoint.begin(); it1 != valid_crosspoint.end(); ) {
            if (it1->id < c_infos.id_end) { it1++; continue; } // c'est pas une conjonction
            bool another_inside = false;
            // si le crosspoint n'est pas encore éliminé et qu'il en inclue un autre, on l'oublie
            auto it2 = it1; it2++;
            for ( ; it2 != valid_crosspoint.end(); ) {
                if (it2->id < c_infos.id_end) { it2++; continue; } // une conjonction ne contient que des fluents qui ne sont pas des crosspoints à eux seuls
                auto intersect = ins1BothIns2(fluent_2_conj_crosspoint[*it1], fluent_2_conj_crosspoint[*it2]);
                if (intersect.ins1.empty()) it2 = valid_crosspoint.erase(it2);
                else if (intersect.ins2.empty()) { another_inside = true; break; }
                else it2++;
            }
            if (another_inside) // il en contient un autre, on l'efface
                it1 = valid_crosspoint.erase(it1);
            else it1++;
        }
        swap(crosspoint, valid_crosspoint);
    }
    for (const auto& c : crosspoint) {
        from_crosspoint_to.emplace(c, move(cross_inf_all.from_to[c]));
        to_crosspoint_from.emplace(c, move(cross_inf_all.to_from[c]));
    }
}


void Splitter6::splitSerialGames() {
    // on coupe les liens entre un sous-jeu et les fluents du sous-jeu précédent
    // (identifiés comme point de croisement)
    // les points de croisement ne sont plus des préconditions à la légalité
    for (auto& entry : meta_actions.meta_links_[LEGAL_PRECONDS]) {
        for (const Fact& f : crosspoint) {
            if (f.id >= c_infos.id_end)
                for (const Fact& c : fluent_2_conj_crosspoint[f]) {
                    entry.second.erase(c);
                    entry.second.erase(Fact(c.id,!c.value));
                }
            else {
                entry.second.erase(f);
                entry.second.erase(Fact(f.id,!f.value));
            }
        }
    }
    // les points de croisement ne sont plus des préconditions au changement
    for (auto& entry : meta_actions.meta_links_[NEXT_PRECONDS]) {
        for (const Fact& f : crosspoint) {
            if (f.id >= c_infos.id_end)
                for (const Fact& c : fluent_2_conj_crosspoint[f]) {
                    entry.second.erase(c);
                    entry.second.erase(Fact(c.id,!c.value));
                }
            else {
                entry.second.erase(f);
                entry.second.erase(Fact(f.id,!f.value));
            }
        }
    }
    // les points de croisement ne sont plus des préconditions pour les fluents indépendants des actions
    for (const Fact& f : crosspoint) {
        if (f.id >= c_infos.id_end) { // une expression
            for (auto& entry : fluents_prems)
                for (const Fact& c : fluent_2_conj_crosspoint[f])
                    entry.second.erase(c.id);
        } else { // un fluent
            for (auto& entry : fluents_prems)
                entry.second.erase(f.id);
        }
    }
}

/***** Splitter : graph for debug - jeux en série *****************************/

/* create a graphViz file repesenting the (sub?)game */
void Splitter6::createGraphvizSerial(const string& name, const Crosspoints_infos& infos) const {
    // create a graphviz .gv file into log/ directory
    string gv_file = log_dir + name + ".gv";
    string ps_file = log_dir + name + ".ps";

    ofstream fp(gv_file);
    if (!fp) {
        cerr << "Error in createGraphvizFile: unable to open file ";
        cerr << gv_file << endl;
        exit(EXIT_FAILURE);
    }
    fp << graphvizSerial(name, infos);
    fp.close();

    cout << endl;
    cout << "graphviz .gv file created, can't create the .ps file ";
    cout << "myself, use this to create .ps file :" << endl;
    cout << "dot -Tps " << gv_file << " -o " << ps_file << endl;
}

/* give the graphViz representation of the (sub?)game with detection of effects for each joint move */
string Splitter6::graphvizSerial(const string& name, const Crosspoints_infos& infos) const {
    stringstream nodes;
    stringstream links;

    // nodes
    for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) {
        nodes << "    " << t_id << " [label=\"" <<  *c_infos.vars[t_id] << "\",color=blue,peripheries=1,";
        if (action_independent[t_id -  c_infos.id_true]) nodes << "penwidth=3];" << endl;
        else nodes << "penwidth=1];" << endl;
        nodes << "    " << t_id << "0000000000000 [label=\"" <<  *c_infos.vars[t_id] << "\",color=red,peripheries=1,";
        if (action_independent[t_id -  c_infos.id_true]) nodes << "penwidth=3];" << endl;
        else nodes << "penwidth=1];" << endl;
    }
    for (FactId d_id = c_infos.id_does.front(); d_id < c_infos.id_does.back(); d_id++) {
        nodes << "    " << d_id << " [label=\"" <<  *c_infos.vars[d_id] << "\",color=black,shape=box,peripheries=1,penwidth=1];" << endl;
    }
    // links
    for (const auto& entry : infos.from_to) {
        for (Fact to : entry.second) {
            links << "    " << entry.first.id << ((entry.first.value)?"":"0000000000000") << " -> " << to.id << ((to.value)?"":"0000000000000");
            if (entry.first.value)
                links << " [color=darkgreen constraint=true penwidth=5];" << endl;
            else
                links << " [color=red constraint=true penwidth=5];" << endl;
        }
    }

    stringstream text;
    text << "/*--------------- graphe " << name << " ---------------*/" << endl;
    text << "digraph game {" << endl;
    text << "    rankdir=LR;" << endl;
    text << "    overlap=false;" << endl;
    text << "    size=\"4, 10\"; " << endl;
    text << "    ranksep=\"3 equally\";" << endl;
    text << "    compound=true;" << endl;
    text << nodes.str();
    text << links.str();
    text << "}";
    
    return text.str();
}

/*******************************************************************************
 *     Identification des sous-jeux
 ******************************************************************************/

void Splitter6::findSubgames() {
    vector<FactId> with_effect(nb_inputs, false); // pour collecter les actions absentes des meta-actions
    subgames_strong.clear();
    fact_to_subgame_strong.clear();
    subgames.clear();
    fact_to_subgame.clear();

    // SUBGAMES WITH STRONG LINKS
    // pour chaque meta-action
    for (const auto& meta_entry : meta_actions.meta_to_actions_) {
        FactId current_id = next_subgame_id++;
        subgames_strong[current_id].insert(meta_entry.first);
        fact_to_subgame_strong[meta_entry.first] = current_id;
        // utiliser les liens de type EFFECTS / LEGAL_PRECONDS / NEXT_PRECONDS
        for (size_t type = 0; type < meta_actions.meta_links_.size(); type++) {
            for (const Fact& f : meta_actions.meta_links_[type].at(meta_entry.first)) {
                if (type == EFFECTS) {
                    // effet moyen de la méta-action sur ce fluent
                    double mean = 0;
                    for (FactId d_id : meta_entry.second) {
                        if (f.value)
                            mean += actions_efflinks[POS][d_id - c_infos.id_does.front()][f.id - c_infos.id_true];
                        else
                            mean += actions_efflinks[NEG][d_id - c_infos.id_does.front()][f.id - c_infos.id_true];
                    }
                    mean /= meta_entry.second.size();
                    if (mean < THR_LOW_LINK)
                        continue;
                }
                FactId old_id = fact_to_subgame_strong[f.id];
                if (old_id == 0) {
                    fact_to_subgame_strong[f.id] = current_id;
                    subgames_strong[current_id].insert(f.id);
                }
                else if (old_id != current_id) {
                    for (FactId g : subgames_strong[old_id]) fact_to_subgame_strong[g] = current_id;
                    subgames_strong[current_id].insert(subgames_strong[old_id].begin(), subgames_strong[old_id].end());
                    subgames_strong.erase(old_id);
                }
            }
        }
        // action avec un effet
        for (FactId d_id : meta_entry.second)
            with_effect[d_id - c_infos.id_does.front()] = true;
    }
    // utiliser les liens entre fluents indépendants des actions : fluents_prems
    for (auto& entry : fluents_prems) {
        if (!action_independent[entry.first - c_infos.id_true]) continue; // ce fluent n'est plus considéré comme indépendant des actions
        FactId current_id = fact_to_subgame_strong[entry.first];
        if (current_id == 0) {
            current_id = next_subgame_id++;
            subgames_strong[current_id].insert(entry.first);
            fact_to_subgame_strong[entry.first] = current_id;
        }
        for (FactId f_id : entry.second) {
            FactId old_id = fact_to_subgame_strong[f_id];
            if (old_id == 0) {
                fact_to_subgame_strong[f_id] = current_id;
                subgames_strong[current_id].insert(f_id);
            }
            else if (old_id != current_id) {
                for (FactId g : subgames_strong[old_id])
                    fact_to_subgame_strong[g] = current_id;
                subgames_strong[current_id].insert(subgames_strong[old_id].begin(), subgames_strong[old_id].end());
                subgames_strong.erase(old_id);
            }
        }
    }

    // SUBGAMES WITH ALL LINKS
    subgames = subgames_strong;
    fact_to_subgame = fact_to_subgame_strong;

    // pour chaque meta-action
    for (const auto& meta_entry : meta_actions.meta_to_actions_) {
        FactId current_id = fact_to_subgame[meta_entry.first];
        if (current_id == 0) {
            current_id = next_subgame_id++;
            subgames[current_id].insert(meta_entry.first);
            fact_to_subgame[meta_entry.first] = current_id;
        }
        // utiliser les liens de type EFFECTS
        for (const Fact& f : meta_actions.meta_links_[EFFECTS].at(meta_entry.first)) {
            FactId old_id = fact_to_subgame[f.id];
            if (old_id == 0) {
                fact_to_subgame[f.id] = current_id;
                subgames[current_id].insert(f.id);
            }
            else if (old_id != current_id) {
                for (FactId g : subgames[old_id]) fact_to_subgame[g] = current_id;
                subgames[current_id].insert(subgames[old_id].begin(), subgames[old_id].end());
                subgames.erase(old_id);
            }
        }
    }

    // actions dans aucune meta-action : noop
    for (FactId d_num = 0; d_num < nb_inputs; d_num++)
        if (!with_effect[d_num])
            noop_actions[d_num] = true;
}

/*******************************************************************************
 *     Unir les sous-jeux en fonction des subgoals
 ******************************************************************************/

std::set<std::set<Fact> > Splitter6::partition_victory_sets(vic_type vic, set<SubgameId>& action_dependant, std::map<FactId, SubgameId>& fact_to_subgame) {
    std::set<std::set<Fact> > new_cond_victory_sets;
    for (auto& vset_old : cond_goalterm_sets[vic]) {
        set<Fact> vset;
        set<Fact> vset_act_ind;
        for (Fact f : vset_old) {
            if (action_dependant.find(fact_to_subgame[f.id]) != action_dependant.end())
                vset.insert(f);
            else
                vset_act_ind.insert(f);
        }
        //if (vset.size() > 1 || (!vset.empty() && vic == VICTORY)) // on garde les conditions de victoire non vides et les sous-buts constitués de plusieurs fluents
        if (!vset.empty())
            new_cond_victory_sets.insert(move(vset));
        //if (vset_act_ind.size() > 1 || (!vset_act_ind.empty() && vic == VICTORY))
        if (!vset_act_ind.empty())
            new_cond_victory_sets.insert(move(vset_act_ind));
    }
    //cond_goalterm_sets[vic].swap(new_cond_victory_sets);
    return new_cond_victory_sets;

}

void Splitter6::checkWithSubGoals() {
    // sous jeux (in)dépendants des actions, intervenant pour goal ou terminal
    set<SubgameId> action_dependant;
//    set<SubgameId> in_term;
    set<SubgameId> usefull_for_term;
    set<SubgameId> in_goal;   set<SubgameId> usefull_for_partial_goal;   set<SubgameId> usefull_for_goal;

    // fluents intervenant dans terminal
    VectorBool values_term(c_infos.values_size, BOOL_F);
    values_term[c_infos.id_terminal] = BOOL_T;
    circuit.backPropagate(values_term);
    // fluents intervenant dans goal
    VectorBool values_goal(c_infos.values_size, BOOL_F);
    for (FactId g_id = c_infos.id_goal.front(); g_id < c_infos.id_goal.back(); g_id++)
        values_goal[g_id] = BOOL_T;
    circuit.backPropagate(values_goal);


    // sous-jeu dépendant des actions
    for (const auto& entry : subgames) {
        if (*entry.second.rbegin() >= c_infos.id_does.front())
            action_dependant.insert(entry.first);
    }

    *out_dcmp << "=> " << action_dependant.size() << " action dep before checking - " << subgames.size() - action_dependant.size() << " action indep" << endl;

    // SUR-DECOMPOSITION ? VERIFIONS QUE LES SOUS_JEUX ASSURENT DES POINTS OU LA TERMINAISON DU JEU
    // si il y a plusieurs sous-jeux dépendants des actions (ils peuvent être sur-décomposés)
    if (action_dependant.size() > 1) {
        // sous-jeux intervenant dans goal (contenant plus d'un fluent)
        for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++)
            if (values_goal[t_id] == BOOL_T && fact_to_subgame[t_id] != 0 && subgames[fact_to_subgame[t_id]].size() > 1)
                in_goal.insert(fact_to_subgame[t_id]);

        // on scinde les sous-buts pour ne pas réunir les sous-jeu dépendants ou indépendants des actions
        auto cond_set = partition_victory_sets(PART_VICTORY, action_dependant, fact_to_subgame);
        // on examine les sous-buts
        for (const auto& vset : cond_set) {
            SubgameId s_id = 0; // id non défini
            for (Fact t : vset) {
                SubgameId old = fact_to_subgame[t.id];
                if (old == 0) {
                    old = next_subgame_id++;
                    fact_to_subgame[t.id] = old;
                    subgames[old].insert(t.id);
                }
                // ce sous-jeu va disparaitre ou bien son utilité est confirmée
                in_goal.erase(old);
                // récupère ou modifie l'id du sous-jeu
                if (s_id == 0) {
                    s_id = old; // premier fluent, on récupérèe l'id du sous-jeu correspondant
                    // ce sous-jeu aporte des points, c'est confirmé
                    usefull_for_partial_goal.insert(s_id);
                }
                else if (old != s_id) {
                    // on change son id
                    for (FactId g : subgames[old]) fact_to_subgame[g] = s_id;
                    subgames[s_id].insert(subgames[old].begin(), subgames[old].end());
                    subgames.erase(old);
                }
            }
        }
        // si il reste plusieurs sous-jeux utiles pour goal
        if (in_goal.size() > 1) {
            // on scinde les conditions de victoire pour ne pas réunir les sous-jeu dépendants ou indépendants des actions
            auto cond_set = partition_victory_sets(VICTORY, action_dependant, fact_to_subgame);
            // on examine les conditions de victoire
            for (const auto& vset : cond_set) {
                SubgameId s_id = 0; // id non défini
                for (Fact t : vset) {
                    SubgameId old = fact_to_subgame[t.id];
                    if (usefull_for_partial_goal.find(old) != usefull_for_partial_goal.end()) continue; // sous-jeu qui assure des points, à ne pas unifier avec d'autres.
                    if (old == 0) {
                        old = next_subgame_id++;
                        fact_to_subgame[t.id] = old;
                        subgames[old].insert(t.id);
                    }
                    // ce sous-jeu va disparaitre ou bien son utilité est confirmée
                    in_goal.erase(old);
                    // récupère ou modifie l'id du sous-jeu
                    if (s_id == 0) {
                        s_id = old; // premier fluent, on récupérèe l'id du sous-jeu correspondant
                        // ce sous-jeu aporte des points, c'est confirmé
                        usefull_for_goal.insert(s_id);
                    }
                    else if (old != s_id) {
                        // on change son id
                        for (FactId g : subgames[old]) fact_to_subgame[g] = s_id;
                        subgames[s_id].insert(subgames[old].begin(), subgames[old].end());
                        subgames.erase(old);
                    }
                }
            }
        }
        // sinon il ne reste qu'un seul sous-jeux utiles pour goal (ou zéro)
        else if (in_goal.size() == 1) {
            usefull_for_goal.insert(*in_goal.begin());
            in_goal.clear();
        }
        // le découpage de tous les sous-jeux utiles pour goal doit être confirmé maintenant...
        if (!in_goal.empty()) { // si ce n'est pas le cas
            *out_dcmp << "WARNING : anomalie de décomposition, sous-jeux ";
            for (auto id : in_goal)
                *out_dcmp << id << " ";
            *out_dcmp << "intervenant dans GOAL sans assurer de points..." << endl;
#ifndef PLAYERGGP_DEBUG
            cout << endl << "WARNING : anomalie de décomposition, sous-jeux intervenant dans GOAL sans assurer de points..." << endl;
#endif
//            auto it_sub = subgames.begin();
//            SubgameId s_id = it_sub->first;
//            ++it_sub;
//            while (it_sub != subgames.end()) {
//                SubgameId old = it_sub->first;
//                for (FactId g : it_sub->second) fact_to_subgame[g] = s_id;
//                subgames[s_id].insert(subgames[old].begin(), subgames[old].end());
//                it_sub = subgames.erase(it_sub);
//            }
////            in_term.clear();
//            usefull_for_goal.clear();
//            usefull_for_goal.insert(s_id);
        }

        // on regroupe les sous-jeux utiles pour goal
        usefull_for_goal.insert(usefull_for_partial_goal.begin(), usefull_for_partial_goal.end());
    }

    // SOUS-DECOMPOSITION ? PEUT-ON DECOMPOSER SANS LIENS FAIBLES ?
    // si il y a un seul sous-jeu dépendant des actions (il est peut être sous-décomposé)
    else {
        bool decomp = false;
        action_dependant.clear();
        bool lonely_action = false;
        // sous jeux action-dépendants avec uniquement les liens forts
        for (const auto& entry : subgames_strong) {
            if (*entry.second.rbegin() >= c_infos.id_does.front()) {
                action_dependant.insert(entry.first);
                if (entry.second.size() == 1) {
                    lonely_action = true; // un sous-jeu avec une seule action => mauvais découpage
                    break;
                }
            }
        }
        // si il n'y a qu'une seule condition de victoire il n'y a qu'un sous-jeu
        // si il n'y a un sous-jeu avec une seule action : mauvais découpage sans liens faibles
        // si il n'y a qu'un sous-jeu dépendant des actions sans liens faibles : pas de découpage
        if (!lonely_action && action_dependant.size() > 1 && cond_goalterm_sets[VICTORY].size() > 1) { // si découpage en sous-jeux strong possible et cohérent
            // sous-jeux intervenant dans goal
            for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++)
                if (values_goal[t_id] == BOOL_T && fact_to_subgame_strong[t_id] != 0)
                    in_goal.insert(fact_to_subgame_strong[t_id]);

            // on scinde les conditions de victoire pour ne pas réunir les sous-jeu dépendants ou indépendants des actions
            auto cond_set = partition_victory_sets(VICTORY, action_dependant, fact_to_subgame_strong);

            size_t nb_action_independent = subgames_strong.size() - action_dependant.size();
            // voir si les conditions de victoire partitionnent les sous-jeux en différents groupes ou si elles confirment le découpage
            for (const auto& vset : cond_set) {
                SubgameId s_id = 0; // id non défini
                for (Fact t : vset) {
                    SubgameId old = fact_to_subgame_strong[t.id];
                    if (old == 0) {
                        old = next_subgame_id++;
                        fact_to_subgame_strong[t.id] = old;
                        subgames_strong[old].insert(t.id);
                    }
                    // ce sous-jeu va disparaitre ou bien son utilité est confirmée
                    in_goal.erase(old);
                    // récupère ou modifie l'id du sous-jeu
                    if (s_id == 0) {
                        s_id = old; // premier fluent, on récupérèe l'id du sous-jeu correspondant
                        // ce sous-jeu aporte des points, c'est confirmé
                        usefull_for_goal.insert(s_id);
                    }
                    else if (old != s_id) {
                        // on change son id
                        for (FactId g : subgames_strong[old]) fact_to_subgame_strong[g] = s_id;
                        subgames_strong[s_id].insert(subgames_strong[old].begin(), subgames_strong[old].end());
                        subgames_strong.erase(old);
                    }
                }
            }

            //if (subgames_strong.size() > nb_action_independent + 1) { // plus d'un jeu action dépendant
            if (in_goal.empty() && subgames_strong.size() > nb_action_independent + 1) { // plus d'un jeu action dépendant et chacun peut apporter la victoire
                //cout << "jeu sans les liens faibles pouvant être décomposé d'après les conditions de victoire" << endl;
                subgames = subgames_strong;
                fact_to_subgame = fact_to_subgame_strong;
                decomp = true;
            }
        }
        // pas de découpage ou fusion, on récupère les sous-jeux (normaux) utiles pour goal et term
        if (!decomp) {
            // sous-jeux intervenant dans teminal
            for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++)
                if (values_term[t_id] == BOOL_T && fact_to_subgame[t_id] != 0)
                    usefull_for_term.insert(fact_to_subgame[t_id]);
            // sous-jeux intervenant dans goal
            for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++)
                if (values_goal[t_id] == BOOL_T && fact_to_subgame[t_id] != 0)
                    usefull_for_goal.insert(fact_to_subgame[t_id]);
        }
    }

    // nettoyage des sous-jeux contenant un seul fluent
    for (auto it = subgames.begin(); it != subgames.end();  ) {
        if (it->second.size() < 2) {
            //cout << "subgame " << it->first << " contient un seul fluent" << endl;
            fact_to_subgame[*it->second.begin()] = 0;
            it = subgames.erase(it);
        }
        else ++it;
    }

    // sous-jeux intervenant dans teminal
    for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++)
        if (values_term[t_id] == BOOL_T && fact_to_subgame[t_id] != 0)
            usefull_for_term.insert(fact_to_subgame[t_id]);

    // sous jeux (in)dépendants des actions, sous-jeux inutiles et actions absentes
    hard_linked.clear(); // pour collecter les actions avec des liens forts (et connaitre celles qui ne sont plus liées dans les sous-jeux strong)
    hard_linked.resize(nb_inputs, false);
    action_dependant.clear();
    set<SubgameId> useless;
    set<SubgameId> usefull;
    set<SubgameId> act_ind_usefull;

    // si on a pas trouvé de sous-jeu terminal ou intervenant dans goal :
    // les fluents/actions intervenant dans goal ou terminal n'ont pas encore été explorés, on élimine aucun sous-jeu
    // sinon on note que seuls ceux qui sont utiles pour goal ou terminal sont utiles
    for (const auto& entry : subgames) {
        if (usefull_for_term.empty() || usefull_for_goal.empty() ||
            usefull_for_term.find(entry.first) != usefull_for_term.end() ||
            usefull_for_goal.find(entry.first) != usefull_for_goal.end()) {
            // on récupère le sous-jeu comme étant utile
            usefull.insert(entry.first);
            if (*entry.second.rbegin() < c_infos.id_does.front())
                act_ind_usefull.insert(entry.first);
        } else {
            useless.insert(entry.first);
        }
        // on examine le sous-jeu pour savoir si il dépend des actions et noter les actions utilisées
        auto it = entry.second.rbegin();
        if (*it >= c_infos.id_does.front()) {
            // dependant des actions
            action_dependant.insert(entry.first);
            // on prend note des actions liées fortement (ou ayant des preconditions) dans ce sous-jeu
            for ( ; it != entry.second.rend(); it++) {
                if (*it >= c_infos.id_end) {
                    for (FactId id_d : meta_actions.meta_to_actions_[*it])
                        hard_linked[id_d - c_infos.id_does.front()] = true;
                }
            }
        }
    }
    // les crosspoints qui amènent un jeu utile
    std::set<Fact> usefull_crosspoint;
    for (const auto& c : crosspoint) {
        for (const auto& f : from_crosspoint_to[c]) {
            if (f.id >= c_infos.id_does.front()) break; // on a épuisé les fluents, c'est une action ou un autre crosspoint
            if (usefull.find(fact_to_subgame[f.id]) != usefull.end()) {
                usefull_crosspoint.insert(c);
                break;
            }
        }
    }
    for (SubgameId s_id : useless) {
        // utile car préconditionne un crosspoint et ce crosspoint amène un jeu utile
        for (Fact c : usefull_crosspoint) {
            for (FactId f_id : subgames[s_id]) {
                if (control_fluents[f_id - c_infos.id_true]) continue;  // on exclue les fluents control
                if (f_id >= c_infos.id_does.front()) break; // on a épuisé les fluents, c'est une meta-action
                if (to_crosspoint_from[c].find(Fact(f_id, true)) != to_crosspoint_from[c].end() ||
                    to_crosspoint_from[c].find(Fact(f_id, false)) != to_crosspoint_from[c].end()) {
                    usefull.insert(s_id);
                    if (*subgames[s_id].rbegin() < c_infos.id_does.front())
                        act_ind_usefull.insert(s_id);
                    break;
                }
            }
        }
    }
    size_t nb_usefull_act_dep = usefull.size() - act_ind_usefull.size();
    size_t nb_useless_act_dep = action_dependant.size() - nb_usefull_act_dep;
    size_t nb_act_indep = subgames.size() - action_dependant.size();
    size_t nb_useless_act_indep = nb_act_indep - act_ind_usefull.size();

     *out_dcmp << "=> " << action_dependant.size() << " action dep after checking (useless: " << nb_useless_act_dep << ") - ";
     *out_dcmp << nb_act_indep << " action indep (useless: " << nb_useless_act_indep << ") " << endl;
}

/*******************************************************************************
 *     Splitter : graph for debug - graphe global decomposition
 ******************************************************************************/

/* create a graphViz file repesenting the (sub?)game */
void Splitter6::createGraphvizFile(const string& name, bool strong) const {
    // create a graphviz .gv file into log/ directory
    string gv_file = log_dir + name + ".gv";
    string ps_file = log_dir + name + ".ps";
    
    ofstream fp(gv_file);
    if (!fp) {
        cerr << "Error in createGraphvizFile: unable to open file ";
        cerr << gv_file << endl;
        exit(EXIT_FAILURE);
    }
    fp << graphvizReprJmoves(name, strong); // d'après l'effet des joint moves
    fp.close();
    
    cout << "graphviz .gv file created, can't create the .ps file ";
    cout << "myself, use this to create .ps file :" << endl;
    cout << "dot -Tps " << gv_file << " -o " << ps_file << endl;
}

/* give the graphViz representation of the (sub?)game with detection of effects for each joint move */
string Splitter6::graphvizReprJmoves(const string& name, bool strong) const {
    stringstream nodes;
    stringstream links;

    // fluents
    for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) {
        nodes << "    " << t_id << " [label=\"" <<  *c_infos.vars[t_id] << "\",color=blue,peripheries=1,";
        
        if (action_independent[t_id -  c_infos.id_true]) nodes << "penwidth=3];" << endl;
        else nodes << "penwidth=1];" << endl;
    }

    // actions / meta-actions
    for (const auto& meta_entry : meta_actions.meta_to_actions_) {
        nodes << "    subgraph cluster_" << meta_entry.first << "{" << endl;
        //nodes << "        label = \"meta" << meta_entry.first << "\"; color = magenta;" << endl;
        nodes << "        color = magenta;" << endl;
        for (FactId d_id : meta_entry.second)
            nodes << "        " << meta_entry.first << d_id << " [label=\"" <<  *c_infos.vars[d_id] << "\",color=black,shape=box,peripheries=1,penwidth=1];" << endl;
        nodes << "    }" << endl;

        for (const Fact& f : meta_actions.meta_links_[EFFECTS].at(meta_entry.first)) {
            double mean = 0;
            for (FactId d_id : meta_entry.second) {
                if (f.value)
                    mean += actions_efflinks[POS][d_id - c_infos.id_does.front()][f.id - c_infos.id_true];
                else
                    mean += actions_efflinks[NEG][d_id - c_infos.id_does.front()][f.id - c_infos.id_true];
            }
            mean /= meta_entry.second.size();
            double penwidth = mean * 10;
            if (!strong || mean >= THR_LOW_LINK) { // un lien apporté lors de la recherche des next_precond peut être à 0
                links << "    " << meta_entry.first << *meta_entry.second.begin() << " -> " << f.id << "[dir=none,ltail=cluster_" << meta_entry.first;
                if (f.value) {
                    links  << ",color=\"" << ((mean == 0)? "green":"darkgreen") << "\" constraint=\"true\" penwidth=\"" << penwidth << "\"];" << endl;
                    //links  << ",color=\"darkgreen\" constraint=\"" << ((penwidth > 5)? "true":"false") << "\" penwidth=\"" << penwidth << "\"];" << endl;
                    //links  << ",color=\"darkgreen\" constraint=\"true\" penwidth=\"" << penwidth << "\"];" << endl;
                } else {
                    links  << ",color=\"" << ((mean == 0)? "brown":"red") << "\" constraint=\"true\" penwidth=\"" << penwidth << "\"];" << endl;
                    //links  << ",color=\"red\" constraint=\"" << ((penwidth > 5)? "true":"false") << "\" penwidth=\"" << penwidth << "\"];" << endl;
                    //links  << ",color=\"red\" constraint=\"true\" penwidth=\"" << penwidth << "\"];" << endl;
                }
            }
        }
        for (const Fact& f : meta_actions.meta_links_[LEGAL_PRECONDS].at(meta_entry.first)) {
            links << "    " << meta_entry.first << *meta_entry.second.begin() << " -> " << f.id << "[dir=none,ltail=cluster_" << meta_entry.first;
            if (f.value) {
                links  << ",color=blue constraint=true penwidth=1];" << endl;
            } else {
                links  << ",color=orange constraint=true penwidth=1];" << endl;
            }
        }
        for (const Fact& f : meta_actions.meta_links_[NEXT_PRECONDS].at(meta_entry.first)) {
            links << "    " << meta_entry.first << *meta_entry.second.begin() << " -> " << f.id << "[dir=none,ltail=cluster_" << meta_entry.first;
            if (f.value) {
                links  << ",color=purple constraint=true penwidth=1];" << endl;
            } else {
                links  << ",color=salmon constraint=true penwidth=1];" << endl;
            }
        }
    }
    // actions noop
    for (FactId d_num = 0; d_num < nb_inputs; d_num++) {
        if (noop_actions[d_num])
            nodes << "    " << d_num + c_infos.id_does.front() << " [label=\"" <<  *c_infos.vars[d_num + c_infos.id_does.front()] << "\",color=black,shape=box,peripheries=1,penwidth=3];" << endl;
    }
    // fluents indépendants des actions
    for (auto& entry : fluents_prems) {
        for (FactId f_id : entry.second)
            links << "    " << f_id << " -> " << entry.first << "[color=black constraint=true penwidth=10];" << endl;
    }
    
//    for (auto& set_f : subgoals) {
//        for (auto it1 = set_f.begin(), it2 = ++set_f.begin(); it2 != set_f.end(); it1++, it2++) {
//            links << "    " << *it1 << " -> " << *it2 << "[color=gold constraint=false penwidth=10];" << endl;
//        }
//    }

    // sous-jeux
    auto* sub = &subgames;
    if (strong) sub = &subgames_strong;
    for (const auto& entry : *sub) {
        nodes << "    subgraph cluster_subgame" << entry.first << "{" << endl;
        nodes << "        color = green;" << endl;
        nodes << "        peripheries=10;" << endl;
        for (FactId id : entry.second) {
            if (id < c_infos.id_does.front())
                nodes << "        " << id << endl;
//            else {
//                for (FactId d_id : meta_actions.meta_to_actions_.at(id)) {
//                    nodes << "        " << id << d_id << endl;
//                }
//            }
        }
        nodes << "    }" << endl;
    }

    stringstream text;
    text << "/*--------------- graphe " << name << " ---------------*/" << endl;
    text << "digraph game {" << endl;
    text << "    rankdir=LR;" << endl;
    text << "    overlap=false;" << endl;
    text << "    size=\"4, 10\"; " << endl;
    text << "    ranksep=\"3 equally\";" << endl;
    text << "    compound=true;" << endl;
    text << nodes.str();
    text << links.str();
    text << "}";
    
    return text.str();
}

/*******************************************************************************
 *     Splitter : for debug
 ******************************************************************************/

void Splitter6::printTermOrId(FactId id, std::ostream& out) {
    if (id == 0)
        out << "always_true";
    else if (id == 1)
        out << "always_false";
    else if (id < c_infos.vars.size())
        out << *c_infos.vars[id];
    else {
        auto it = c_infos.sub_expr_terms.find(id);
        if (it != c_infos.sub_expr_terms.end())
            out << *it->second;
        else
            out << id;
    }
    out << " ";
}

void Splitter6::printTermOrId(const Fact& f, std::ostream& out) {
    if (!f.value)
        out << "(not ";
    if (f.id == 0)
        out << "always_true";
    else if (f.id == 1)
        out << "always_false";
    else if (f.id < c_infos.vars.size())
        out << *c_infos.vars[f.id];
    else {
        auto it = c_infos.sub_expr_terms.find(f.id);
        if (it != c_infos.sub_expr_terms.end())
            out << *it->second;
        else
            out << f.id;
    }
    if (!f.value)
        out << ")";
    out << " ";
}

void Splitter6::printActionIndependent(std::ostream& out) {
    out << "-------------------------ACTION INDEPENDENT" << endl;
    for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) {
        if (action_independent[t_id - c_infos.id_true])
            printTermOrId(t_id, out);
    }
    out << endl;
    out << "-------------------------NOT IN NEXT" << endl;
    for (FactId d_id = c_infos.id_does.front(); d_id < c_infos.id_does.back(); d_id++) {
        if (does_in_next[d_id - c_infos.id_does.front()].empty())
            printTermOrId(d_id, out);
    }
    out << endl;
}

void Splitter6::printLegalPrecond(std::ostream& out) {
    out << "-------------------------LEGAL PRECOND" << endl;
    for (const auto& precond_entry : precond_to_legal) {
        printTermOrId(precond_entry.first, out);
        out << " : " << endl;
        for (const auto& role_entry : precond_entry.second) {
            out << "\t\t" << *c_infos.roles[role_entry.first] << " : " << endl << "\t\t";
            for (auto a : role_entry.second) {
                printTermOrId(a, out);
            }
            out << endl;
        }
        out << endl;
    }
    out << endl;
}

void Splitter6::printDoesPrecondInNext(std::ostream& out) {
    out << "-------------------------NEXT PREMISSES" << endl;
    out << "---------SAFE" << endl;
    for (const auto& next_entry : precond_to_does) {
        printTermOrId(next_entry.first, out);
        out << " : " << endl;
        for (const auto& role_entry : next_entry.second) {
            out << "\t\t" << *c_infos.roles[role_entry.first] << " : " << endl;
            for (const auto& precond_entry : role_entry.second) {
                out << "\t\t\t\t";
                printTermOrId(precond_entry.first, out);
                out << " : ";
                for (FactId a : precond_entry.second) {
                    printTermOrId(a, out);
                }
                out << endl;
            }
            out << endl;
        }
    }
    out << "---------UNSAFE" << endl;
    for (const auto& next_entry : unsafe_precond_to_does) {
        printTermOrId(next_entry.first, out);
        out << " : " << endl;
        for (const auto& role_entry : next_entry.second) {
            out << "\t\t" << *c_infos.roles[role_entry.first] << " : " << endl;
            for (const auto& precond_entry : role_entry.second) {
                out << "\t\t\t\t";
                printTermOrId(precond_entry.first, out);
                out << " : ";
                for (FactId a : precond_entry.second) {
                    printTermOrId(a, out);
                }
                out << endl;
            }
            out << endl;
        }
    }
    out << endl;
}

void Splitter6::printInNbJmove(std::ostream& out) {
    out << "--------------NOMBRE MAX DE MOUVEMENTS JOINTS OU APPARAIT CHAQUE ACTION POUR UNE POSITION" << endl;
    for (FactId d_id = c_infos.id_does.front(); d_id < c_infos.id_does.back(); d_id++) {
        FactId d_num = d_id - c_infos.id_does.front();
        printTermOrId(d_id, out);
        out << " - jmove_max=" << in_nb_jmoves[d_num];
        out << " - nb_jmove=" << in_joint_moves[d_num].size();
        
        float mean = 0;
        out << " - ( ";
        for (const auto& m : in_joint_moves[d_num]) {
            mean += m->second.occ_;
            out << m->second.occ_ << " ";
        }
        mean /= in_joint_moves[d_num].size();
        out << ") mean_occ=" << mean;
    
        out << endl;
    }
}

void Splitter6::printJmoveEffects(std::ostream& out) {
    for (auto move : jmoves_effects) {
        out << endl;
        for (FactId a : move.first)
            printTermOrId(a, out);
        out << " occ=" << move.second.occ_ << endl;
        for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) {
            if (move.second.eff_[POS][t_id - c_infos.id_true] > 0 || move.second.eff_[NEG][t_id - c_infos.id_true] > 0) {
                out << "\t\t";
                printTermOrId(t_id, out);
                out << " pos=" << move.second.eff_[POS][t_id - c_infos.id_true];
                out << " neg=" << move.second.eff_[NEG][t_id - c_infos.id_true];
                out << endl;
            }
        }
        
        out << "change together :" << endl;
        for (auto& entry : move.second. change_together_) {
            out << "\t\t";
            printTermOrId(entry.first, out);
            out << " : ";
            for (Fact f : entry.second)
                printTermOrId(f, out);
            out << endl;
        }
    }
}

void Splitter6::printInJmoveEffects(std::ostream& out) {
    for (FactId d_num = 0; d_num < nb_inputs; d_num++) {
        out << endl;
        printTermOrId(d_num + c_infos.id_does.front(), out);
        out << endl << "\t\tocc=";
        for (auto it_move : in_joint_moves[d_num])
            printf("%-3zu ", it_move->second.occ_);
        for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) {
            out << endl << "\t\tpos=";
            for (auto it_move : in_joint_moves[d_num])
                printf("%-3zu ", it_move->second.eff_[POS][t_id - c_infos.id_true]);
            printTermOrId(t_id, out);
            out << endl << "\t\tneg=";
            for (auto it_move : in_joint_moves[d_num])
                printf("%-3zu ", it_move->second.eff_[NEG][t_id - c_infos.id_true]);
            printTermOrId(t_id, out);
            out << endl;
        }
    }
}

void Splitter6::printWhatChangeTogether(std::ostream& out) {
    out << "--------------CHANGE TOGETHER" << endl;
    for (const auto& entry : change_together_global) {
        if (entry.second.empty()) continue;
        out << "[" << change_together_global_occ[entry.first] << "] ";
        out << "[STEP=" << change_together_global_step[entry.first] << "] ";
        printTermOrId(entry.first, out);
        out << " : ";
        for (const auto& f : entry.second) {
            printTermOrId(f, out);
        }
        out << endl;
    }
}

void Splitter6::printActionsLinks(std::ostream& out) {
    out << "-------------------------ACTIONS LINKS" << endl;
    for (FactId d_id = c_infos.id_does.front(); d_id < c_infos.id_does.back(); d_id++) {
        printTermOrId(d_id, out);
        out << endl;
        for (FactId t_id = c_infos.id_true; t_id < c_infos.id_does.front(); t_id++) {
            float pos = actions_efflinks[POS][d_id - c_infos.id_does.front()][t_id - c_infos.id_true];
            float neg = actions_efflinks[NEG][d_id - c_infos.id_does.front()][t_id - c_infos.id_true];

            if (pos != 0 || neg != 0 ) {
                out << "\t\t";
                printTermOrId(t_id, out);
                out << " pos=" << pos << " neg=" << neg   << endl;
            }
        }
        out << "\t effets liés:" << endl;
        for (const auto& entry : change_together_per_action[d_id - c_infos.id_does.front()]) {
            out << "\t\t[" << change_together_per_action_occ[d_id - c_infos.id_does.front()][entry.first] << "] ";
            printTermOrId(entry.first, out);
            out << " : ";
            for (const Fact& f : entry.second)
                printTermOrId(f, out);
            out << endl;
        }
    }
}

void Splitter6::printActionEffectOnFluents(std::ostream& out) {
    out << "-------------------------ACTIONS EFFECTS ON FLUENTS" << endl;
    for (FactId t_num = 0; t_num < nb_bases; t_num++) {
        //if (action_independent[t_num]) continue;
        printTermOrId(t_num + c_infos.id_true, out);
        out << endl;
        for (FactId d_num = 0; d_num < nb_inputs; d_num++) {
            if (actions_efflinks[POS][d_num][t_num] > 0 || actions_efflinks[NEG][d_num][t_num] > 0) {
                out << "\t\tpos=" << actions_efflinks[POS][d_num][t_num] << " \t\tneg=" << actions_efflinks[NEG][d_num][t_num] << "\t";
                printTermOrId(d_num + c_infos.id_does.front(), out);
                 out << endl;
            }
        }
        out << endl;
    }
}

void Splitter6::printMeta2Actions(MetaMap mp, string title) {
    cout << "-------------------------META-ACTIONS" << title << endl;
    for (const auto& meta_entry : mp.meta_to_actions_) {
        printTermOrId(meta_entry.first, cout);
        cout << " : ";
        for (const auto& a : meta_entry.second)
            printTermOrId(a, cout);
        cout << endl;
    }
}

void Splitter6::printMeta2Links(MetaMap mp, std::string title) {
    cout << "-------------------------" << title << endl;
    for (int id = 0; id < TOTAL_LINKS; id++) {
        if (id == LEGAL_PRECONDS) {
                cout << "----------------- LEGAL PRECOND" << endl;
        } else if (id == EFFECTS) {
                cout << "----------------- EFFECTS" << endl;
        } else if (id == NEXT_PRECONDS) {
                cout << "----------------- NEXT PRECOND" << endl;
        }
        
        for (const auto& link_entry : mp.meta_links_[id]) {
            printTermOrId(link_entry.first, cout);
            cout << " : ";
            for (const auto& l : link_entry.second)
                printTermOrId(l, cout);
            cout << endl;
        }
    }
}

void Splitter6::printSubgamesStrong(std::ostream& out) {
    out << "-------------------------SUBGAMES STRONGS" << endl;
    int count = 1;
    for (const auto& entry : subgames_strong) {
        out << "SUBGAME " << count++ << " ------------ (id=" << entry.first << ")" << endl;
        for (FactId id : entry.second) {
            if (id >= c_infos.id_does.front()) { // meta-action
                out << "[ ";
                for (FactId d_id : meta_actions.meta_to_actions_[id])
                    printTermOrId(d_id, out);
                out << "]";
            } else {             // fluent
                printTermOrId(id, out);
            }
        }
        out << endl;
    }
}

void Splitter6::printSubgames(std::ostream& out) {
    out << "-------------------------SUBGAMES" << endl;
    int count = 0;
    for (const auto& entry : subgames) {
        out << "SUBGAME " << ++count << " ------------ (id=" << entry.first << ")" << endl;
        for (FactId id : entry.second) {
            if (id >= c_infos.id_does.front()) { // meta-action
                out << "[ ";
                for (FactId d_id : meta_actions.meta_to_actions_[id])
                    printTermOrId(d_id, out);
                out << "]";
            } else {         // fluent
                printTermOrId(id, out);
            }
        }
        out << endl;
    }
    count = 0;
    out << "-------------------------ACTION WITHOUT KNOWN EFFECT" << endl;
    for (FactId d_id = c_infos.id_does.front(); d_id < c_infos.id_does.back(); d_id++) {
        FactId d_num = d_id - c_infos.id_does.front();
        if (noop_actions[d_num] && in_nb_jmoves[d_num] == 0) {
            printTermOrId(d_id, out);
            ++count;
        }
    }
    out << endl;
    out << "=> " <<  count << " actions never tested" << endl;
    count = 0;
    out << "-------------------------ACTION WITHOUT EFFECT" << endl;
    for (FactId d_id = c_infos.id_does.front(); d_id < c_infos.id_does.back(); d_id++) {
        FactId d_num = d_id - c_infos.id_does.front();
        if (noop_actions[d_num] && in_nb_jmoves[d_num] != 0) {
            printTermOrId(d_id, out);
            ++count;
        }
    }
    out << endl;
    out << "=> " << count << " actions without effect" << endl;
    out << "-------------------------NOT INTO SUBGAMES" << endl;
    count = 0;
    int count2 = 0;
    for (FactId id = c_infos.id_true; id < c_infos.id_does.front(); id++)
        if (fact_to_subgame[id] == 0) {
            printTermOrId(id, out);
            ++count;
        }
    for (FactId d_num = 0; d_num < nb_inputs; d_num++)
        if (hard_linked.size() > d_num && !hard_linked[d_num]) {
            printTermOrId(d_num + c_infos.id_does.front(), out);
            ++count2;
        }
    out << endl;
    out << "=> " << count << " fluents never changed" << endl;
    out << "=> " << count2 << " unconnected actions" << endl;
}

void Splitter6::printCrosspoints(std::ostream& out) {
    out << "-------------------------CROSSPOINTS" << endl;
    for (const auto& f : crosspoint) {
        printTermOrId(f, out);
        if (f.id >= c_infos.id_end) {
            out << ": ";
            for (const Fact& g : fluent_2_conj_crosspoint[f]) {
                printTermOrId(g, out);
            }
        }
        out << endl;
    }
    out << endl;
}

void Splitter6::printCondVictory(std::ostream& out) {
    for (auto vic : {PART_VICTORY, VICTORY}) {
        out << "-------------------------" << ((vic == VICTORY)? "VICTORY": "PART VICTORY") << endl;
        for (const auto& sub : cond_goalterm_sets[vic]) {
            for (Fact f : sub)
                printTermOrId(f, out);
            out << endl;
        }
        out << endl;
    }
}

void Splitter6::printCondTerminal(std::ostream& out) {
    out << "-------------------------TERMINAL" << endl;
    for (const auto& cond : cond_goalterm_sets[TERMINAL]) {
        for (Fact f : cond)
            printTermOrId(f, out);
        out << endl;
    }
    out << endl;
}
