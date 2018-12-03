//
//  SimpleTerm.h
//  playerGGP
//
//  Created by Dexter on 18/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef playerGGP_SimpleTerm_h
#define playerGGP_SimpleTerm_h

#include <iostream>
#include <vector>

/* SimpleTerm : container for an atom => string
 *                          or a term => vector<SimpleTerm *>
 * allow the construction of a nested list of terms, atoms are not uniques
 *
 * warning : no copy constructor or operator= defined !
 */
class SimpleTerm;
typedef SimpleTerm * SimpleTermPtr;

class SimpleTerm {
  private:
    const std::string atom;
    std::vector<SimpleTermPtr> term;
    
  public:
    inline SimpleTerm() {}
    inline SimpleTerm(std::string&& str) : atom(std::forward<std::string>(str)) {}
    inline ~SimpleTerm() {
        for (SimpleTermPtr t : term) {
            delete t;
        }
    }
    inline void add(SimpleTermPtr t) { term.push_back(t); }
    inline bool isAtom() const { return term.size() == 0; }
    inline const std::string& getAtom() const { return atom; }
    inline const std::vector<SimpleTermPtr>& getGdlSimpleTerm() const {
        return term;
    }
    
    friend std::ostream& operator<<(std::ostream& out, const SimpleTerm& st);
};

#endif
