//
//  UctSinglePlayer2.hpp
//  playerGGP
//
//  Created by AlineHuf on 25/02/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#ifndef UctSinglePlayer2_hpp
#define UctSinglePlayer2_hpp

#include "Circuit.h"
#include <unordered_map>

class Link2;
class Node2;
typedef Link2 * Link2Ptr;
typedef Node2 * Node2Ptr;

class Link2 {
public:
    Link2() : move(nullptr), visits(0), sum_score(0), fully_explored(false) {};
private:
    TermPtr move;
    size_t visits;
    float sum_score;
    bool fully_explored;

    friend std::ostream& operator<<(std::ostream& out, const Node2& t);
    friend class Node2;
    friend class UctSinglePlayer2;
    friend class UctSinglePlayer2BW;
};

class Node2 {
    Node2(const VectorTermPtr& pos, size_t hash, const VectorTermPtr& moves);
    static size_t computeHash(VectorTermPtr p);
private:
    const VectorTermPtr pos;
    size_t hash;
    size_t visits;
    size_t nb_fully_explored;
    std::vector<Link2> childs;

    friend std::ostream& operator<<(std::ostream& out, const Node2& t);
    friend class UctSinglePlayer2;
    friend class UctSinglePlayer2BW;
};


class UctSinglePlayer2 {
public:
    UctSinglePlayer2(Circuit& circ, float c = 0.4f);
    ~UctSinglePlayer2(){ for (auto& e : transpo) delete e.second; };

    std::pair<bool, int> run(int max = 10000000);
    void printCurrent();
    
private:
    float selection_expansion();
    float selection(Node2Ptr currentNode2);
    Node2Ptr expansion();
    Score simulation();
    void backpropagate(Score score);

    void createGraphvizFile(const std::string& name) const;
    std::string graphvizRepr(const std::string& name) const;

    Circuit& circuit;
    TermPtr role;
    float uct_const;
    std::default_random_engine rand_gen;
    int itermax;

    Node2Ptr root;
    VectorBool current;
    std::vector<Node2Ptr> descent_nptr;
    std::vector<int> descent_mid;
    std::unordered_map<size_t, Node2Ptr> transpo;
    bool solution_found;
    int best_game_score = 0;
};

#endif /* UctSinglePlayer2_hpp */
