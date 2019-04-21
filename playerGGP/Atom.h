//
//  Atom.h
//  playerGGP
//
//  Created by Dexter on 22/11/2014.
//  Copyright (c) 2014 dex. All rights reserved.
//

#ifndef playerGGP_Atom_h
#define playerGGP_Atom_h

/* Atom - string with an associated hash key */
class Atom {
private:
    const std::string name;
    size_t hash;
    
    inline Atom(const std::string& s) : name(s), hash(std::hash<std::string>()(s)) {}
    inline ~Atom() {}
    Atom(const Atom&); // copy prohibited
public:
    inline const std::string& getName() const { return name; }
    inline size_t getHash() const { return hash; }
    
    friend class GdlFactory;
    friend std::ostream& operator<<(std::ostream& out, const Atom& a);
    
//    inline int operator < (const Atom &a) const {
//        if(hash != a.hash)
//            return (hash < a.hash);
//        return (name < a.name);
//    }
//    inline int operator > (const Atom &a) const {
//        if(hash != a.hash)
//          return (hash > a.hash);
//        return (name > a.name);
//    }
//    inline int operator == (const Atom &a) const {
//        if(hash != a.hash)
//          return false;
//        return (name == a.name);
//    }
//    inline int operator != (const Atom &a) const {
//        return !operator==(a);
//    }
};

typedef const Atom * AtomPtr;

inline std::ostream& operator<<(std::ostream& out, const Atom& a) {
    out << a.name;
    return out;
}

#endif
