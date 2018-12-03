//
//  UctSinglePlayerDecompAndNormal.hpp
//  playerGGP
//
//  Created by AlineHuf on 18/04/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#ifndef UctSinglePlayerDecompAndNormal_hpp
#define UctSinglePlayerDecompAndNormal_hpp

#include "Circuit.h"
#include <unordered_map>

class Link4;
class Node4;
typedef Link4 * Link4Ptr;
typedef Node4 * Node4Ptr;

class Link4 {
public:
    Link4(TermPtr m, Node4Ptr n) : next_node(n), visits(0), sum_score(0), fully_explored(false) { moves.insert(m); };
private:
    SetTermPtr moves;
    size_t visits;
    size_t sum_score;
    Node4Ptr next_node;
    bool fully_explored;

    friend std::ostream& operator<<(std::ostream& out, const Link4& t);
    friend class Node4;
    friend class UctSinglePlayerDecompAndNormal;
};

class Node4 {
    Node4(VectorTermPtr&& p, size_t d, size_t h, bool term = false) :
    nb_childs(0), pos(std::forward<VectorTermPtr>(p)), depth(d), hash(h), visits(0), nb_fully_explored(0), terminal(term) {};
    ~Node4() { for (Link4Ptr c : childs) delete c; }

    static size_t computeHash(VectorTermPtr p, size_t depth);
    Link4Ptr getChild(TermPtr move) const;
    SetTermPtr getUnexplored(const VectorTermPtr& legals);
    Node4Ptr noteMoveResult(TermPtr move, VectorTermPtr&& pos, size_t depth, bool term, std::unordered_map<size_t, Node4Ptr>& transpo);
    void backpropagateNotFullyExplored();

    const size_t hash;
    const VectorTermPtr pos;
    const size_t depth;
    std::set<std::pair<Node4Ptr, Link4Ptr>> parent_nodes;
    std::unordered_set<TermPtr> childs_moves;
    size_t visits;
    std::vector<Link4Ptr> childs;
    size_t nb_fully_explored;
    bool terminal;

    size_t nb_childs; // pour vérifier si on le note completement exploré quand il faut
    std::set<Node4Ptr> global_nodes; // noeuds correspondants dans l'arbre global

    friend std::ostream& operator<<(std::ostream& out, const Node4& t);
    friend class UctSinglePlayerDecompAndNormal;
};

class UctSinglePlayerDecompAndNormal {
public:
    UctSinglePlayerDecompAndNormal(Circuit& circ, std::vector<std::unordered_set<TermPtr>>&& sgt, float c = 0.4f);
    ~UctSinglePlayerDecompAndNormal(){
        for (auto& t : transpo) { for (auto& e : t) delete e.second; } };

    std::pair<bool, int> run(int max = 10000000);
    void printCurrent();

private:
    void selection_expansion();
    bool selection(std::vector<Node4Ptr> cnode, Node4Ptr node, int i);
    bool expansion(std::vector<Node4Ptr>& cnode, Node4Ptr& node, SetTermPtr& unexplored);
    void simul_backprop();

    VectorTermPtr getSubPos(const VectorTermPtr& global_pos, int i);
    std::vector<Link4Ptr> getBestMoves(Node4Ptr n);
    void intersect(SetTermPtr& s1, const SetTermPtr& s2);

    void printSubTree(std::ostream& out, Node4Ptr n, int max_level = 10);
    void printSubTree(std::ostream& out, Node4Ptr n, int level, int max_level, std::set<Node4Ptr>& done);
    void createGraphvizFile(const std::string& name, int s) const;
    std::string graphvizRepr(const std::string& name, int s) const;

    void createGraphvizFileFromPos(const std::string& name, Node4Ptr pos) const;
    std::string graphvizReprFromPos(const std::string& name, Node4Ptr pos) const;

    // constantes
    Circuit& circuit;
    TermPtr role;
    const std::vector<std::unordered_set<TermPtr>> subgames_trues;
    float uct_const;
    std::default_random_engine rand_gen;

    // variables
    std::vector<std::vector<bool>> subgame_mask;

    std::vector<Node4Ptr> root;
    Node4Ptr global_root;
    VectorBool init_vals;
    VectorBool current;
    std::vector<std::vector<Node4Ptr>> descent_node;
    std::vector<std::vector<Link4Ptr>> descent_move;
    std::vector<Node4Ptr> global_descent_node;
    std::vector<Link4Ptr> global_descent_move;


    std::vector<std::unordered_map<size_t, Node4Ptr>> transpo;
    std::unordered_map<size_t, Node4Ptr> global_transpo;
    std::vector<bool> subgame_terminal;
    std::vector<bool> subgame_fully_explored;
    size_t nb_fully_explored;
    std::vector<bool> subgame_max_score;
    bool solution_found;
};

#endif /* UctSinglePlayerDecompAndNormal_hpp */
