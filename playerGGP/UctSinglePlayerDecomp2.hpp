//
//  UctSinglePlayerDecomp2.hpp
//  playerGGP
//
//  Created by AlineHuf on 05/03/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#ifndef UctSinglePlayerDecomp2_hpp
#define UctSinglePlayerDecomp2_hpp

#include "Circuit.h"
#include <unordered_map>

class SubLink;
class SubNode;
typedef SubLink * SubLinkPtr;
typedef SubNode * SubNodePtr;

class SubLink {
public:
    SubLink(TermPtr m, SubNodePtr n) : next_node(n), visits(0), sum_score(0), sum_min(0), sum_max(0), min(-1), max(-1) { moves_explored.emplace(m,false); };
private:
    std::unordered_map<TermPtr, bool> moves_explored;
    size_t visits;
    size_t sum_score;
    size_t sum_min;
    size_t sum_max;
    SubNodePtr next_node;
    Score min;
    Score max;

    bool fullyExplored(SetTermPtr legal); // toutes les actions légales sont totalement explorées
    int nbFullyExplored();

    friend std::ostream& operator<<(std::ostream& out, const SubLink& t);
    friend class SubNode;
    friend class UctSinglePlayerDecomp2;
};

class SubNode {
    SubNode(SetTermPtr&& p, size_t d, size_t h, bool term = false) :
    pos(std::forward<SetTermPtr>(p)), depth(d), hash(h), visits(0), terminal(term) {};
    ~SubNode() { for (SubLinkPtr c : childs) delete c; }

    static size_t computeHash(SetTermPtr p, size_t depth);
    SubLinkPtr getChild(TermPtr move) const;
    SetTermPtr getUnexplored(const VectorTermPtr& legals);
    SubNodePtr noteMoveResult(TermPtr move, SetTermPtr&& pos, size_t depth, bool term, std::unordered_map<size_t, SubNodePtr>& transpo);

    bool fullyExplored(SetTermPtr legal); // toutes les actions légales sont totalement explorées
    int nbFullyExplored();

    const size_t hash;
    const SetTermPtr pos;
    const size_t depth;
    size_t visits;
    std::vector<SubLinkPtr> childs;
    std::unordered_set<TermPtr> childs_moves;
    bool terminal;

    friend std::ostream& operator<<(std::ostream& out, const SubNode& t);
    friend class UctSinglePlayerDecomp2;
};

class UctSinglePlayerDecomp2 {
public:
    UctSinglePlayerDecomp2(Circuit& circ, std::vector<std::unordered_set<TermPtr>>&& sgt, float c = 0.4f);
    ~UctSinglePlayerDecomp2(){
        for (auto& t : transpo) { for (auto& e : t) delete e.second; } };

    std::pair<bool, int> run(int max = 10000000);
    void printCurrent();

private:
    std::vector<std::vector<float>> selection_expansion();
    std::vector<std::vector<float>> selection(std::vector<SubNodePtr> cnode);
    bool expansion(std::vector<SubNodePtr>& cnode, SetTermPtr& unexplored);
    void simul_backprop();
    void backprop(std::vector<std::vector<float>> scores);

    SetTermPtr getSubPos(const VectorTermPtr& global_pos, int i);
//    std::vector<SubLinkPtr> getBestMoves(SubNodePtr n);
//    void intersect(SetTermPtr& s1, const SetTermPtr& s2);

    void printSubTree(std::ostream& out, SubNodePtr n, int max_level = 10);
    void printSubTree(std::ostream& out, SubNodePtr n, int level, int max_level, std::set<SubNodePtr>& done);
    void createGraphvizFile(const std::string& name, int s) const;
    std::string graphvizRepr(const std::string& name, int s) const;

    // constantes
    Circuit& circuit;
    TermPtr role;
    const std::vector<std::unordered_set<TermPtr>> subgames_trues;
    float uct_const;
    std::default_random_engine rand_gen;

    // variables
    std::vector<std::vector<bool>> subgame_mask;

    std::vector<SubNodePtr> root;
    VectorBool init_vals;
    VectorBool current;
    std::vector<std::vector<SubNodePtr>> descent_node;
    std::vector<std::vector<SubLinkPtr>> descent_link;
    std::vector<TermPtr> descent_move;

    std::vector<std::unordered_map<size_t, SubNodePtr>> transpo;
    std::vector<bool> subgame_terminal;
    std::vector<bool> subgame_fully_explored;
    size_t nb_fully_explored;
    std::vector<bool> subgame_max_score;
    //std::vector<bool> subgame_solved;
    //size_t nb_solved;
    Score found;
    bool solution_found;
};

#endif /* UctSinglePlayerDecomp2_hpp */
