//
//  YapData.cpp
//  playerGGP
//
//  Created by Dexter on 25/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#include <set>
#include "YapData.h"

/*******************************************************************************
 *      YapData : to manage YAP user CPredicates and retrieve data from YAP
 ******************************************************************************/

void YapData::init(Yap& yap, YapTranslator& t) {
    translator = &t;
    storeset = std::unique_ptr<StoreSetStruct>(new StoreSetStruct(yap));
    storeset->storeset_result = std::set<TermPtr>();
    storeground = std::unique_ptr<StoreGroundStruct>(new StoreGroundStruct(yap));
    storeground->storeground_result = std::set<TermPtr>();
}

/* structure containing a user predicate and method to retrieve data */
StoreSetStruct::StoreSetStruct(Yap& yap) {
    // user predicate
    yap.UserCPredicate("storeset", storeset, 1);

}

/* user predicate that always returns true but has a side effect: 
 * record his argument into storeset_result */
Yap::Bool StoreSetStruct::storeset(Yap* yap) {
    Yap::Term ret = yap->A(1);
    YapData* data = reinterpret_cast<YapData*>(yap->GetData());
    TermPtr t = data->translator->yapToGdl(ret);
    //std::cout << "storeset <= " << *t << std::endl;
    std::set<TermPtr>& storeset_result = data->storeset->storeset_result;
    storeset_result.insert(t);
    return true;
}

/* get the contents of storeset_result and clear it */
VectorTermPtr StoreSetStruct::getData() {
    VectorTermPtr v;
    v.reserve(storeset_result.size());
    v.assign(storeset_result.begin(), storeset_result.end());
    storeset_result.clear();
    return v;
}

/* structure containing a user predicate and method to retrieve data */
StoreGroundStruct::StoreGroundStruct(Yap& yap) {
    // user predicate
    yap.UserCPredicate("storeground", storeground, 1);
}

/* user predicate that always returns true but has a side effect:
 * record his argument into storeground_result */
Yap::Bool StoreGroundStruct::storeground(Yap* yap) {
    Yap::Term ret = yap->A(1);
    bool with_var = false;
    YapData* data = reinterpret_cast<YapData*>(yap->GetData());
    TermPtr t = data->translator->yapToGdl(ret, &with_var);
    //std::cout << "storeset <= " << *t << std::endl;
    std::set<TermPtr>& storeground_result = data->storeground->storeground_result;
    if(with_var)
        std::cerr << "Warning: term " << *t << " not grounded" << std::endl;
    else
        storeground_result.insert(t);
    return true;
}

/* get the contents of storeground_result and clear it */
VectorTermPtr StoreGroundStruct::getData() {
    VectorTermPtr v;
    v.reserve(storeground_result.size());
    v.assign(storeground_result.begin(), storeground_result.end());
    storeground_result.clear();
    return v;
}
