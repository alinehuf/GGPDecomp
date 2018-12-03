//
//  YapTranslator.cpp
//  playerGGP
//
//  Created by dexter on 13/11/13.
//  Copyright (c) 2013 dex. All rights reserved.
//

#include <cassert>
#include "YapTranslator.h"

using std::cout;
using std::cerr;
using std::endl;
using std::pair;

// reservé "role", "init", "input", "base", "<=", "next", "true", "does", "not", "goal",
//         "terminal", "legal", "distinct", "sees", "or", "random", "',"

/*******************************************************************************
 *      YapTranslator
 ******************************************************************************/

YapTranslator::YapTranslator(Yap& y, GdlFactory& f) : factory(f), yap(y) {
    gdl_or = factory.getOrAddAtom("or");
    gdl_implied = factory.getOrAddAtom("<=");
    gdl_not = factory.getOrAddAtom("not");
    gdl_distinct = factory.getOrAddAtom("distinct");
    gdl_and = factory.getOrAddAtom("',");
    gdl_cons = factory.getOrAddAtom(".");
    gdl_var = factory.getOrAddTerm("?x");
}

/*******************************************************************************
 *      GDL => Yap::Term
 ******************************************************************************/

/* ask for the transformation of a term into yap
 * give a map to store the variables */
Yap::Term YapTranslator::gdlToYap(TermPtr t) {
    std::unordered_map<TermPtr, Yap::Term> vars;
    return gdlToYap(t, vars, true);
}

/* transform a term or rule into yap */
Yap::Term YapTranslator::gdlToYap(TermPtr t, std::unordered_map<TermPtr, Yap::Term>& vars, bool implied) {
    VectorTermPtr args = t->getArgs();
    int arity = (int) (args.size());
    AtomPtr atom = t->getFunctor();
    const std::string& atom_str = atom->getName();
    
    if (arity == 0) { // number, atom or variable
        if(atom_str.front() == '#') {     // an atom reported as number
            return yap.MkIntTerm(atom_str[1] - '0');
        }
        if(atom_str.front() != '?') {     // an atom
            auto cache = atomToYapTerm.find(atom);
            if(cache != atomToYapTerm.end()) {
                return cache->second;
            }
            Yap::Atom yatom = yap.LookupAtom(gdlAtomToYapString(atom).c_str());
            Yap::Term yterm = yap.MkAtomTerm(yatom);
            atomToYapTerm[atom] = yterm;
            yapAtomToTermPtr[yatom] = t;
            return yterm;
        }
        Yap::Term var;               // a variable
        if (atom_str.compare("?_") == 0) { // undefined var
            var = yap.MkVarTerm();
        } else {
            auto it = vars.find(t);
            if (it != vars.end())    // a known variable
                return it->second;
            var = yap.MkVarTerm();   // an unknown variable
            vars[t] =  var;
        }
        return var;
    }
    // a term with implicit conjunction, transform it and starts again
    if (implied && atom == gdl_implied && arity >= 3)
        return gdlToYap(impliedWithExplicitConj(t), vars);
    // a rule without premises : treat as a term
    if (implied && atom == gdl_implied && arity == 1)
        return gdlToYap(args.front(), vars);
    // an 'or' term with more than 2 arguments : transform it and starts again
    if (atom == gdl_or && arity >= 3)
        return gdlToYap(gdlOrToNestedOr(t), vars);
    // transform the functor
    Yap::Functor f;
    auto it = atomToYapFunctors.find(atom);
    if(it == atomToYapFunctors.end()) {
        Yap::Atom yatom = yap.LookupAtom(gdlAtomToYapString(atom).c_str());
        f = (atomToYapFunctors[atom][arity] = yap.MkFunctor(yatom, arity));
        yapAtomToTermPtr[yatom] = factory.getOrAddTerm(atom);
    } else {
        auto it2 = it->second.find(arity);
        if(it2 == it->second.end()) {
            Yap::Atom yatom = yap.LookupAtom(gdlAtomToYapString(atom).c_str());
            f = (it->second[arity] = yap.MkFunctor(yatom, arity));
        } else
            f = it2->second;
    }
    // transform the arguments
    Yap::Term yargs[arity];
    for(int i = 0; i < arity; i++) {
        yargs[i] = gdlToYap(args[i], vars);
    }
    return yap.MkApplTerm(f, arity, yargs);
}

/* transform a conjunction  */
TermPtr YapTranslator::impliedWithExplicitConj(TermPtr t) {
    const VectorTermPtr& args = t->getArgs();
    assert(t->getFunctor() == gdl_implied && args.size() >= 3);
    TermPtr conj = args.back();
    for(VectorTermPtr::const_reverse_iterator it = args.rbegin() + 1; it != args.rend() - 1; ++it) {
        VectorTermPtr conj2;
        conj2.reserve(2);
        conj2.push_back(*it);
        conj2.push_back(conj);
        conj = factory.getOrAddTerm(gdl_and, std::move(conj2));
    }
    VectorTermPtr new_args;
    new_args.reserve(2);
    new_args.push_back(args.front());
    new_args.push_back(conj);
    return factory.getOrAddTerm(gdl_implied, std::move(new_args));
}

/* transform a or with more than 2 arguments into a nested or term */
TermPtr YapTranslator::gdlOrToNestedOr(TermPtr t) {
    const VectorTermPtr& args = t->getArgs();
    assert(args.size() >= 3);
    TermPtr disj = args.back();
    for(VectorTermPtr::const_reverse_iterator it = args.rbegin() + 1; it != args.rend(); ++it) {
        VectorTermPtr disj2;
        disj2.reserve(2);
        disj2.push_back(*it);
        disj2.push_back(disj);
        disj = factory.getOrAddTerm(gdl_or, std::move(disj2));
    }
    return disj;
}

/* transform an atom into yap language */
std::string YapTranslator::gdlAtomToYapString(AtomPtr atom) {
    static const std::string impliedby(":-");
    static const std::string distinct("\\==");
    static const std::string neg("\\+");
    static const std::string disj(";");
    
    if (atom == gdl_implied)
        return impliedby;
    if (atom == gdl_distinct)
        return distinct;
    if (atom == gdl_not)
        return neg;
    if (atom == gdl_or)
        return disj;

    std::string str = atom->getName();
    if(str.front() == '\'') { // suppress ' indicating a yap predicate
        return str.c_str() + 1;
    }
    return str + ' '; // prevent from collision with reserved yap predicate name
}

/*******************************************************************************
 *      Yap::Term => GDL
 ******************************************************************************/


TermPtr YapTranslator::yapToGdl(Yap::Term t, bool* with_var) {
    if (yap.IsAtomTerm(t))    // atom
        return yapAtomToTermPtr.find(yap.AtomOfTerm(t))->second;
    if (yap.IsApplTerm(t)) {  // predicate with arguments
        Yap::Functor yfunct = yap.FunctorOfTerm(t);
        assert(yapAtomToTermPtr.find(yap.NameOfFunctor(yfunct)) != yapAtomToTermPtr.end());
        AtomPtr funct = yapAtomToTermPtr.find(yap.NameOfFunctor(yfunct))->second->getFunctor();
        int arity = yap.ArityOfFunctor(yfunct);
        VectorTermPtr vec(arity);
        for(int i = 1; i <= arity; ++i)
            vec[i - 1] = yapToGdl(yap.ArgOfTerm(i, t), with_var);
        return factory.getOrAddTerm(funct, std::move(vec));
    }
    if(yap.IsPairTerm(t)) {   // cons term (car . cdr)
        VectorTermPtr args;
        args.reserve(2);
        args.push_back(yapToGdl(yap.HeadOfTerm(t), with_var));
        args.push_back(yapToGdl(yap.TailOfTerm(t), with_var));
        return factory.getOrAddTerm(gdl_cons, std::move(args));
    }
    if(yap.IsVarTerm(t)) {   // variable
        if(with_var)
            *with_var = true;
        //return factory.computeGdlCode(std::to_string(t)).front();
        return factory.getOrAddTerm(std::string(yap.AtomName(yap.AtomOfTerm(t))));
    }
    if(yap.IsIntTerm(t))     // integer
        return factory.getOrAddTerm(std::to_string(yap.IntOfTerm(t)));
    throw std::exception();  // other ?
}

/*******************************************************************************
 *      print map contents => for debug
 ******************************************************************************/

void myPutC(int c) {
    cout << (char) c << endl;
}

void YapTranslator::showAtomToYapTerm(const std::string& s) {
    cout << "atomToYapTerm map ---------- " << s << endl;
    int i = 0;
    for (const auto& p : atomToYapTerm) {
        cout << "[" << i++ << "] " << *p.first << " ---> (" << p.second << ") ";
        //yap.Write(p.second, myPutC, YAP_WRITE_HANDLE_VARS | YAP_WRITE_QUOTED);
        cout << endl;
    }
}

void YapTranslator::showAtomToYapFunctors(const std::string& s) {
    cout << "atomToYapFunctors map ---------- " << s << endl;
    int i = 0;
    for (const auto& p : atomToYapFunctors) {
        for (const auto& f : p.second) {
            cout << "[" << i << "] " << *p.first << " ---> (" << f.second << ") ";
            //Yap::Term funct = yap.MkAtomTerm(yap.NameOfFunctor(f.second));
            //yap.Write(funct, myPutC, YAP_WRITE_HANDLE_VARS | YAP_WRITE_QUOTED);
            cout << "/" << f.first << endl;
        }
        i++;
    }
}
void YapTranslator::showYapAtomToTermPtr(const std::string& s) {
    cout << "yapAtomToTermPtr map ---------- " << s << endl;
    int i = 0;
    for (const auto& p : yapAtomToTermPtr) {
        cout << "[" << i++ << "] (" << p.first << ") ";
        //Yap::Term yt = yap.MkAtomTerm(p.first);
        //yap.Write(yt, myPutC, YAP_WRITE_HANDLE_VARS | YAP_WRITE_QUOTED);
        cout << " ---> " << *p.second << endl;
    }
}


