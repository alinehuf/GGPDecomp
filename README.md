Code d'un joueur GGP.

Joueur non opérationnel en compétition. Le code n'est pas optimisé et plutôt brouillon (beaucoup de tests en cours). 

Programme expérimental pour tester différentes approches pour la décomposition des jeux et son exploitation.

Splitter6.cpp = décomposition des jeux, méthode statistique

Uct*.cpp = differents joueurs UCT pour des jeux à un seul joueur. Un joueur classique et différents joueurs tirant parti de la décomposition.

Attention: la partie joueur (circuit, reasoner, player) n'est pas thread safe, une refonte du programme est necessaire pour implémenter les jeux multijoueurs. L'algorithme UCT sur plusieurs arbres est en cours de développement. Des tests préliminaires sont présentés dans ma thèse.

Voir http://www.alinehuf.fr/doctorat-info/these_alinehuf.pdf pour l'explication des buts recherchés. 

