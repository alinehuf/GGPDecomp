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
#include "UctSinglePlayerNodeLink.hpp"
#include <unordered_map>

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
    std::vector<NodePtr> descent_nptr;
    std::vector<int> descent_mid;
    std::unordered_map<size_t, NodePtr> transpo;
    bool solution_found;
    int best_game_score = 0;
};

#endif /* UctSinglePlayer_hpp */
