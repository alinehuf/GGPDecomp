//
//  GdlTools.cpp
//  playerGGP
//
//  Created by Dexter on 22/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#include "GdlTools.h"
#include <queue>
#include <cassert>

using std::string;
using std::map;
using std::set;
using std::queue;
using std::deque;

GdlTools::GdlTools(GdlFactory& gdlFactory, VectorTermPtr gdl) : factory(gdlFactory), gdlCode(gdl) {
    gdl_distinct = factory.getOrAddAtom("distinct");
    gdl_implied = factory.getOrAddAtom("<=");
    gdl_or = factory.getOrAddAtom("or");
    gdl_not = factory.getOrAddAtom("not");
    gdl_ground_not = factory.getOrAddAtom("'groundnot");
    gdl_dummy = factory.getOrAddTerm("?./dummy");
    gdl_wildcard = factory.getOrAddAtom("_");
    partDynamicsFromStatics(gdl);
}

/*******************************************************************************
 *      Get all, static and dynamic predicates
 ******************************************************************************/

/* put dynamic predicates into dynamic_preds
 * put static predicates into static_preds
 * put all predicates into all_preds
 */
void GdlTools::partDynamicsFromStatics(const VectorTermPtr& gdl) {
    // get all the predicates
    map<TermPtr, SetTermPtr > to;
    for(TermPtr t : gdl) {
        if(t->getFunctor() == gdl_implied) {
            VectorTermPtr args = t->getArgs();
            TermPtr concl = args.front();
            TermPtr fun_t = getFunctorWithVars(concl);
            all_preds.insert(fun_t);
            SetTermPtr premises = getPremises(t);
            
            for(TermPtr prem : premises) {
                if(prem->getFunctor() != gdl_distinct || prem->getArgs().size() != 2) {
                    TermPtr fun_prem = getFunctorWithVars(prem);
                    to[fun_prem].insert(fun_t);
                    all_preds.insert(fun_prem);
                }
            }
        } else {
            all_preds.insert(getFunctorWithVars(t));
        }
    }
    // get dynamic predicates : they depend on another predicate
    // add some known predicates
    dynamic_preds.insert(factory.getOrAddTerm("terminal"));
    TermPtr var0 = factory.getOrAddTerm("?0 ");
    TermPtr var1 = factory.getOrAddTerm("?1 ");
    VectorTermPtr v(1, var0);
    dynamic_preds.insert(factory.getOrAddTerm(factory.getOrAddAtom("next"), VectorTermPtr(v)));
    dynamic_preds.insert(factory.getOrAddTerm(factory.getOrAddAtom("true"), VectorTermPtr(v)));
    v.push_back(var1);
    dynamic_preds.insert(factory.getOrAddTerm(factory.getOrAddAtom("goal"), VectorTermPtr(v)));
    dynamic_preds.insert(factory.getOrAddTerm(factory.getOrAddAtom("legal"), VectorTermPtr(v)));
    dynamic_preds.insert(factory.getOrAddTerm(factory.getOrAddAtom("does"), VectorTermPtr(v)));
    // find the others
    queue<TermPtr, deque<TermPtr>> q(deque<TermPtr>(dynamic_preds.begin(), dynamic_preds.end()));
    while(!q.empty()) {
        TermPtr t = q.front(); q.pop();
        for(TermPtr child : to[t]) {
            if(dynamic_preds.find(child) == dynamic_preds.cend()) {
                q.push(child);
                dynamic_preds.insert(child);
            }
        }
    }
    // get static predicates
    // The two lists all_preds and dynamic_preds are ordered in the same way
    // by comparing the pointers of each element, one can know if the predicate
    // is not in dynamic : it is then put into static
    auto all = all_preds.cbegin();
    auto dyn = dynamic_preds.cbegin();
    while(all != all_preds.cend() && dyn != dynamic_preds.cend()) {
        if(*all < *dyn) {
            static_preds.insert(*all);
            ++all;
        } else if (*dyn < *all) {
            ++dyn;
        } else {
            ++all;
            ++dyn;
        }
    }
    // add what remains in all_preds after the end of the dynamic
    for(;all != all_preds.cend(); ++all)
        static_preds.insert(*all);
    
//    std::cout << "static_preds -------------" << std::endl;
//    factory.printGdlCode(std::cout, static_preds);
//    std::cout << "dynamic_preds -------------" << std::endl;
//    factory.printGdlCode(std::cout, dynamic_preds);
//    std::cout << "all_preds -------------" << std::endl;
//    factory.printGdlCode(std::cout, all_preds);
}

/*******************************************************************************
 *      Check for static or dynamic
 ******************************************************************************/

bool GdlTools::isStatic(TermPtr t) {
    if(t->getFunctor() == gdl_implied || t->getFunctor() == gdl_not)
        t = t->getArgs().front();
    return static_preds.find(getFunctorWithVars(t)) != static_preds.end();
}

/*******************************************************************************
 *      Get functor / premises
 ******************************************************************************/

TermPtr GdlTools::getConclusion(TermPtr t) {
    if(t->getFunctor() != gdl_implied)
        return t;
    return t->getArgs().front();
}

TermPtr GdlTools::getFunctorWithVars(TermPtr t) {
    if(t->getArgs().empty())
        return t;
    VectorTermPtr l(t->getArgs().size());
    for(size_t i = 0; i < t->getArgs().size(); ++i) {
        l[i] = factory.getOrAddTerm(factory.getOrAddAtom('?' + std::to_string(i) + ' '));
    }
    return factory.getOrAddTerm(t->getFunctor(), std::move(l));
}

/* get all the terms included in the premises of t */
SetTermPtr GdlTools::getPremises(TermPtr t) {
    SetTermPtr premises;
    for(auto prem = t->getArgs().cbegin() + 1; prem != t->getArgs().cend(); ++prem)
        extractTerms(*prem, premises);
    return premises;
}

/* extract terms recursively
 * exemple : (or a b) or (or a (not b)) give [a b], (not a) give [a], a give [a]
 */
void GdlTools::extractTerms(TermPtr t, SetTermPtr& extracted) {
    if(t->getFunctor() == gdl_or || (t->getFunctor() == gdl_not && t->getArgs().size() == 1)) {
        for(TermPtr arg : t->getArgs())
            extractTerms(arg, extracted);
    } else {
        extracted.insert(t);
    }
}

///* put all arguments in term t into args */
//void GdlTools::getArgs(TermPtr t, SetTermPtr& args) {
//    for(TermPtr arg : t->getArgs()) {
//        if(arg->getArgs().empty()) {
//            args.insert(arg);
//        } else {
//            getArgs(arg, args);
//        }
//    }
//}

/*******************************************************************************
 *     Check whether a term can be fully instantiated, otherwise try to repair
 ******************************************************************************/

// check that  static are fully instanciated
// check that dynamic can be fully instantiated
TermPtr GdlTools::checkVars(TermPtr t) {
    // its a rule
    if(t->getFunctor() == gdl_implied) {
        // check if vars used by distinct or not are present before
        SetTermPtr vars;
        for(auto arg = t->getArgs().cbegin() + 1; arg != t->getArgs().cend(); ++arg) {
            if(!checkVars(*arg, vars)) {
                std::cerr << "Term " << **arg;
                std::cerr << " not fully instantiated in " << *t << std::endl;
                return tryToRepair(t);
            }
        }
        // check if all vars in premises are present in the conclusion
        SetTermPtr vars_concl;
        getVars(t->getArgs().front(), vars_concl);
        for(TermPtr v : vars_concl) {
            if(vars.find(v) == vars.end()) {
                std::cerr << "Term " << *t->getArgs().front();
                std::cerr << " not fully instantiated in " << *t << std::endl;
                return t;
            }
        }
        return t;
    }
    // its a term
    SetTermPtr vars_concl;
    getVars(t, vars_concl);
    if(!vars_concl.empty())
        std::cerr  << "Uninstantiated variables in " << *t << std::endl;
    return t;
}

// check if a premise distinct or not contains variables that are not
// in another premise before
bool GdlTools::checkVars(TermPtr t, SetTermPtr& vars) {
    if( (t->getFunctor() == gdl_not && t->getArgs().size() == 1)
        || (t->getFunctor() == gdl_distinct && t->getArgs().size() == 2) ) {
        SetTermPtr vars_in_not;
        getVars(t, vars_in_not);
        for(TermPtr var : vars_in_not)
            if(vars.find(var) == vars.cend())
                return false;
    } else if(t->getFunctor() == gdl_or && t->getArgs().size() >= 2) {
        for(auto arg = t->getArgs().cbegin(); arg != t->getArgs().cend(); ++arg)
            if(!checkVars(*arg, vars))
                return false;
    } else {
        getVars(t, vars);
    }
    return true;
}

/* put all variables in term t into vars */
void GdlTools::getVars(TermPtr t, SetTermPtr& vars) {
    for(TermPtr arg : t->getArgs()) {
        //if(arg->getArgs().empty()) {
        if(arg->getFunctor()->getName().front() == '?') {
            assert(arg->getArgs().empty());
            vars.insert(arg);
        } else {
            getVars(arg, vars);
        }
    }
}

/* try to repair a term with distinct or not with variables not seen before */
TermPtr GdlTools::tryToRepair(TermPtr t) {
    assert(t->getFunctor() == gdl_implied);
    VectorTermPtr args = t->getArgs();
    SetTermPtr vars_concl;
    SetTermPtr vars_prem;
    for(VectorTermPtr::iterator arg = args.begin() + 1; arg != args.end();) {
        if(!checkVars(*arg, vars_prem)) {
            if(!insertNoNotOrDistinct(args, arg)) {
                std::cerr << "Cannot be fixed" << std::endl;
                return t;
            }
        } else {
            ++arg;
        }
    }
    t = factory.getOrAddTerm(gdl_implied, std::move(args));
    std::cerr << "Fixed:  " << *t << std::endl;
    return t;
}
bool GdlTools::insertNoNotOrDistinct(VectorTermPtr& args, VectorTermPtr::iterator arg) {
    VectorTermPtr::iterator it;
    // pass the distinct or not terms
    for(it = arg + 1; it != args.end() && !noNotOrDistinct(*it); ++it) {}
    if(it == args.end())
        return false;
    TermPtr pos = *it; // keep the first term which isn't distinct or not
    for(auto it2 = it; it2 != arg; --it2)
        *it2 = *(it2 - 1);  // shifts all the arguments
    *arg = pos;             // place the term which isn't distinct or not before
    return true;
}

/* return true if the term isn't (or doesn't contain) not or distinct */
bool GdlTools::noNotOrDistinct(TermPtr t) {
    if((t->getFunctor() == gdl_not && t->getArgs().size() == 1)
       || (t->getFunctor() == gdl_distinct && t->getArgs().size() == 2)) {
        return false;
    }
    if(t->getFunctor() == gdl_or && t->getArgs().size() >= 2) {
        for(TermPtr arg : t->getArgs()) {
            if(!noNotOrDistinct(arg))
                return false;
        }
    }
    return true;
}

/*******************************************************************************
 *      Trandform GDL
 ******************************************************************************/

/* add a dummy arg in all the dynamic predicates to force instanciation */
TermPtr GdlTools::addDummy(TermPtr t) {
    if(t->getFunctor() == gdl_implied) {
        VectorTermPtr args;
        for(TermPtr arg : t->getArgs()) {
            args.push_back(addDummy(arg));
        }
        return factory.getOrAddTerm(gdl_implied, std::move(args));
    }
    else if(t->getFunctor() == gdl_not && t->getArgs().size() == 1) {
        VectorTermPtr args(1, addDummy(t->getArgs().front()));
        return factory.getOrAddTerm(gdl_not, std::move(args));
    }
    else if(t->getFunctor() == gdl_ground_not && t->getArgs().size() == 1) {
        VectorTermPtr args(1, addDummy(t->getArgs().front()));
        return factory.getOrAddTerm(gdl_ground_not, std::move(args));
    }
    else if(t->getFunctor() == gdl_or && t->getArgs().size() >= 2) {
        VectorTermPtr args;
        for(TermPtr arg : t->getArgs())
            args.push_back(addDummy(arg));
        return factory.getOrAddTerm(gdl_or, std::move(args));
    }
    else if(t->getFunctor() == gdl_distinct && t->getArgs().size() == 2) {
        return t;
    }
    else {
        VectorTermPtr args(t->getArgs());
        args.push_back(gdl_dummy);
        return factory.getOrAddTerm(t->getFunctor(), std::move(args));
    }
}

/* for all the or terms duplicate the rule with each possible argument of or */
VectorTermPtr GdlTools::removeOr(TermPtr t) {
    VectorTermPtr l;
    removeOr(t, l);
    return l;
}
void GdlTools::removeOr(TermPtr t, VectorTermPtr& l) {
    if(t->getFunctor() != gdl_implied) {
        l.push_back(t);
        return;
    }
    for(size_t i = 1; i < t->getArgs().size(); ++i) {
        TermPtr or_term = t->getArgs()[i];
        if(or_term->getFunctor() == gdl_or && or_term->getArgs().size() >= 2) {
            VectorTermPtr args = t->getArgs();
            args[i] = or_term->getArgs().front();
            removeOr(factory.getOrAddTerm(gdl_implied, std::move(args)), l);
            if(or_term->getArgs().size() > 2) {
                args = t->getArgs();
                VectorTermPtr or_args(or_term->getArgs().cbegin() + 1, or_term->getArgs().cend());
                args[i] = factory.getOrAddTerm(gdl_or, std::move(or_args));
                removeOr(factory.getOrAddTerm(gdl_implied, std::move(args)), l);
            } else {
                args = t->getArgs();
                args[i] = or_term->getArgs().back();
                removeOr(factory.getOrAddTerm(gdl_implied, std::move(args)), l);
            }
            return;
        }
    }
    l.push_back(t);
}

// replace not by groundnot
// TODO : voir si on ne peut pas supprimer directement les not en une passe
TermPtr GdlTools::removeNot(TermPtr t) {
    if(containsNot(t))
        return replaceNot(t);
    return t;
}
TermPtr GdlTools::replaceNot(TermPtr t) {
    if(t->getFunctor() == gdl_not && t->getArgs().size() == 1 && isStatic(t))
        std::cerr << "Negation of static term " << *t << std::endl;
    if(t->getFunctor() == gdl_not && t->getArgs().size() == 1 && !isStatic(t)) {
        VectorTermPtr args(1, t->getArgs().front());
        return factory.getOrAddTerm(gdl_ground_not, std::move(args));
    }
    if(t->getFunctor() == gdl_implied) {
        VectorTermPtr args = t->getArgs();
        for(VectorTermPtr::iterator arg = args.begin() + 1; arg != args.end(); ++arg)
            *arg = replaceNot(*arg);
        return factory.getOrAddTerm(gdl_implied, std::move(args));
    }
    if(t->getFunctor() == gdl_or && t->getArgs().size() >= 2) {
        VectorTermPtr args = t->getArgs();
        for(VectorTermPtr::iterator arg = args.begin(); arg != args.end(); ++arg)
            *arg = replaceNot(*arg);
        return factory.getOrAddTerm(gdl_or, std::move(args));
    }
    return t;
}
bool GdlTools::containsNot(TermPtr t) {
    if(t->getFunctor() == gdl_not && t->getArgs().size() == 1)
        return true;
    if(t->getFunctor() == gdl_implied) {
        for(VectorTermPtr::const_iterator arg = t->getArgs().cbegin() + 1; arg != t->getArgs().cend(); ++arg) {
            if(containsNot(*arg))
                return true;
        }
    } else if(t->getFunctor() == gdl_or && t->getArgs().size() >= 2) {
        for(TermPtr arg : t->getArgs())
            if(containsNot(arg))
                return true;
    }
    return false;
}

/* remove static terms from a rule */
TermPtr GdlTools::removeStatic(TermPtr t) {
    if(t->getFunctor() != gdl_implied)
        return t;
    VectorTermPtr args;
    args.push_back(t->getArgs().front());
    for(auto arg = t->getArgs().cbegin() + 1; arg != t->getArgs().cend(); ++arg) {
        if(!isStatic(*arg) && (*arg)->getFunctor() != gdl_distinct)
            args.push_back(*arg);
    }
    if(args.size() == 1)
        return t->getArgs().front();
    return factory.getOrAddTerm(gdl_implied, std::move(args));
}

/* add an argument to a term */
TermPtr GdlTools::addArg(TermPtr t, TermPtr arg) {
    VectorTermPtr args(t->getArgs());
    args.push_back(arg);
    return factory.getOrAddTerm(t->getFunctor(), std::move(args));
}

/* change conclusion of term t */
TermPtr GdlTools::changeConclusion(TermPtr t, TermPtr concl) {
    if(t->getFunctor() != gdl_implied)
        return concl;
    VectorTermPtr args = t->getArgs();
    args.front() = concl;
    return factory.getOrAddTerm(gdl_implied, std::move(args));
}

/* create a rule with a term (conclusion) and a premise, or add a premise to an existing rule */
TermPtr GdlTools::makeRuleWithPremise(TermPtr t, TermPtr prem) {
    VectorTermPtr args = t->getFunctor() == gdl_implied ? t->getArgs() : VectorTermPtr(1, t);
    args.push_back(prem);
    return factory.getOrAddTerm(gdl_implied, std::move(args));
}

/*******************************************************************************
 *      Trandform GDL : tests with the wilcards
 ******************************************************************************/


//TermPtr GdlTools::replaceWildcard(TermPtr t) {
//    AtomPtr functor(t->getFunctor());
//    VectorTermPtr args;
//    for (TermPtr arg : t->getArgs()) {
//        args.push_back(replaceWildcard(arg));
//    }
//    if (functor->getName().compare("?_") == 0)
//        functor = gdl_wildcard;
//    return factory.getOrAddTerm(functor, std::move(args));
//}

/* replace all unique variable by a wildcard */
TermPtr GdlTools::addWildcard(TermPtr t) {
    map<TermPtr, int> vars;
    getVars(t, vars);
    set<AtomPtr> uniqVars;
    for (const auto& v : vars)
        if (v.second == 1)
            uniqVars.insert(v.first->getFunctor());
    return replaceVarsByWilcard(t, uniqVars);
}
TermPtr GdlTools::replaceVarsByWilcard(TermPtr t, set<AtomPtr> vars) {
    AtomPtr functor(t->getFunctor());
    VectorTermPtr args;
    for (TermPtr arg : t->getArgs()) {
        args.push_back(replaceVarsByWilcard(arg, vars));
    }
    if (vars.find(functor) != vars.end())
        functor = gdl_wildcard;
    return factory.getOrAddTerm(functor, std::move(args));
}
void GdlTools::getVars(TermPtr t, map<TermPtr, int>& vars) {
    for(TermPtr arg : t->getArgs()) {
        if (arg->getFunctor()->getName().front() == '?') {
            vars[arg]++;
        } else {
            getVars(arg, vars);
        }
    }
}


///* replace all variable except those into the set by a wildcard */
//TermPtr GdlTools::keepOnlyVars(TermPtr t, set<AtomPtr> vars) {
//    AtomPtr functor = t->getFunctor();
//    VectorTermPtr args;
//    for (TermPtr arg : t->getArgs()) {
//        args.push_back(keepOnlyVars(arg, vars));
//    }
//    if (functor->getName().front() == '?' && vars.find(functor) == vars.end())
//        functor = gdl_wildcard;
//    return factory.getOrAddTerm(functor, std::move(args));
//}

VectorTermPtr GdlTools::decomposeClauses(TermPtr clause) {
    VectorTermPtr gdl;
    bool rewrite = false;
    if (clause->getFunctor() == factory.getOrAddAtom("<=")) {
        VectorTermPtr clause_args = clause->getArgs();
        TermPtr conclusion = *clause_args.begin();
        SetTermPtr vars;
        getVars(conclusion, vars);
        std::map<TermPtr, size_t> termToGroup;
        if (vars.size() > 1) {
            size_t current_idx = 1;
            for (auto it = clause_args.begin() + 1; it != clause_args.end(); ++it) {
                SetTermPtr prem_vars;
                getVars(*it, prem_vars);
                for (TermPtr v : prem_vars) {
                    if (vars.find(v) != vars.end()) {
                        size_t old_idx = termToGroup[v];
                        if (old_idx == 0)
                            termToGroup[v] = current_idx;
                        else {
                            for (auto& g : termToGroup) {
                                if (g.second == old_idx)
                                    g.second = current_idx;
                            }
                        }
                    }
                }
                ++current_idx;
            }
            
            std::map<size_t, std::set<AtomPtr> > groups;
            for (auto& g : termToGroup)
                groups[g.second].insert(g.first->getFunctor());
            
            if (groups.size() > 1) {
                VectorTermPtr args;
                for (auto& g : groups) {
                    args.push_back(conclusion);
                    for (auto it = clause_args.begin() + 1; it != clause_args.end(); ++it) {
                        SetTermPtr prem_vars;
                        getVars(*it, prem_vars);
                        if (g.second.find((*prem_vars.begin())->getFunctor()) != g.second.end()) {
                            args.push_back(*it);
                        }
                    }
                    gdl.push_back(factory.getOrAddTerm(clause->getFunctor(), move(args)));
                    rewrite = true;
                }
            }
        }
    }
    if (!rewrite)
        gdl.push_back(clause);

    //factory.printGdlCode(std::cout, gdl);
    return gdl;
}

