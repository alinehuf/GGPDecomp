//
//  UctSinglePlayerNodeLink.h
//  playerGGP
//
//  Created by AlineHuf on 05/03/2018.
//  Copyright © 2018 dex. All rights reserved.
//

#ifndef UctSinglePlayerNodeLink_h
#define UctSinglePlayerNodeLink_h

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

#endif /* UctSinglePlayerNodeLink_h */
