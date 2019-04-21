//
//  Circuit.h
//  playerGGP
//
//  Created by Dexter on 06/12/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef __playerGGP__Circuit__
#define __playerGGP__Circuit__

#include <unordered_map>
#include <array>
#include <stack>
#include <random>
#include "Gate.h"
#include "Propnet.h"

#define BOOL_F 0x0000
#define BOOL_T 0xFFFF
#define UNDEF  0x00FF

#define RUNDEF 0xFF00

#define MAYB_F 0xF000
#define MAYB_T 0x0FFF
#define BOTH   0xF0F0

#define DIF_TRUE_FALSE 2 // distance between USED_FALSE and USED_TRUE in bits

#define NOT_USED   0x0000
#define USED_FALSE 0xC000
#define USED_TRUE  0x3000
#define USED_TANDF 0xF000

#define DO_NOT_USE 0x000F

#define USED_CONJ  0x000F
#define USED_REVERS 0x0C00
#define USED_NORMAL 0x0300
#define USED_STATUS 0x0F00

#define WITH_FALSE  0x00C0
#define WITH_TRUE   0x0030
#define WITH_TANDF  0x00F0

typedef unsigned short Boolean;
typedef std::vector<Boolean> VectorBool;
typedef std::vector<int> VectorInt;
typedef short int Score;


class Circuit {
public:
    Circuit(Propnet& propnet);
    ~Circuit() {};
    
    struct OpAnd {
        GateId res;
        GateId arg1;
        GateId arg2;
    };
    
    struct OpOr {
        GateId res;
        GateId arg1;
        GateId arg2;
    };
    
    struct OpNot {
        GateId res;
        GateId arg;
    };
    
    struct Stratum {
        std::vector<OpNot> v_not;
        std::vector<OpAnd> v_and;
        std::vector<OpOr> v_or;
    };
    
    struct Info {
        VectorTermPtr roles;
        std::map<TermPtr, RoleId> inv_roles;
        VectorTermPtr init;
        std::vector<TermPtr> vars;
        std::unordered_map<TermPtr, size_t> inv_vars;
        
        std::unordered_map<size_t, TermPtr> sub_expr_terms;
        std::vector<GatePtr> gates;       // Splitter4, Splitter6
        std::set<GateId> sub_expr;        // Splitter4
        
        GateId id_true;
        std::vector<GateId> id_does;
        GateId id_terminal;
        std::vector<GateId> id_legal;
        GateId id_next;
        std::vector<GateId> id_goal;
        GateId id_end;
        size_t values_size;
        
        enum TypeCircuit { TERMINAL = 0, LEGAL = 1, GOAL = 2, NEXT = 3, NB_TYPE_PROG = 4 };
        std::array<std::vector<Stratum>, NB_TYPE_PROG> circuits;
        std::vector<std::vector<GateId> > goal_gates; // tout le circuit GOAL par strates
        std::vector<std::vector<GateId> > terminal_gates; // tout le circuit TERMINAL par strates
    };
    
    GdlFactory& getGdlFactory() const { return gdl_factory; }
    const Info& getInfos() const { return infos; }
    const VectorTermPtr& getInitPos() const;
    const VectorTermPtr& getRoles() const;
    void setPosition(VectorBool& values, const VectorTermPtr& pos) const;
    const VectorTermPtr& getPosition(const VectorBool& values) const;
    void playout(VectorBool& values);
    void terminal_legal_goal(VectorBool& values);
    bool isTerminal(const VectorBool& values) const ;
    const VectorTermPtr& getLegals(const VectorBool& values, TermPtr role) const;
    void setMove(VectorBool& values, TermPtr move) const;
    void next(VectorBool& values);
    Score getGoal(const VectorBool& values, TermPtr role) const;
    void computeGoal(VectorBool& values);
    
    // for debug
    void printVars(std::ostream& out) const;
    void printValues(const VectorBool& values) const;
    void printTrueValues(const VectorBool& values) const;
    void printRules(std::ostream& out) const;
    
private:
    Info infos;
    // to play with this circuit
    GdlFactory& gdl_factory;
    std::default_random_engine rand_gen;
    VectorTermPtr position;
    std::vector<Score> goals;
    std::vector<VectorTermPtr> legals;
    Boolean terminal = BOOL_F;
    
    // mark outputs and insert them into stack
    void insertAndMark(std::stack<GatePtr>& s, const std::vector<VectorGatePtr>& gates, Gate::TypeCirc type);
    void insertAndMark(std::stack<GatePtr>& s, const VectorGatePtr& vgates, Gate::TypeCirc type);
    // sort all gates (leaves first) and mark the circuits that use them
    std::deque<GatePtr> sortAndMark(std::stack<GatePtr>& s);
    // choose the circuit and stratum where to place the gate and its id
    void markCircuitStratumAndId(const std::deque<GatePtr>& order);
    
    void compute(Info::TypeCircuit type, VectorBool& values) const;
    void compute_not(const std::vector<OpNot>&, VectorBool& values) const;
    void compute_and(const std::vector<OpAnd>&, VectorBool& values) const;
    void compute_or(const std::vector<OpOr>&, VectorBool& values) const;
     
    void backPropagate(VectorBool& values) const;
    void backPropagate(const std::vector<OpNot>&, VectorBool& values) const;
    void backPropagate(const std::vector<OpAnd>&, VectorBool& values) const;
    void backPropagate(const std::vector<OpOr>&, VectorBool& values) const;
    
    void spread(VectorBool& values) const;
    void spread(const std::vector<OpNot>&, VectorBool& values) const;
    void spread(const std::vector<OpAnd>&, VectorBool& values) const;
    void spread(const std::vector<OpOr>&, VectorBool& values) const;

    void propagate3States(VectorBool& values) const;
    void propagate3States(const std::vector<OpNot>& v, VectorBool& values) const;
    void propagate3States(const std::vector<OpAnd>&, VectorBool& values) const;
    void propagate3States(const std::vector<OpOr>&, VectorBool& values) const;
    
    void propagate3StatesStrict(VectorBool& values) const;
    void propagate3StatesStrict(const std::vector<OpNot>& v, VectorBool& values) const;
    void propagate3StatesStrict(const std::vector<OpAnd>&, VectorBool& values) const;
    void propagate3StatesStrict(const std::vector<OpOr>&, VectorBool& values) const;
    
    void propagate2UndefStrict(VectorBool& values) const;
    void propagate2UndefStrict(const std::vector<OpNot>& v, VectorBool& values) const;
    void propagate2UndefStrict(const std::vector<OpAnd>&, VectorBool& values) const;
    void propagate2UndefStrict(const std::vector<OpOr>&, VectorBool& values) const;

    void propagateNot(VectorBool& values) const;
    void propagateNot(const std::vector<OpNot>& v, VectorBool& values) const;
    void propagateNot(const std::vector<OpAnd>&, VectorBool& values) const;
    void propagateNot(const std::vector<OpOr>&, VectorBool& values) const;

    void backPropagateConj(VectorBool& values) const;
    void backPropagateConj(const std::vector<OpNot>&, VectorBool& values) const;
    void backPropagateConj(const std::vector<OpAnd>&, VectorBool& values) const;
    void backPropagateConj(const std::vector<OpOr>&, VectorBool& values) const;

    void backPropagateNot(VectorBool& values) const;
    void backPropagateNot(const std::vector<OpNot>&, VectorBool& values) const;
    void backPropagateNot(const std::vector<OpAnd>&, VectorBool& values) const;
    void backPropagateNot(const std::vector<OpOr>&, VectorBool& values) const;
    
    void propagate3StatesCcl(VectorBool& values) const;
    void propagate3StatesCcl(const std::vector<OpNot>& v, VectorBool& values) const;
    void propagate3StatesCcl(const std::vector<OpAnd>&, VectorBool& values) const;
    void propagate3StatesCcl(const std::vector<OpOr>&, VectorBool& values) const;

    friend class Splitter6;
    friend class UctSinglePlayerDecomp;
};


#endif /* defined(__playerGGP__Circuit__) */
