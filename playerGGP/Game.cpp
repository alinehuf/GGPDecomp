//
//  Game.cpp
//  playerGGP
//
//  Created by Dexter on 07/05/2015.
//  Copyright (c) 2015 dex. All rights reserved.
//

#include "Game.h"
#include <algorithm>

using std::string;
using std::vector;
using std::cout;
using std::endl;

/*******************************************************************************
 *      Move
 ******************************************************************************/

//string Move::getAction() const {
//    return (*mv->getArgs().rbegin())->getFunctor()->getName();
//}

std::ostream& operator<<(std::ostream& out, const Move& m) {
    return (out << *m.mv);
}

//bool Move::operator<(const Move& move) const {
//    return (mv < move.mv);
//}
//
//bool Move::operator==(const Move& move) const {
//    return (mv == move.mv);
//}

/*******************************************************************************
 *      Position
 ******************************************************************************/

Position::Position(VectorTermPtr&& f): pos(new Pos(std::forward<VectorTermPtr>(f))) {
    std::sort(pos->facts.begin(), pos->facts.end(), [](TermPtr x, TermPtr y) { return x < y; } );
    hash = 0;
    int i = 1;
    for(TermPtr e : pos->facts) {
        hash ^= e->getHash() * i;
        i += 2;
    }
    hash *= 11;
}

Position::Position(const Position& p, const Move& m, RoleId pid): moves_to_play(p.moves_to_play), pos(p.pos), played(p.played) {
    moves_to_play.push_back(m.mv);
    std::sort(moves_to_play.begin(), moves_to_play.end(), [](TermPtr x, TermPtr y) { return x < y; } );
    played.insert(pid);
    hash = p.hash ^ m.mv->getHash();
}

int Position::getNbLegal(RoleId pid) const {
    assert(played.find(pid) == played.end());
    if(!pos->goals.empty())
        return 0;
    return int(pos->legals[pid].size());
}

bool Position::isTerminal() const {
    return !pos->goals.empty();
}

Score Position::getGoal(RoleId pl) const {
    assert(!pos->goals.empty());
    return pos->goals[pl];
}

const vector<Score>& Position::getGoals() const {
    assert(!pos->goals.empty());
    return pos->goals;
}

MovePtr Position::getMove(RoleId pid, MoveId mv) const {
    assert(pid < pos->legals.size());
    assert(mv < (int) pos->legals[pid].size());
    assert(played.find(pid) == played.end());
    return MovePtr(new Move(pos->legals[pid][mv]));
}

std::ostream& operator<<(std::ostream& out, const Position& p) {
    for (TermPtr f : p.pos->facts)
        out << *f << ' ';
    for (TermPtr m : p.moves_to_play)
        out << *m;
    out << " (" << p.hash << ")";
    return out;
}

bool Position::operator==(const Position& p) const {
    return (this == &p ||
            (hash == p.hash && pos->goals == p.pos->goals && moves_to_play == p.moves_to_play && pos->facts == p.pos->facts));
}

/*******************************************************************************
 *      Game
 ******************************************************************************/

Game::Game(std::unique_ptr<Reasoner>&& reasoner) : reasoner(std::forward<std::unique_ptr<Reasoner>>(reasoner)), rand_gen((std::random_device()).operator()()) {
    construct();
}

//Game::Game(const Game& game) : reasoner(std::unique_ptr<Reasoner>(game.reasoner->copy())), rand_gen((std::random_device()).operator()()) {
//    construct();
//}

void Game::construct() {
    roles = reasoner->getRoles();
    for(RoleId pid = 0; pid < roles.size(); ++pid)
        inv_roles[roles[pid]] = pid;
    nb_roles = roles.size();
    VectorTermPtr f(reasoner->getInitPos());
    reasoner->setPosition(f);
    Position* p = new Position(std::move(f));
    if(reasoner->isTerminal()) {
        for(TermPtr r : roles)
            p->pos->goals.push_back(reasoner->getGoal(r));
    } else {
        for(TermPtr r : roles) {
            p->pos->legals.push_back(reasoner->getLegals(r));
            std::sort(p->pos->legals.back().begin(), p->pos->legals.back().end(), [](TermPtr x, TermPtr y) { return x < y; });
        }
    }
    init_pos = PositionPtr(p);
}

//Game* Game::copy() const {
//    Game* game = new Game(*this);
//    return game;
//}

PositionPtr Game::next(PositionPtr& p, MovePtr& m) {
    assert(!p->isTerminal());
    if(p->moves_to_play.size() < nb_roles - 1) {
        RoleId pid = inv_roles.at(m->mv->getArgs().front());
        return PositionPtr(new Position(*p, *m, pid));
    }
    assert(p->moves_to_play.size() == p->pos->legals.size() - 1);
    reasoner->setPosition(p->pos->facts);
    for(TermPtr mtp : p->moves_to_play)
        reasoner->setMove(mtp);
    reasoner->setMove(m->mv);
    reasoner->next();
    Position* newpos;
    if(reasoner->isTerminal()) {
        newpos = new Position(VectorTermPtr());
        for(TermPtr r : roles)
            newpos->pos->goals.push_back(reasoner->getGoal(r));
    } else {
        VectorTermPtr facts = reasoner->getPosition();
        newpos = new Position(std::move(facts));
        for(TermPtr r : roles) {
            newpos->pos->legals.push_back(reasoner->getLegals(r));
            std::sort(newpos->pos->legals.back().begin(), newpos->pos->legals.back().end(), [](TermPtr x, TermPtr y) { return x < y; });
        }
    }
    return PositionPtr(newpos);
}

bool Game::playout(PositionPtr& p, vector<Score>& scores) {
    // terminal ? get the scores
    if(p->isTerminal()) {
        scores.clear();
        for(Score s : p->getGoals())
            scores.push_back(s);
        return true;
    }
    // some moves are chosen ? chose the moves for the remaining players and do a playout
    if(!p->moves_to_play.empty()) {
        PositionPtr nextpos = p;
        for(RoleId pl = 0; pl < nb_roles; ++pl) {
            if(p->played.find(pl) != p->played.end())
                continue;
            MovePtr nextmove = p->getMove(pl, rand_gen() % p->getNbLegal(pl));
            nextpos = next(nextpos, nextmove);
            if(nextpos == nullptr)
                return false;
        }
        return playout(nextpos, scores);
    }
    // do a playout
    reasoner->setPosition(p->pos->facts);
    reasoner->playout();
    scores.clear();
    for(TermPtr r : roles)
        scores.push_back(reasoner->getGoal(r));
    return true;
}

vector<MovePtr> Game::getMoves(VectorTermPtr moves) const {
    vector<MovePtr> m;
    AtomPtr does = reasoner->getGdlFactory().getOrAddAtom("does");
    for(size_t i = 0; i < roles.size(); ++i) {
        VectorTermPtr v(2);
        v[0] = roles[i];
        v[1] = moves[i];
        m.push_back(MovePtr(new Move(reasoner->getGdlFactory().getOrAddTerm(does, std::move(v)))));
    }
    return m;
}

std::ostream& operator<<(std::ostream& out, const Game& g) {
    out << "Roles: ";
    for(size_t i = 0; i < g.nb_roles; ++i)
        out << *g.roles[i] << ' ';
    return out;
}

