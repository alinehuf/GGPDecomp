//
//  Splitter6.h
//  playerGGP
//
//  Created by Dexter on 03/03/2017.
//  Copyright © 2017 dex. All rights reserved.
//

#ifndef Splitter6_h
#define Splitter6_h

#include "Circuit.h"

class Splitter6 {
public:
    Splitter6(Circuit& circuit, Propnet& propnet);
    ~Splitter6() {};
    std::unordered_set<TermPtr>& whiteList() { return white_list; }
    std::unordered_set<TermPtr>& blackList() { return black_list; }
    std::vector<std::unordered_set<TermPtr>>& subgamesTrues() { return subgames_trues; }
    std::vector<std::unordered_set<TermPtr>>& subgamesLegals() { return subgames_legals; }
    int subgamesUseless() { return useless_actions; }
    std::vector<std::unordered_set<TermPtr>> subgamesTruesOnly() {
        std::vector<std::unordered_set<TermPtr>> subgames_trues_only;
        for (const auto& sg : subgames_trues)
            if (!sg.empty()) subgames_trues_only.push_back(sg);
        return subgames_trues_only;
    }
private:
    Circuit& circuit;
    const Circuit::Info& c_infos;
    GdlFactory& factory;
    Propnet& propnet;
    size_t nb_inputs;
    size_t nb_bases;
    
    /* SEUILS ET RATIOS */
    int NUM_TEST = 4;
    int NUM_PLAYOUT = 1000;
    int PLAYOUT_DONE = 0;
    float DIFF_RATIO = 1.5;       // Phi   // ratio entre deux effets distincts
    int GLOBAL_LINKED_OCC = 3;    // Psi   // nombre minimum d'observation d'un effet lié global
    int ACTION_LINKED_OCC = 5;    // Theta // nombre minimum d'observation d'un effet lié par action (cf : dual hamilton)
    float THR_LOW_LINK = 0.1;     // Omega // les liens en dessous de ce poids sont considérés faibles (séparation des sous-jeux sous-décomposés)

    /* data */
    // informations collectées une fois pour toute dans les règles
    VectorBool init_values;
    std::vector<std::set<Fact> > does_in_next; // pour chaque action les next(f) dans lesquels elle intervient
                                               // si l'action n'est dans aucun does, elle peut avoir un effet implicite...
                                               // si l'action est dans une négation, le fluent est associé à false
    std::vector<std::set<Fact> > next_with_does; // pour chaque fluent les does qui interviennent dans la règle next
                                                 // si il n'y a aucune action => c'est un fluent indépendant des actions
    std::vector<bool> next_without_does;
    // enrichi mais jamais réinitialisé
    std::map<FactId, std::set<FactId> > fluents_prems; // liens entre les fluents indépendants des actions
    // informations réatualisées à chaque décomposition
    std::vector<bool> action_independent;  // fluents indépendants des actions
    std::vector<bool> control_fluents;
    std::vector<FactId> noop_actions;      // actions sans effet
    bool alternate_moves;                  // a-t-on détecté des coups alternés ? 

    /* fluents utilisés en précondition des legals */
    std::map<Fact, std::map<RoleId, std::set<FactId> > > precond_to_legal; // fluent précondition => role => actions légales

    /* fluents utilisés en conjonction avec les actions dans un NEXT */
    // pour chaque conclusion d'un next, pour chaque role, pour chaque fluent en premisse, l'action qui apparait en conjonction avec le premisse
    // si l'action est utilisée négative n'est la conclusion de la règle qui est négative.
    std::map<Fact, std::map<RoleId, std::map<Fact, std::set<FactId> > > > precond_to_does; // conclusion du next (avec négation de l'action) => role => fluent précondition => actions utilisées en conjonction
    std::map<Fact, std::map<RoleId, std::map<Fact, std::set<FactId> > > > unsafe_precond_to_does; // idem quand l'action est suposée ne pas modifier la conclusion du NEXT


    /* occurences et effets de chaque joint move */
    typedef enum {POS = 1, NEG = 0} efftype;
    struct effects {
        effects(FactId first, FactId last) : first_(first), occ_(0) {
            eff_[POS].resize(last - first, 0);
            eff_[NEG].resize(last - first, 0);
        };
        FactId first_;
        size_t occ_;
        std::array<std::vector<size_t>, 2> eff_; // occurences d'un effet sur chaque fluent (numéroté à partir de 0)
        std::map<Fact, std::set<Fact> > change_together_; // fluent et le groupe de fluents qui changent systématiquement en même temps
        std::map<Fact, int > change_together_occ_; // fluent et le groupe de fluents qui changent systématiquement en même temps
    };
    typedef std::map<std::vector<FactId>, struct effects> jmove_map;
    jmove_map jmoves_effects; // pour chaque joint move (vecteur d'actions, une pour chaque joueur), les effets correspondants.
    
    /* liste d'iterateurs sur les mouvements joints ou une action apparait */
    struct _comp_it {
        bool operator() (const jmove_map::iterator& it1, const jmove_map::iterator& it2) {
            auto id1 = it1->first.begin();
            auto id2 = it2->first.begin();
            while (id1 != it1->first.end()) {
                if (*id1 == *id2) { id1++; id2++; }
                else return (*id1 < *id2);
            }
            return false;
        }
    };
    std::vector<int> in_nb_jmoves; // nombre max de joint-moves dans lequels l'action pourait apparaitre dans une position
    std::vector<std::set<jmove_map::iterator, _comp_it> > in_joint_moves; // joint-moves dans lequels l'action apparait
    std::array<std::vector<std::vector<float> >, 2> actions_efflinks; // pour chaque action, stocke la force du lien [0, 1] vers chaque fluent

    std::vector<std::map<Fact, std::set<Fact> > > change_together_per_action; // pour chaque action, stocke les fluents sur lesquels elle a potentiellement un effet et les effets liés    
    std::vector<std::map<Fact, std::set<Fact> > > change_together_per_action_rev; // map inverse de la précédente
    std::vector<std::map<Fact, int > > change_together_per_action_occ; // occurence observées des changements des fluents

    std::map<Fact, std::set<Fact> > change_together_global; // pour chaque fluents qui devient vrai ou faux : les fluents qui changent en même temps (vers vrai ou faux)
    std::map<Fact, std::set<Fact> > change_together_global_rev; // map inverse de la précédente
    std::map<Fact, int > change_together_global_occ; // occurence observées des changements des fluents
    std::map<Fact, int > change_together_global_step;

    /* jeux en série - simplifé */
    typedef struct {
        std::map<Fact, std::set<Fact> > from_to;    // from_to;     fluent -> action or fluent -> fluent or action -> fluent
        std::map<Fact, std::set<Fact> > to_from;    // to_from;     action <- fluent or fluent <- fluent or fluent <- action
    } Crosspoints_infos;
    std::set<Fact> crosspoint;
    std::map<Fact, std::set<Fact> > from_crosspoint_to;
    std::map<Fact, std::set<Fact> > to_crosspoint_from;
    std::map<Fact, std::set<Fact> > fluent_2_conj_crosspoint; // nouveau fluent vers conjonction de fluents associée

    /* meta-action */
    enum {EFFECTS = 0, NEXT_PRECONDS = 1, LEGAL_PRECONDS = 2, TOTAL_LINKS = 3};
    struct MetaMap {
        MetaMap() : next_free_id_(0) {};
        MetaMap(FactId n) : next_free_id_(n) { meta_links_.resize(TOTAL_LINKS); };
        void addMetaWithLinks(std::set<FactId> group, std::vector<std::set<Fact> >&& links);
        void clear() {
            next_free_id_ = 0;
            actions_to_meta_.clear();
            meta_to_actions_.clear();
            meta_links_.clear();
        }
        FactId next_free_id_;
        std::map<std::set<FactId>, FactId> actions_to_meta_; // set of actions to a Fact which represent it
        std::map<FactId, std::set<FactId>> meta_to_actions_; // Fact which represent a meta-action to the set of actions
        std::vector<std::map<FactId, std::set<Fact> > > meta_links_; // a meta-action with different link types to different fluents
    };
    /* to split meta-action */
    template<typename T>
    struct threeParts {
        std::set<T> ins1;
        std::set<T> both;
        std::set<T> ins2;
    };
    template<typename T>
    threeParts<T> ins1BothIns2(const std::set<T>& s1, const std::set<T>& s2);
    MetaMap meta_actions;
    
    /* subgoals */
    typedef enum {PART_VICTORY=0, VICTORY=1, TERMINAL=2, NB_VIC_TYPE=3} vic_type;
    std::array<std::set<std::set<Fact> >, NB_VIC_TYPE> cond_goalterm_sets; // contenu des conditions de victoire, subgoals et terminal
    /* subgames */ 
    typedef FactId SubgameId;
    SubgameId next_subgame_id = 1;
    // subgame without low weight links
    std::map<FactId, SubgameId> fact_to_subgame_strong; // contains fluent and meta-action ids
    std::map<SubgameId, std::set<FactId> > subgames_strong;
    // subgame with all links
    std::map<FactId, SubgameId> fact_to_subgame; // contains fluent and meta-action ids
    std::map<SubgameId, std::set<FactId> > subgames;
    std::vector<bool> hard_linked;

    // traitement des sous-jeux contenant une seule action
    std::unordered_set<TermPtr> white_list;
    std::unordered_set<TermPtr> black_list;

    // traitement des sous-jeux en général
    std::vector<std::unordered_set<TermPtr>> subgames_trues;
    std::vector<std::unordered_set<TermPtr>> subgames_legals;
    int useless_actions;

    /* methods */
    //VectorTermPtr chooseLeastExplored(std::vector<VectorTermPtr>& legals);
    /* find action independant fluents and links between them */
    void actionDependencies();
    void findAlwaysLegal();
    void actionIndependentPrems();
    void actionIndependentPrems_aux(FactId t_id);
    /* general methods */
    template <typename T>
    static std::set<T> intersect(const std::set<T>& s1, const std::set<T>& s2);

    /* préconditions aux actions dans LEGAL et NEXT */
    void findDoesPrecondInNext();
    void updateDoesPrecondInNext(std::array<std::vector<std::vector<float> >, 2> prev_efflinks);
    void findLegalPrecond();

    /* get and analyse informations about (joint moves) actions */
    void getTransitionInfos(const VectorBool& previous, const VectorBool& values, std::vector<int>& nb_jmoves, int step);
    void analyseTransitionInfos();
    bool hasNoImplicitOrExplicitEffect(FactId d_num, Fact f);
    bool isNotResponsibleOfCooccurentEffect(FactId d_num, Fact f);
    std::pair<std::set<FactId>,std::set<FactId> > actionsWithConfirmedEffectOrNot(Fact f);
    void eraseEffect(FactId d_num, Fact f);
    void eraseCooccurOnly(FactId d_num, Fact f);
    void addEffectsToRecheck(FactId d_num, Fact f, std::map<FactId, std::set<Fact> >& effects_to_check);

    void testEffect(std::map<FactId, std::set<Fact> >& effects_to_check, const std::string& name);


    /* cherche les méta-actions (actions composées) */
    /* JEUX PARALLÈLES */
    void getMetaActionsFromLegalPreconds(Splitter6::MetaMap& metaMapL);
    void getMetaActionsFromNextPreconds(Splitter6::MetaMap& metaMapN);
    void getMetaActionsFromEffects(Splitter6::MetaMap& metaMapE);
    void splitMetaActions(Splitter6::MetaMap& metaMap1, Splitter6::MetaMap& metaMap2, Splitter6::MetaMap& metaMapDef);
    void findMetaActions();

    /* JEUX EN SERIE */
    void findCrossPoint();
    void addConjCrossPoint(const std::set<Fact>& conj, Crosspoints_infos& cross_inf, Crosspoints_infos& cross_inf_all);
    void splitSerialGames();
    
    /* JEUX TROP DECOMPOSÉS */
    void getCondVictory(); // collecte des expressions dans les goal garantissant un score > 0
    std::set<std::set<Fact> > partition_victory_sets(vic_type vic, std::set<SubgameId>& action_dependant, std::map<FactId, SubgameId>& fact_to_subgame);
    void checkWithSubGoals();   // utilisation des expressions pour confirmer les sous-jeux

    /* IDENTIFICATION DES SOUS-JEUX */
    void findSubgames();

    /* Informations sur les sous-jeux */
    void getWhiteAndBlackLists();
    void getSubgamesTruesAndLegals();
    
    /* debug */
    void createGraphvizSerial(const std::string& name, const Crosspoints_infos& infos) const;
    std::string graphvizSerial(const std::string& name, const Crosspoints_infos& infos) const;

    void createGraphvizFile(const std::string& name, bool strong = false) const;
    std::string graphvizReprJmoves(const std::string& name, bool strong) const;
    
    void printTermOrId(FactId id, std::ostream& out);
    void printTermOrId(const Fact& f, std::ostream& out);

    void printActionIndependent(std::ostream& out);
    void printLegalPrecond(std::ostream& out);
    void printDoesPrecondInNext(std::ostream& out);
    void printInNbJmove(std::ostream& out);
    void printActionsEffects(std::ostream& out);
    void printJmoveEffects(std::ostream& out);
    void printInJmoveEffects(std::ostream& out); // effects in different joint moves for each action
    void printActionsLinks(std::ostream& out);
    void printActionEffectOnFluents(std::ostream& out); // for each fluent the effect of each action if different from 0
    void printWhatChangeTogether(std::ostream& out);
    void printMeta2Actions(MetaMap mp, std::string title);
    void printMeta2Links(MetaMap mp, std::string title);
    void printCrosspoints(std::ostream& out);
    void printSubgamesStrong(std::ostream& out);
    void printSubgames(std::ostream& out);
    void printCondVictory(std::ostream& out);
    void printCondTerminal(std::ostream& out);

};

#endif /* Splitter6_h */
