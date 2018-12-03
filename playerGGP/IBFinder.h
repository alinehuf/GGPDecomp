//
//  IBFinder.h
//  playerGGP
//
//  Created by Dexter on 03/12/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef __playerGGP__IBFinder__
#define __playerGGP__IBFinder__

#include "GdlFactory.h"
#include "GdlTools.h"
#include "YapTranslator.h"

//! This class allows to instantiate a GDL description.
class IBFinder {
  public:
    IBFinder(GdlFactory& factory, const VectorTermPtr& gdl, GdlTools& gdlTools);
    ~IBFinder() {};
    
    const VectorTermPtr& getAllInputs() const { return all_inputs; };
    const VectorTermPtr& getbases() const { return bases; };
    
  private:
    GdlFactory& factory;                       // a factory : string -> Term
    std::unique_ptr<YapTranslator> translator; // translator Term -> Yap::Term
    std::unique_ptr<Yap> yap;
    
    VectorTermPtr all_inputs;
    VectorTermPtr bases;
    
    static constexpr const char* input_base_predicates =
#include "InputBase.pred"
    ;
};

#endif /* defined(__playerGGP__IBFinder__) */

