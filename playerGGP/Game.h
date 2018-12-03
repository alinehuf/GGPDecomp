//
//  Game.h
//  playerGGP
//
//  Created by Dexter on 07/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#ifndef __playerGGP__Game__
#define __playerGGP__Game__

#include "Reasoner.h"

class Game;
typedef std::shared_ptr<Game> GamePtr;

class Move;
typedef std::shared_ptr<const Move> MovePtr;
typedef int MoveId;

class Position;
typedef std::shared_ptr<const Position> PositionPtr;

std::ostream& operator<<(std::ostream& flux, const Game& t);
std::ostream& operator<<(std::ostream& flux, const Move& t);
std::ostream& operator<<(std::ostream& flux, const Position& t);



class Move {
    friend class Game;
    friend class Position;
  public:
    Move(TermPtr move) : mv(move) {};
    ~Move() {};
    //string Move::getAction() const;
    //bool operator<(const Move& y) const;
    //bool operator==(const Move& y) const;
  private:
    TermPtr mv;
    
  friend std::ostream& operator<<(std::ostream& out, const Move& mv);
};

class Position {
    friend class Game;
  public:
    ~Position() {};
    inline const std::set<RoleId>& getHavePlayed() const { return played; }
    inline VectorTermPtr getFacts() const { return pos->facts; }
    int getNbLegal(RoleId pl) const;
    bool isTerminal() const;
    Score getGoal(RoleId pl) const;
    const std::vector<Score>& getGoals() const;
    MovePtr getMove(RoleId, MoveId mv) const;
    bool operator==(const Position& y) const;
    inline bool operator!=(const Position& y) const { return !(operator==(y)); }
    
    struct Hash {
        inline size_t operator() (const PositionPtr& x) const
        { return x->hash; }
    };
    
    struct Equal {
        inline bool operator() (const PositionPtr& x, const PositionPtr& y) const
        { return (*x == *y); }
    };
    
  private:
    Position(VectorTermPtr&& f);
    Position(const Position& p);
    Position(const Position& p, const Move& m, RoleId pid);
    std::vector<TermPtr> moves_to_play;
    struct Pos {
        Pos(VectorTermPtr&& f) : facts(std::forward<VectorTermPtr>(f)) {}
        VectorTermPtr facts;
        std::vector<std::vector<TermPtr>> legals;
        std::vector<Score> goals;
    };
    std::shared_ptr<Pos> pos;
    size_t hash;
    std::set<RoleId> played;
    
    friend std::ostream& operator<<(std::ostream& out, const Position& p);
};

class Game {
  public:
    Game(int n) : nb_roles(n), rand_gen((std::random_device()).operator()()) {}
    Game(std::unique_ptr<Reasoner>&& reasoner);
    //Game(const Game& game);
    ~Game() { std::cout << "Destroy Game" << std::endl; }
    
    //Game* copy() const; // use Game(const Game& game); to return a deepcopy (copy of the reasoner)
    
    inline size_t getNbRoles() const { return nb_roles; }
    inline PositionPtr getInitPos() const { return init_pos; }
    inline RoleId getRoleId(TermPtr role) const { return inv_roles.at(role); }
    PositionPtr next(PositionPtr& pos, MovePtr& move);
    bool playout(PositionPtr& pos, std::vector<Score>& v);

    std::vector<MovePtr> getMoves(VectorTermPtr moves) const;
    void print(std::ostream& flux) const;
    
  private:
    void construct();
    size_t nb_roles;
    PositionPtr init_pos;
    std::unique_ptr<Reasoner> reasoner;
    std::vector<TermPtr> roles;
    std::unordered_map<TermPtr, RoleId> inv_roles;
    std::default_random_engine rand_gen;
    
    friend std::ostream& operator<<(std::ostream& out, const Game& g);
};


#endif /* defined(__playerGGP__Game__) */
