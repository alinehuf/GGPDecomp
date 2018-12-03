//
//  UctPlayer.cpp
//  playerGGP
//
//  Created by Dexter on 12/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#include "UctPlayer.h"

using std::vector;
using std::pair;
using std::cout;
using std::endl;

/*******************************************************************************
 *      UctPlayer::UctNode : a node of the game tree
 ******************************************************************************/

struct UctPlayer::UctNode {
    UctNode(PositionPtr position, size_t nb_roles) : pos(position),
                                                  nb_tested_move(nb_roles),
                                                  nb_visit_per_move(nb_roles),
                                                  score_sum_per_move(nb_roles) {
        int jm = 1;
        for(auto i = 0; i < nb_roles; ++i) {
            auto n = pos->isTerminal() ? 0 : pos->getNbLegal(i);
            jm *= n;
            nb_visit_per_move[i].resize(n, 0);
            score_sum_per_move[i].resize(n, 0);
        }
        childs.resize(jm);
    }
    bool checkFullyExplored() {
        for (int c = 0; c < childs.size(); ++c)
            if (childs[c] == nullptr || !childs[c]->fully_explored)
                return (fully_explored = false);
        cout << "fully explored " << *pos << endl;
        return (fully_explored = true);
    }
    PositionPtr pos = nullptr;
    int nb_visits = 0;                 // total number of visits for this node
    std::vector<int> nb_tested_move; // number of tested moves for each player
    std::vector< std::vector<int> > nb_visit_per_move;  // number of visits for each move of each player
    std::vector< std::vector<int> > score_sum_per_move; // sum of scores for each move of each player
    std::vector<UctNodePtr> childs;    // list of child nodes (corresponding to all combinations of moves)
    bool fully_explored = false;
};

/*******************************************************************************
 *      UctPlayer
 ******************************************************************************/

UctPlayer::UctPlayer(GamePtr g, RoleId pid, float uct_const) : Player(g, pid), uct_const(uct_const),
                                                               rand_gen((std::random_device()).operator()()) {}

MovePtr UctPlayer::getNextMove(UctNodePtr node) {
    if(node->nb_visit_per_move[myid].size() == 0)
        return nullptr;
    
    int choice;
    if(node->nb_tested_move[myid] == 0) {
        choice = rand_gen() % node->nb_visit_per_move[myid].size();
    } else {
        float best = -1.0f;
        vector<int> best_id;
        for(int i = 0; i < node->nb_visit_per_move[myid].size(); ++i) {
            if(node->nb_visit_per_move[myid][i]) {
                float score = float(node->score_sum_per_move[myid][i]) / node->nb_visit_per_move[myid][i];
                if(score > best) {
                    best = score;
                    best_id.clear();
                }
                if(score == best)
                    best_id.push_back(i);
            }
        }
        choice = best_id[rand_gen() % best_id.size()];
    }
    MovePtr bst = node->pos->getMove(myid, choice);
    cout << "score max (visits): ";
    cout << float(node->score_sum_per_move[myid][choice]) / node->nb_visit_per_move[myid][choice];
    cout << " (" << node->nb_visit_per_move[myid][choice] << ") " << *bst << endl;
    return bst;
}

MovePtr UctPlayer::exploreOnce(PositionPtr pos) {
    if (root == nullptr || *pos != *root->pos) {
        auto it = cache.find(pos);
        if(it == cache.end())
            it = cache.emplace(pos, UctNodePtr(new UctNode(pos, game->getNbRoles()))).first;
        root = it->second;
    }
    uct(root);
    if (root->fully_explored)
        setFullyExplored();
    return getNextMove(root);
}

MovePtr UctPlayer::exploreNTimes(PositionPtr pos, int descents) {
    if(root == nullptr || *pos != *root->pos) {
        auto it = cache.find(pos);
        if(it == cache.end())
            it = cache.emplace(pos, UctNodePtr(new UctNode(pos, game->getNbRoles()))).first;
        root = it->second;
    }
    for(int i = 0; i < descents; ++i)
        uct(root);
    if (root->fully_explored)
        setFullyExplored();
    return getNextMove(root);
}

void UctPlayer::uct(UctNodePtr node) {
    vector<int> mv_path;
    vector<UctNodePtr> node_path(1, node);
    // selection
    do {
        mv_path.push_back(selection(node_path.back()));
        assert(!node_path.back()->fully_explored);
        if(mv_path.back() < 0) // terminal
            break;
        node_path.push_back(node_path.back()->childs[mv_path.back()]);;
    } while(node_path.back()->nb_visits);
    // playout
    vector<Score> scores;
    if(mv_path.back() < 0) {  // terminal
        scores = node_path.back()->pos->getGoals();
        node_path.back()->fully_explored = true;
        mv_path.pop_back();
    } else {
        game->playout(node_path.back()->pos, scores);
    }
    node_path.back()->nb_visits++;
    // update
    for (auto it_node = node_path.rbegin() + 1; it_node != node_path.rend(); ++it_node) {
        int choice = mv_path.back();
        for(int p = (int) game->getNbRoles() - 1; p >= 0; --p) {
            int move = choice % (*it_node)->nb_visit_per_move[p].size();
            if((*it_node)->nb_visit_per_move[p][move] == 0)
                (*it_node)->nb_tested_move[p]++;
            (*it_node)->score_sum_per_move[p][move] += scores[p];
            (*it_node)->nb_visit_per_move[p][move]++;
            choice /= (*it_node)->nb_visit_per_move[p].size();
        }
        (*it_node)->nb_visits++;
        (*it_node)->checkFullyExplored();
        mv_path.pop_back();
    }
}

int UctPlayer::selection(UctNodePtr node) {
    int joint_move = 0;
    vector<UctNodePtr> st(1, node);
    for(auto p = 0; p < game->getNbRoles(); ++p) {
        // no legal move => abort
        if(node->nb_visit_per_move[p].size() == 0)
            return -1;
        int choice = -1;
        // presence of untested move for this player : choose one randomly
        if(node->nb_tested_move[p] < node->nb_visit_per_move[p].size()) {
            vector<int> unvisited;
            for(int i = 0; i < node->nb_visit_per_move[p].size(); ++i)
                if(node->nb_visit_per_move[p][i] == 0)
                    unvisited.push_back(i);
            choice = unvisited[rand_gen() % unvisited.size()];
        }
        // else choose randomly among the best moves
        else {
            float best = -1.0f;
            vector<int> best_id;
            for(int i = 0; i < node->nb_visit_per_move[p].size(); ++i) {
                float score = float(node->score_sum_per_move[p][i]) / node->nb_visit_per_move[p][i]
                + 100 * uct_const * sqrt( log(node->nb_visits) / node->nb_visit_per_move[p][i]);
                if(score > best) {
                    best = score;
                    best_id.clear();
                }
                if(score == best)
                    best_id.push_back(i);
            }
            choice = best_id[rand_gen() % best_id.size()];
        }
        joint_move = joint_move * (int) node->nb_visit_per_move[p].size() + choice;
    }
    // best move already fully explored choose another one
    if (node->childs[joint_move] && node->childs[joint_move]->fully_explored) {
        if (node->nb_tested_move[0] == 0) {
            for(auto p = 0; p < game->getNbRoles(); ++p)
                 node->nb_tested_move[p]++;
        }
        vector<int> choices;
        for(int i = 0; i < node->childs.size(); ++i) {
            if (!node->childs[i] || !node->childs[i]->fully_explored)
                choices.push_back(i);
        }
        if (choices.size() != 0)
            joint_move = choices[rand_gen() % choices.size()];
    }
    // next position does not already exist
    if(!node->childs[joint_move]) {
        int choice = joint_move;
        PositionPtr pos = node->pos;
        for(int p = (int) game->getNbRoles() - 1; p >= 0; --p) {
            MovePtr move = pos->getMove(p, choice % node->nb_visit_per_move[p].size());
            pos = game->next(pos, move);
            choice /= node->nb_visit_per_move[p].size();
        }
        auto it = cache.find(pos);
        if(it == cache.end())
            it = cache.emplace(pos, UctNodePtr(new UctNode(pos, game->getNbRoles()))).first;
        node->childs[joint_move] = it->second;
    }
    return joint_move;
}

//void UctPlayer::uct(UctNodePtr node) {
//    vector<int> mv_path;
//    // selection
//    UctNodePtr current_node = node;
//    do {
//        mv_path.push_back(selection(current_node));
//        if(mv_path.back() < 0) // terminal
//            break;
//        current_node = current_node->childs[mv_path.back()];
//    } while(current_node->nb_visits);
//    // playout
//    vector<Score> scores;
//    if(mv_path.back() < 0) {  // terminal
//        scores = current_node->pos->getGoals();
//        current_node->fully_explored = true;
//        mv_path.pop_back();
//    } else {
//        game->playout(current_node->pos, scores);
//        current_node->checkFullyExplored();
//    }
//    current_node->nb_visits++;
//    // update
//    for(int mv : mv_path) {
//        int choice = mv;
//        for(int p = (int) game->getNbRoles() - 1; p >= 0; --p) {
//            int move = choice % node->nb_visit_per_move[p].size();
//            if(node->nb_visit_per_move[p][move] == 0)
//                node->nb_tested_move[p]++;
//            node->score_sum_per_move[p][move] += scores[p];
//            node->nb_visit_per_move[p][move]++;
//            choice /= node->nb_visit_per_move[p].size();
//        }
//        node->nb_visits++;
//        assert(node->nb_visits > 0);
//        node = node->childs[mv];  // update next node
//    }
//}

//int UctPlayer::selection(UctNodePtr node) {
//    int joint_move = 0;
//    vector<UctNodePtr> st(1, node);
//    for(auto p = 0; p < game->getNbRoles(); ++p) {
//        // no legal move => abort
//        if(node->nb_visit_per_move[p].size() == 0)
//            return -1;
//        int choice = -1;
//        // presence of untested move for this player : choose one randomly
//        if(node->nb_tested_move[p] < node->nb_visit_per_move[p].size()) {
//            vector<int> unvisited;
//            for(int i = 0; i < node->nb_visit_per_move[p].size(); ++i)
//                if(node->nb_visit_per_move[p][i] == 0)
//                    unvisited.push_back(i);
//            choice = unvisited[rand_gen() % unvisited.size()];
//        }
//        // else choose randomly among the best moves
//        else {
//            float best = -1.0f;
//            vector<int> best_id;
//            for(int i = 0; i < node->nb_visit_per_move[p].size(); ++i) {
//                float score = float(node->score_sum_per_move[p][i]) / node->nb_visit_per_move[p][i]
//                            + 100 * uct_const * sqrt( log(node->nb_visits) / node->nb_visit_per_move[p][i]);
//                if(score > best) {
//                    best = score;
//                    best_id.clear();
//                }
//                if(score == best)
//                    best_id.push_back(i);
//            }
//            choice = best_id[rand_gen() % best_id.size()];
//        }
//        joint_move = joint_move * (int) node->nb_visit_per_move[p].size() + choice;
//    }
//    // next position does not already exist
//    if(!node->childs[joint_move]) {
//        int choice = joint_move;
//        PositionPtr pos = node->pos;
//        for(int p = (int) game->getNbRoles() - 1; p >= 0; --p) {
//            MovePtr move = pos->getMove(p, choice % node->nb_visit_per_move[p].size());
//            pos = game->next(pos, move);
//            choice /= node->nb_visit_per_move[p].size();
//        }
//        auto it = cache.find(pos);
//        if(it == cache.end())
//            it = cache.emplace(pos, UctNodePtr(new UctNode(pos, game->getNbRoles()))).first;
//        node->childs[joint_move] = it->second;
//    }
//    return joint_move;
//}


