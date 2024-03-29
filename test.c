#include "atelier.c"
#include <string.h>

int main()
{
    // Déclaration de l'usine
    struct ParamFactory mon_usine;
    mon_usine.nbAteliers = 4;
    mon_usine.nbConteneursVide = 8;
    mon_usine.nbConteneursParClient = 2;

    // Déclaration des paramètres des ateliers
    // Liste des paramètres des ateliers
    struct ParamAtelier **pas = malloc(mon_usine.nbAteliers * sizeof(struct ParamAtelier *));

    // Atelier 0 => ressources infinies
    struct ParamAtelier pa0;
    // pa0.idAtelier = 0;
    char *str0 = "Fibre";
    pa0.nomAtelier = malloc(sizeof(str0) * sizeof(char));
    strcpy(pa0.nomAtelier, str0);
    pa0.tpsProd = 5;
    pa0.qtyPieceParConteneur = 40;
    pa0.nbRessources = 0;
    pa0.nbClients = 1;
    pa0.clients = malloc(pa0.nbClients * sizeof(int));
    pa0.clients[0] = 1;
    // pa0.nbConteneurs = 0;
    pas[0] = &pa0;

    // Atelier 1
    struct ParamAtelier pa1;
    // pa1.idAtelier = 1;
    pa1.nomAtelier = malloc(sizeof("FibreColor") * sizeof(char));
    strcpy(pa1.nomAtelier, "FibreColor");
    pa1.tpsProd = 10;
    pa1.qtyPieceParConteneur = 30;
    pa1.nbRessources = 1;
    pa1.ressources = malloc(pa1.nbRessources * sizeof(int *));
    pa1.ressources[0] = malloc(2 * sizeof(int));
    pa1.ressources[0][0] = 0;  // Type de ressource
    pa1.ressources[0][1] = 30; // Quantité de ressource
    pa1.nbClients = 1;
    pa1.clients = malloc(pa1.nbClients * sizeof(int));
    pa1.clients[0] = 2;
    // pa1.nbConteneurs = mon_usine.nbConteneursParClient;
    pas[1] = &pa1;

    // Atelier 2
    struct ParamAtelier pa2;
    // pa2.idAtelier = 2;
    pa2.nomAtelier = malloc(sizeof("Fil") * sizeof(char));
    strcpy(pa2.nomAtelier, "Fil");
    pa2.tpsProd = 7;
    pa2.qtyPieceParConteneur = 20;
    pa2.nbRessources = 1;
    pa2.ressources = malloc(pa2.nbRessources * sizeof(int *));
    pa2.ressources[0] = malloc(2 * sizeof(int));
    pa2.ressources[0][0] = 1;  // Type de ressource
    pa2.ressources[0][1] = 20; // Quantité de ressource
    pa2.nbClients = 1;
    pa2.clients = malloc(pa2.nbClients * sizeof(int));
    pa2.clients[0] = 3;
    // pa2.nbConteneurs = mon_usine.nbConteneursParClient;
    pas[2] = &pa2;

    // Atelier 3
    struct ParamAtelier pa3;
    // pa3.idAtelier = 3;
    pa3.nomAtelier = malloc(sizeof("Tissus") * sizeof(char));
    strcpy(pa3.nomAtelier, "Tissus");
    pa3.tpsProd = 12;
    pa3.qtyPieceParConteneur = 15;
    pa3.nbRessources = 1;
    pa3.ressources = malloc(pa3.nbRessources * sizeof(int *));
    pa3.ressources[0] = malloc(2 * sizeof(int));
    pa3.ressources[0][0] = 2;  // Type de ressource
    pa3.ressources[0][1] = 15; // Quantité de ressource
    pa3.nbClients = 1;
    pa3.clients = malloc(pa3.nbClients * sizeof(int));
    pa3.clients[0] = mon_usine.nbAteliers; // id de l'atelier client
    // pa3.nbConteneurs = mon_usine.nbConteneursParClient;
    pas[3] = &pa3;

    fprintf(stderr, "########## Init param ateliers des tests OK ###############\n");

    init_factory(&mon_usine, pas);

    fprintf(stderr, "########## Init factory OK##################################\n");

    status_factory_full();

    // Attendre fin de l'initialisation
    sleep(5);

    // Lancement d'une commande d'un composant
    int idAtelierCommande = 3;
    int qtyCommande = 5;

    struct ParamAtelier paClient;
    paClient.nomAtelier = malloc(sizeof("Client") * sizeof(char));
    strcpy(paClient.nomAtelier, "Client");
    paClient.initConteneurs = 0;
    paClient.nbRessources = 1;
    paClient.nbClients = 0;
    paClient.ressources = malloc(pa3.nbRessources * sizeof(int *));
    paClient.ressources[0] = malloc(2 * sizeof(int));
    paClient.ressources[0][0] = idAtelierCommande; // Type de ressource
    paClient.ressources[0][1] = qtyCommande;       // Quantité de ressource
    paClient.conteneur = malloc(paClient.nbRessources * sizeof(struct Conteneur));

    client_job(paClient);

    while (1)
    {
        system("clear");
        status_factory_short();
        sleep(1);
    }

    for (int i = 0; i < mon_usine.nbAteliers; i++)
    {
        pthread_join(tid[i], NULL);
    }
    pthread_join(homme_flux_tid, NULL);

    // Affiche le conteneur du client
    // printf("==== CONTENEUR CLIENT : Type | Qty\n");
    // printf("                         %s |  %d", pa3.nomAtelier, paClient.conteneur[0].qte);

    return 0;
}