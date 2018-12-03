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
    Link2(TermPtr m, Node2Ptr n) : next_node(n), visits(0), sum_score(0), fully_explored(false) { moves.insert(m); };
private:
    SetTermPtr moves;
    size_t visits;
    size_t sum_score;
    Node2Ptr next_node;
    bool fully_explored;

    friend std::ostream& operator<<(std::ostream& out, const Node2& t);
    friend class Node2;
    friend class UctSinglePlayer2;
};

class Node2 {
    Node2(const VectorTermPtr& p, bool term) : pos(p), visits(0), nb_fully_explored(0), terminal(term) {};
private:
    SetTermPtr getUnexplored(const VectorTermPtr& legals);
    Link2Ptr getChild(TermPtr move);
    Node2Ptr noteMoveResult(TermPtr move, VectorTermPtr&& pos, bool term);

    const VectorTermPtr pos;
    size_t visits;
    std::vector<Link2> childs;
    size_t nb_childs; // pour vérifier si on le note completement exploré quand il faut
    size_t nb_fully_explored;
    bool terminal;
    
    friend std::ostream& operator<<(std::ostream& out, const Node2& t);
    friend class UctSinglePlayer2;
};

class UctSinglePlayer2 {
public:
    UctSinglePlayer2(Circuit& circ, float c = 0.4f);
    ~UctSinglePlayer2(){ for (Node2Ptr n : all_nodes) delete n; };

    std::pair<bool, int> run(int max = 10000000);
    void printCurrent();
    
private:
    void selection_expansion();
    bool selection(Node2Ptr currentNode2);
    bool expansion(Node2Ptr cnode, SetTermPtr& unexplored);
    void simul_backprop();

    void createGraphvizFile(const std::string& name) const;
    std::string graphvizRepr(const std::string& name) const;

    Circuit& circuit;
    TermPtr role;
    float uct_const;
    std::default_random_engine rand_gen;

    Node2Ptr root;
    VectorBool current;
    std::vector<Node2Ptr> descent_node;
    std::vector<Link2Ptr> descent_move;
    std::vector<Node2Ptr> all_nodes;
    bool solution_found;
};

#endif /* UctSinglePlayer2_hpp */
