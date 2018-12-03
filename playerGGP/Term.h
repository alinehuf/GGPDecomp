//
//  Term.h
//  playerGGP
//
//  Created by Dexter on 18/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef __playerGGP__Term__
#define __playerGGP__Term__

#include <iostream>
#include <vector>
#include <set>
#include "Atom.h"

/* Term - container for a gdl term (functor + args),
 * note : functor without args represent an atom */
class Term;

typedef const Term * TermPtr;
typedef std::vector<TermPtr> VectorTermPtr;
typedef std::set<TermPtr> SetTermPtr;

class Term {
  private:
    AtomPtr functor;
    const VectorTermPtr args;
    size_t hash;       // functor + args hash

    Term(AtomPtr atom);
    Term(AtomPtr atom, VectorTermPtr&& args);
    ~Term() {};        // nothing to free, it doesn't own its Term arguments, just a pointers on them
    Term(const Term&); // copy prohibited
    
    static size_t computeHash(AtomPtr atom, const VectorTermPtr& args);

public:
    AtomPtr getFunctor() const { return functor; };
    const VectorTermPtr& getArgs() const { return args; };
    size_t getHash() const { return hash; };

    bool operator<(Term const& ct) const;

    friend class GdlFactory;
    friend std::ostream& operator<<(std::ostream& out, const Term& t);
};

#endif /* defined(__playerGGP__Term__) */
