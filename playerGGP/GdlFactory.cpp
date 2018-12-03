//
//  GdlFactory.cpp
//  playerGGP
//
//  Created by dexter on 24/10/13.
//  Copyright (c) 2013 dex. All rights reserved.
//

#include "GdlFactory.h"

#include <sstream>
#include <fstream>
#include <cassert>
#include <iterator>

using std::stringstream;
using std::string;
using std::pair;
using std::vector;
using std::map;


/*******************************************************************************
 *      GdlFactory
 ******************************************************************************/

//GdlFactory::GdlFactory(const string &text) {
//    string pureGdl = removeComments(text);
//    cout << pureGdl << endl;
//    SimpleTermPtr gdlSimpleCode = parseGdlText(pureGdl);
//    printGdlSimpleCode(cout, gdlSimpleCode);
//    VectorTermPtr gdlCode = computeGdlCode(gdlSimpleCode);
//    printGdlCode(cout, gdlCode);
//}

GdlFactory::~GdlFactory() {
    // delete the Terms allocated and stored in the dictionnary
    for(const auto& t : termsDict) {
        delete t.second;
    }
    // delete the Atoms allocated and stored in the dictionnary
    for(const auto& t : atomsDict) {
        delete t.second;
    }
}

/*******************************************************************************
 *      From text to structure containing unique terms
 ******************************************************************************/

VectorTermPtr GdlFactory::computeGdlCode(const char * filename) {
    // open file
    std::ifstream in(filename, std::ios_base::in);
    if (!in) {
        std::cerr << "Error: Could not open input file" << std::endl;
        throw(errno);
    }
    in.unsetf(std::ios::skipws); // don't skip the white spaces. (skip by default)
    // read the contents into a string
    string contents((std::istream_iterator<char>(in)), std::istream_iterator<char>());
    in.close();
    
    return computeGdlCode(move(contents));
}

/* transform a GDL string into a hierarchy of unique Terms */
VectorTermPtr GdlFactory::computeGdlCode(string&& gdlText) {
    SimpleTermPtr simpleCode = parseGdlText(removeComments(gdlText)); // parse the text
    VectorTermPtr gdlCode = computeGdlCode(simpleCode->getGdlSimpleTerm()); // a hierarchy of unique terms
    delete simpleCode;  // doesn't need the parsed text anymore
    return gdlCode;
}

/* transform a parsed GDL (SimpleTerm) into a hierarchy of unique Terms */
VectorTermPtr GdlFactory::computeGdlCode(const vector<SimpleTermPtr>& gdlSimpleCode) {
    VectorTermPtr gdlCode;
    for (vector<SimpleTermPtr>::const_iterator it = gdlSimpleCode.begin(); it != gdlSimpleCode.end(); ++it) {
        gdlCode.push_back(computeGdlCode(*it));
    }
    return gdlCode;
}

/* transform recursively a SimpleTerm into Term */
TermPtr GdlFactory::computeGdlCode(SimpleTermPtr t) {
    if (t->isAtom()) {
        return getOrAddTerm(getOrAddAtom(string(t->getAtom())));
    }
    vector<SimpleTermPtr> r = t->getGdlSimpleTerm();
    VectorTermPtr args;
    for (vector<SimpleTermPtr>::iterator it = r.begin() + 1; it != r.end(); ++it) {
        args.push_back(computeGdlCode(*it));
    }
    assert(r.front()->isAtom());
    TermPtr gdl = getOrAddTerm(getOrAddAtom(string(r.front()->getAtom())), move(args));
    return gdl;
}

/* get an Atom in atomsDict if it exist, otherwise create a new Atom */
AtomPtr GdlFactory::getOrAddAtom(string&& str) {
    map<size_t, AtomPtr>::iterator it = atomsDict.find(Atom(str).getHash());
    if (it != atomsDict.end()) {
        return it->second;
    }
    AtomPtr a = new Atom(str);
    atomsDict[a->getHash()] = a;
    return a;
}

/* get an Atom Term in termsDict if it exist, otherwise create a new Term */
TermPtr GdlFactory::getOrAddTerm(string&& str) {
    return getOrAddTerm(getOrAddAtom(std::move(str)));
}

/* get an Atom Term in termsDict if it exist, otherwise create a new Term */
TermPtr GdlFactory::getOrAddTerm(AtomPtr atom) {
    map<size_t, TermPtr>::iterator it = termsDict.find(atom->getHash());
    if (it != termsDict.end()) {
        return it->second;
    }
    TermPtr t = new Term(atom);
    termsDict[atom->getHash()] = t;
    return t;
}

/* get a Term in termsDict if it exist, otherwise create a new Term */
TermPtr GdlFactory::getOrAddTerm(AtomPtr atom, VectorTermPtr&& args) {
    map<size_t, TermPtr>::iterator it = termsDict.find(Term::computeHash(atom, args));
    if (it != termsDict.end()) {
        return it->second;
    }
    TermPtr t = new Term(atom, std::move(args));
    termsDict[t->getHash()] = t;
    return t;
}

/*******************************************************************************
 *      From text to simpleGdlCode (nested list of string)
 ******************************************************************************/

/* parse the Gdl text and transform it into a vector of nested terms */
SimpleTermPtr GdlFactory::parseGdlText(const string &text) {
    string::const_iterator c_iter = text.begin();
    string::const_iterator c_end = text.end();
    SimpleTermPtr gdl = parseGdlText(c_iter, c_end);
    return gdl;
}

/* iterate recursively through the nested lists to transform them */
SimpleTermPtr GdlFactory::parseGdlText(string::const_iterator& c_iter, string::const_iterator& c_end) {
    SimpleTermPtr gdl = new SimpleTerm();
    bool insideWord = false;
    string word;
    
    while (c_iter < c_end) {
        if (*c_iter == ' ' || *c_iter == '(' || *c_iter == ')') {
            if (insideWord) {
                gdl->add(new SimpleTerm(string(word.c_str())));
                word.clear();
            }
            if (*c_iter == '(')
                gdl->add(parseGdlText(++c_iter, c_end));
            else if (*c_iter == ')')
                break;
            insideWord = false;
        } else {
            word.push_back(*c_iter);
            insideWord = true;
        }
        ++c_iter;
    }
    return gdl;
}

/* remove special characters, put the rest in lower case, remove the comments */
string GdlFactory::removeComments(const std::string& text) {
    bool inside_comment = false;
    std::stringstream newtext;
    char last = ' ';
    char current;
    int count = 0;
    
    for (string::const_iterator c = text.begin(); c != text.end(); ++c) {
        // remove special characters and put the rest in lower case
        if(*c >= 32 && *c <= 126) current = tolower(*c);
        else if (*c != '\n' && *c != '\r') current = ' ';
        else current = *c; // \n or \r
        // remove comments and empty lines
        if (current == ';')
            inside_comment = true;
        // if \r or \n => possibly end of a comment, add a space if necessary
        else if (current == '\r' || current == '\n') {
            if (last != ' ') {
                newtext << ' ';
                last = ' ';
            }
            inside_comment = false;
        }
        // not in a comment, add the char if not an already present space
        else if (!inside_comment && !(current ==' ' && last==' ')) {
            // count parenthessis
            if (current == '(') ++count;
            else if (current == ')') --count;
            // store char
            newtext << current;
            last = current;
        }
    }
    if (count != 0) {
        std::cerr << "error : unpaired parenthesis in gdl (" << count << ") !" << std::endl;
    }
    return newtext.str();
}

/*******************************************************************************
 *      Print functions for debug
 ******************************************************************************/

/* for debugging, print the gdlSimpleCode on out */
void GdlFactory::printGdlSimpleCode(std::ostream& out, SimpleTermPtr code) {
    out << "Simple GDL code :";
    if (code->isAtom())
        out << code << std::endl;
    else {
        for(SimpleTermPtr t : code->getGdlSimpleTerm()) {
            out << *t;
        }
        out << std::endl;
    }
}

/* for debugging, print the gdlCode on out */
void GdlFactory::printGdlCode(std::ostream& out, const VectorTermPtr& code) const {
    for(TermPtr t : code) {
        out << *t << std::endl;
    }
}

/* for debugging, print the gdlCode on out */
void GdlFactory::printGdlCode(std::ostream& out, const SetTermPtr& code) {
    for(TermPtr t : code) {
        out << *t << std::endl;
    }
}

/* for debugging, print the content of the atomsDict on out */
void GdlFactory::printAtomsDict(std::ostream& out) {
    for(auto entry : atomsDict) {
        out << "[" << entry.first << "] " << *entry.second << std::endl;
    }
}

/* for debugging, print the content of the termsDict on out */
void GdlFactory::printTermsDict(std::ostream& out) {
    for(auto entry : termsDict) {
        out << "[" << entry.first << "] " << *entry.second << std::endl;
    }
}

