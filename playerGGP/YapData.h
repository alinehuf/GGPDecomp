//
//  YapData.h
//  playerGGP
//
//  Created by Dexter on 25/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef __playerGGP__YapData__
#define __playerGGP__YapData__

#include <yap++/YapInterface.hpp>
#include <set>
#include "YapTranslator.h"

struct YapData;

/* used to retrieve init, goal, input and base gdl terms */
struct StoreSetStruct {
    StoreSetStruct(Yap& yap);
    static Yap::Bool storeset(Yap* yap);  // Yap Cpredicate
    VectorTermPtr getData();
    std::set<TermPtr> storeset_result;
};

/* used to retrieve all the fully instanciated gdl rules (and terms ?) */
struct StoreGroundStruct {
    StoreGroundStruct(Yap& yap);
    static Yap::Bool storeground(Yap* yap);  // Yap Cpredicate
    VectorTermPtr getData();
    std::set<TermPtr> storeground_result;
    
};

/* used to manage the user CPredicates and data from YAP */
struct YapData {
    void init(Yap& yap, YapTranslator& t);

    std::unique_ptr<StoreSetStruct> storeset;
    std::unique_ptr<StoreGroundStruct> storeground;
    
    YapTranslator * translator;
};



#endif /* defined(__playerGGP__YapData__) */
