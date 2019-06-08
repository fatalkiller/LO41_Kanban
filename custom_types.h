struct CarteMagnetique
{
    int idAtelierDemandeur;   // Indice dans tableau de thread atelier demandeur
    int idAtelierFournisseur; // Indice dans tableau de thread atelier fournisseur
    int qtyMax;               // Contenance max d'un conteneur
    char *nomPiece;           // Nom de la pièce (string)
} ;

struct Conteneur
{
    int qte;                      // Quantité dans le conteneur
    struct CarteMagnetique *carte; // Carte magnétique liée au conteneur
} ;

struct ParamAteliers
{
    int idAtelier;              // Atelier courant
    int tpsProd;                // Temps de production en secondes
    int qtyComposant;           // Quantité de composant amont nécessaires
    struct Conteneur *conteneur; // Conteneur actuellement utilisé dans l'atelier
} ;

struct AireDeCollecte
{
    int nbConteneurVideActuel;          // Nombre de conteneur vide dans la liste conteneursVide
    struct Conteneur **conteneursVide;   // Liste de conteneurs vide
    struct Conteneur ***conteneursPlein; // Liste de conteneurs plein, prêt, par atelier => { atelier1 : listeConteneurPlein1, atelier2: listeConteneursPlein2...}
} ;
