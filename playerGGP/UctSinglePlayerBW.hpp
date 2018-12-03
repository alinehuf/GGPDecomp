//
//  UctSinglePlayerBW.hpp
//  playerGGP
//
//  Created by AlineHuf on 05/03/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#ifndef UctSinglePlayerBW_hpp
#define UctSinglePlayerBW_hpp

#include "Circuit.h"
#include "UctSinglePlayerNodeLink.hpp"
#include <unordered_map>

class UctSinglePlayerBW {
public:
    UctSinglePlayerBW(Circuit& circ, std::unordered_set<TermPtr>& bl, std::unordered_set<TermPtr>& wl, float c = 0.4f);
    ~UctSinglePlayerBW(){ for (auto& e : transpo) delete e.second; };

    std::pair<bool, int> run(int max = 10000000);
    void printCurrent();

private:
    VectorTermPtr getFilteredLegals(const VectorBool& values, TermPtr role);
    void selection();
    bool selection(NodePtr currentNode);
    void expansion();
    Score simulation();
    void backpropagate(Score score);

    Circuit& circuit;
    TermPtr role;
    float uct_const;
    std::default_random_engine rand_gen;
    int itermax;
    std::unordered_set<TermPtr>& black_list;
    std::unordered_set<TermPtr>& white_list;

    NodePtr root;
    VectorBool current;
    std::vector<NodePtr> descent_nptr;
    std::vector<int> descent_mid;
    std::unordered_map<size_t, NodePtr> transpo;
    bool solution_found;
};

#endif /* UctSinglePlayerBW_hpp */
