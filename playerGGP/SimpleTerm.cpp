//
//  SimpleTerm.cpp
//  playerGGP
//
//  Created by Dexter on 18/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#include "SimpleTerm.h"

/*******************************************************************************
 *      SimpleTerm
 ******************************************************************************/

/* for debugging, print recursively a SimpleTerm */
std::ostream& operator<<(std::ostream& out, const SimpleTerm& st) {
    static int indent;
    if (st.isAtom())
        out << st.atom << " ";
    else {
        out << "\n";
        for (int i = 0; i < indent; ++i) out << "    ";
        out << "( ";
        ++indent;
        for(const SimpleTermPtr& t : st.term) {
            out << *t;
        }
        indent--;
        out << ") ";
    }
    return out;
}
