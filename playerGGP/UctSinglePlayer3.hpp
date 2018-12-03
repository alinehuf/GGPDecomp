//
//  UctSinglePlayer3.hpp
//  playerGGP
//
//  Created by AlineHuf on 25/02/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#ifndef UctSinglePlayer3_hpp
#define UctSinglePlayer3_hpp

#include "Circuit.h"
#include <unordered_map>

class Link3;
class Node3;
typedef Link3 * Link3Ptr;
typedef Node3 * Node3Ptr;

class Link3 {
public:
    Link3(TermPtr m, Node3Ptr n) : next_node(n), visits(0), sum_score(0), fully_explored(false) { moves.insert(m); };
private:
    SetTermPtr moves;
    size_t visits;
    size_t sum_score;
    Node3Ptr next_node;
    bool fully_explored;

    friend std::ostream& operator<<(std::ostream& out, const Link3& t);
    friend class Node3;
    friend class UctSinglePlayer3;
};

class Node3 {
    Node3(VectorTermPtr&& p, size_t d, size_t h, bool term) : nb_childs(0), pos(std::forward<VectorTermPtr>(p)), depth(d), hash(h), visits(0), nb_fully_explored(0), terminal(term) {};
    ~Node3() { for (Link3Ptr c : childs) delete c; }
private:
    static size_t computeHash(VectorTermPtr p, size_t depth);
    SetTermPtr getUnexplored(const VectorTermPtr& legals);
    Link3Ptr getChild(TermPtr move);
    Node3Ptr noteMoveResult(TermPtr move, VectorTermPtr&& pos, size_t depth, bool term, std::unordered_map<size_t, Node3Ptr>& transpo);
    void backpropagateNotFullyExplored(int level);
    
    const size_t hash;
    const VectorTermPtr pos;
    const size_t depth;
    std::set<std::pair<Node3Ptr, Link3Ptr>> parent_nodes;
    std::unordered_set<TermPtr> childs_moves;
    size_t visits;
    std::vector<Link3Ptr> childs;
    size_t nb_childs; // pour vérifier si on le note completement exploré quand il faut
    size_t nb_fully_explored;
    bool terminal;

    friend std::ostream& operator<<(std::ostream& out, const Node3& t);
    friend class UctSinglePlayer3;
};

class UctSinglePlayer3 {
public:
    UctSinglePlayer3(Circuit& circ, float c = 0.4f);
    ~UctSinglePlayer3(){ for (auto& t : transpo) delete t.second; };

    std::pair<bool, int> run(int max = 10000000);
    void printCurrent();

private:
    void selection_expansion();
    bool selection(Node3Ptr currentNode3);
    bool expansion(Node3Ptr cnode, SetTermPtr& unexplored);
    void simul_backprop();

    void createGraphvizFile(const std::string& name) const;
    std::string graphvizRepr(const std::string& name) const;

    Circuit& circuit;
    TermPtr role;
    float uct_const;
    std::default_random_engine rand_gen;

    Node3Ptr root;
    VectorBool current;
    std::vector<Node3Ptr> descent_node;
    std::vector<Link3Ptr> descent_move;
    std::unordered_map<size_t, Node3Ptr> transpo;
    bool solution_found;
};

#endif /* UctSinglePlayer3_hpp */
