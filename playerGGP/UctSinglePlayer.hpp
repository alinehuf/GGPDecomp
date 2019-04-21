//
//  UctSinglePlayer.hpp
//  playerGGP
//
//  Created by AlineHuf on 25/02/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#ifndef UctSinglePlayer_hpp
#define UctSinglePlayer_hpp

#include "Circuit.h"
#include <unordered_map>

class Link;
class Node;
typedef Link * LinkPtr;
typedef Node * NodePtr;

class Link {
public:
    Link() : move(nullptr), visits(0), sum_score(0), fully_explored(false) {};
private:
    TermPtr move;
    size_t visits;
    size_t sum_score;
    bool fully_explored;

    friend std::ostream& operator<<(std::ostream& out, const Node& t);
    friend class Node;
    friend class UctSinglePlayer;
    friend class UctSinglePlayerBW;
};

class Node {
    Node(const VectorTermPtr& pos, size_t hash, const VectorTermPtr& moves);
    static size_t computeHash(VectorTermPtr p);
private:
    const VectorTermPtr pos;
    size_t hash;
    size_t visits;
    size_t nb_fully_explored;
    std::vector<Link> childs;

    friend std::ostream& operator<<(std::ostream& out, const Node& t);
    friend class UctSinglePlayer;
    friend class UctSinglePlayerBW;
};


class UctSinglePlayer {
public:
    UctSinglePlayer(Circuit& circ, float c = 0.4f);
    ~UctSinglePlayer(){ for (auto& e : transpo) delete e.second; };

    std::pair<bool, int> run(int max = 10000000);
    void printCurrent();
    
private:
    void selection();
    bool selection(NodePtr currentNode);
    void expansion();
    Score simulation();
    void backpropagate(Score score);

    void createGraphvizFile(const std::string& name) const;
    std::string graphvizRepr(const std::string& name) const;

    Circuit& circuit;
    TermPtr role;
    float uct_const;
    std::default_random_engine rand_gen;
    int itermax;

    NodePtr root;
    VectorBool current;
    VectorBool current_debug;
    std::vector<NodePtr> descent_nptr;
    std::vector<int> descent_mid;
    std::unordered_map<size_t, NodePtr> transpo;
    bool solution_found;
    int best_game_score = 0;
};

#endif /* UctSinglePlayer_hpp */
