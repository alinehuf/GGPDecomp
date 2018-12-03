//
//  Gate.h
//  playerGGP
//
//  Created by Dexter on 01/12/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef __playerGGP__Gate__
#define __playerGGP__Gate__

#include <set>
#include <assert.h>
#include "Term.h"

class Gate;
typedef Gate * GatePtr;
typedef std::set<GatePtr> SetGatePtr;
typedef std::vector<GatePtr> VectorGatePtr;

typedef size_t GateId;
typedef GateId FactId;

struct Fact {
    Fact() : id(0), value(true) {}
    Fact(size_t i, bool val) : id(i), value(val) {}
    Fact(const Fact& f) : id(f.id), value(f.value) {}
    FactId id;
    bool value;
    
    bool operator<(const Fact f) const {
        return (id < f.id || (id == f.id && value < f.value) );
    }
    bool operator==(const Fact f) const {
        return (id == f.id && value == f.value);
    }

    bool operator!=(const Fact f) const {
        return (id != f.id || value != f.value);
    }
//    Fact& operator=(const Fact& f) const {
//        id = f.id;
//        value = f.value;
//        return *this;
//    }
};

class Gate { // Logic Gate
  public:
    //typedef enum : int {WITH_DOES = 0b0000, NO_DOES = 0b0100, SUB_EXPR = 0b0101, MAJOR_SUB_EXPR = 0b0111, WITH_NOT = 0b1111} Status; // WITH_MAJOR = 0b0110,
//    typedef enum : int {WITH_DOES = 0b0000, NO_DOES = 0b0001, EXPR = 0b0011, SUB_EXPR = 0b0111, MAJOR_SUB_EXPR = 0b1111} Status;
//    struct Premises {
//        std::set<Fact> trues;
//        std::set<Fact> does;
//        Premises add(Premises p) {
//            assert(&p != this);
//            trues.insert(p.trues.begin(), p.trues.end());
//            does.insert(p.does.begin(), p.does.end());
//            return *this;
//        }
//        bool containsFact(size_t id, bool value) const {
//            Fact f(id, value);
//            return (trues.find(f) != trues.end());
//        }
//        bool empty() {
//            return (trues.empty() && does.empty());
//        }
//        bool operator<(const Premises p) const {
//            return (trues < p.trues || (!(trues > p.trues) && does < p.does) );
//        }
//    };
  private :
    int typeCirc;
    int insideCirc;
    int stratum;
//    int gstratum;
//    int tstratum;
    GateId gate_id;
    //bool no_does;
    //bool is_subexpr;
    int status;

//    std::set<Premises> set_premisses;

  public:
    enum Type {NONE, FACT, WIRE, AND, OR, NOT};
    inline Type getType() const { return type; };
    inline TermPtr getTerm() const { return term; };
    inline size_t getHash() const { return hash; };
    inline const SetGatePtr& getInputs() const { return inputs; };
    inline  const SetGatePtr& getOutputs() const { return outputs; };
    bool operator==(const Gate& g) const;
    
    typedef enum : int { TERM = 0b1, LEG = 0b10, GL = 0b100, NXT = 0b1000, LEG_NXT = 0b1010, GL_TERM = 0b101} TypeCirc;
    inline void setTypeCirc(int t) { typeCirc = t; }
    inline int getTypeCirc() { return typeCirc; }
    inline void setInsideCirc(int t) { insideCirc = t; }
    inline int getInsideCirc() { return insideCirc; }
    inline void setStratum(int s) { stratum = s; }
//    inline void setGstratum(int s) { gstratum = s; }
//    inline void setTstratum(int s) { tstratum = s; }
    inline int getStratum() { return stratum; }
//    inline int getGstratum() { return gstratum; }
//    inline int getTstratum() { return tstratum; }
    inline void setId(int i) { gate_id = i; }
    inline GateId getId() { return gate_id; }
    
//    void setStatus (Status s) { status = s; }
//    inline bool isActionIndependent() const { return ((status & NO_DOES) == NO_DOES); }
//    inline bool isSubExpr() const { return ((status & SUB_EXPR) == SUB_EXPR); }
//    inline bool isMajorSubExpr() const { return ((status & MAJOR_SUB_EXPR) == MAJOR_SUB_EXPR); }
    //inline bool isWithNot() const { return (status == WITH_NOT); }
    
    
//    void computePremises(std::set<GateId>& sub_expr, const std::vector<GateId>& id_does, bool redo = false);
//    void insertValidatedPremises(Premises&& new_prem, const std::vector<GateId>& id_does);
//    void insertAndSimplifyPremises(Premises&& new_prem);
//    const std::set<Premises>& getPremises() const { return set_premisses; };

    
  private:
    Type type;
    TermPtr term;
    size_t hash;
    SetGatePtr inputs;
    SetGatePtr outputs;
    
    Gate(Type type, TermPtr t, SetGatePtr&& inputs);
    ~Gate() {};
    Gate(const Gate&); // copy prohibited
    void computeHash();
    void swapExceptOutputs(Gate& gate);

    friend class GateFactory;
    friend std::ostream& operator<<(std::ostream& out, const Gate& t);
};


#endif /* defined(__playerGGP__Gate__) */
