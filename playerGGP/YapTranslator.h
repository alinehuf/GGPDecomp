//
//  YapTranslator.h
//  playerGGP
//
//  Created by dexter on 13/11/13.
//  Copyright (c) 2013 dex. All rights reserved.
//

#ifndef __playerGGP__YapTranslator__
#define __playerGGP__YapTranslator__

#include <yap++/YapInterface.hpp>
#include <iostream>
#include <unordered_map>
#include "GdlFactory.h"

typedef const Yap::Term * YapTermPtr;

class YapTranslator {
public:
    YapTranslator(Yap& yap, GdlFactory& f);
    ~YapTranslator() {};
    
    Yap::Term gdlToYap(TermPtr t);
    TermPtr yapToGdl(Yap::Term t, bool* with_var = nullptr);
    
    void showAtomToYapTerm(const std::string& = "");
    void showAtomToYapFunctors(const std::string& = "");
    void showYapAtomToTermPtr(const std::string& = "");
    
private:
    GdlFactory& factory;
    Yap& yap;
    
    AtomPtr gdl_or;
    AtomPtr gdl_implied;
    AtomPtr gdl_not;
    AtomPtr gdl_distinct;
    AtomPtr gdl_and;
    AtomPtr gdl_cons;
    TermPtr gdl_var;
    
    std::unordered_map<AtomPtr, Yap::Term> atomToYapTerm;
    std::unordered_map<AtomPtr, std::unordered_map<int, Yap::Functor> > atomToYapFunctors;
    std::unordered_map<Yap::Atom, TermPtr> yapAtomToTermPtr;
    
    std::string gdlAtomToYapString(AtomPtr atom);
    Yap::Term gdlToYap(TermPtr t, std::unordered_map<TermPtr, Yap::Term>& vars, bool i = false);
    TermPtr impliedWithExplicitConj(TermPtr t);
    TermPtr gdlOrToNestedOr(TermPtr t);
};


#endif /* defined(__playerGGP__YapTranslator__) */
