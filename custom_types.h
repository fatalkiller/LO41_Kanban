// Carte magnétique
struct CarteMagnetique
{
    long type;                // type pour l'envoi du message dans la file de message (= 1 ici)
    int idAtelierFournisseur; // Indice dans tableau de thread atelier fournisseur
    int qtyMax;               // Contenance max d'un conteneur
    char *nomPiece;           // Nom de la pièce (string)
};

// Conteneur
struct Conteneur
{
    int qte;                       // Quantité dans le conteneur
    struct CarteMagnetique *carte; // Carte magnétique liée au conteneur
};

// Paramètres des ateliers
struct ParamAtelier
{
    int idAtelier;            // Atelier courant
    char *nomAtelier;         // Nom de la pièce produite par cet atelier (string)
    int tpsProd;              // Temps de production en secondes
    int qtyPieceParConteneur; // Quantité de composant dans un conteneur de ce type de pièce
    int nbRessources;         // Nombre de ressources (taille de "ressources")
    int **ressources;         // Tableau contenant les types et quantités nécessaires de chaques ressources
                              // avec ressources[i][0] => Type de ressource
                              //      ressources[i][1] => Quantité de cette ressource
    int nbClients;            // Nombre de clients (taille de "clients")
    int *clients;
    int nbConteneurs;             // Listes des id ateliers clients
    struct Conteneur **conteneur; // Conteneur actuellement utilisé dans l'atelier
};

// Aire de collecte des conteneurs
struct AireDeCollecte
{
    int nbConteneurVideActuel;           // Nombre de conteneur vide dans la liste conteneursVide
    struct Conteneur **conteneursVide;   // Liste de conteneurs vide
    struct Conteneur ***conteneursPlein; // Liste de conteneurs plein, prêt, par atelier => { atelier1 : listeConteneurPlein1, atelier2: listeConteneursPlein2...}
};

struct ParamFactory
{
    int nbAteliers;            // Nombre d'ateliers
    int nbConteneursVide;      // Nombre de conteneurs vides dans l'air de collecte
    int nbConteneursParClient; // Nombre de conteneurs plein par client dans l'aire de collecte
};