//
//  GdlFactory.h
//  playerGGP
//
//  Created by dexter on 24/10/13.
//  Copyright (c) 2013 dex. All rights reserved.
//

#ifndef __playerGGP__GdlFactory__
#define __playerGGP__GdlFactory__

#include <vector>
#include <set>
#include <map>
#include "SimpleTerm.h"
#include "Term.h"

/* object containing the GDL code and allowing to handle it */
class GdlFactory {
  private:
    std::map<size_t, TermPtr> termsDict;     // to store every single Term
    std::map<size_t, AtomPtr> atomsDict;     // to store every single Atom
    
    SimpleTermPtr parseGdlText(std::string::const_iterator& c_iter, std::string::const_iterator& c_end);
    
  public:
    GdlFactory() {};
    //GdlFactory(const std::string& text);
    ~GdlFactory();  // free atomsDict and termsDict
    
    std::string removeComments(const std::string& text);    // remove comments and redundant white spaces
    SimpleTermPtr parseGdlText(const std::string& text);    // parse GDL string into SimpleTerm
    
    VectorTermPtr computeGdlCode(const char *filename);                     // file to Terms
    VectorTermPtr computeGdlCode(std::string&& gdlText);                    // string to Terms
    VectorTermPtr computeGdlCode(const std::vector<SimpleTermPtr>& gdlSimpleCode); // SimpleTerms to Terms
    TermPtr computeGdlCode(SimpleTermPtr term);                             // one SimpleTerm to Term
    
    AtomPtr getOrAddAtom(std::string&& str);                         // get an atom from a string or add it
    TermPtr getOrAddTerm(std::string&& str);                         // get an term from a string or add it
    TermPtr getOrAddTerm(AtomPtr atom);                              // add a term consisting of a single atom
    TermPtr getOrAddTerm(AtomPtr atom, VectorTermPtr&& args); // add a term : functor + args
    
    // for debug
    void printGdlSimpleCode(std::ostream& out, SimpleTermPtr code);  // print a SimpleTerm
    void printGdlCode(std::ostream& out, const VectorTermPtr& code) const ; // print a list of Term
    void printGdlCode(std::ostream& out, const SetTermPtr& code); // print a set of Term
    void printAtomsDict(std::ostream& out);
    void printTermsDict(std::ostream& out);
};

#endif /* defined(__playerGGP__GdlFactory__) */
