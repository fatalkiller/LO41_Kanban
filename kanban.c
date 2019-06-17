#include "custom_types.h"
#include "atelier.c"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main() {

    /// --- Début de l'initialisation ---

    // On ouvre le fichier de donnée
    FILE *f=NULL;
    f=fopen("donnees.txt","r");

    // Récupération des information de la factory
    struct ParamFactory factory;
    fscanf(f, "%d %d %d", &factory.nbAteliers, &factory.nbConteneursParClient, &factory.nbConteneursVide);

    char **ateliers = malloc(factory.nbAteliers * sizeof(char*));
    char ***fournisseurs = malloc(factory.nbAteliers * sizeof(char**));
    struct ParamAtelier *params = malloc(factory.nbAteliers * sizeof(struct ParamAtelier));

    // Récupération des informations de chaques ateliers
    for (size_t i = 0; i < factory.nbAteliers; i++)
    {
        params[i].idAtelier = i;
        params[i].nomAtelier = malloc(30 * sizeof(char));
        fscanf(f, "%s %d %d %d", params[i].nomAtelier, &params[i].tpsProd, &params[i].qtyPieceParConteneur, &params[i].nbRessources);
        // printf("idAtelier : %d, nomAtelier : %s, tpsProd : %d, qtyPieceParConteneur : %d, nbRessources : %d\n", params[i].idAtelier, params[i].nomAtelier, 
        // params[i].tpsProd, params[i].qtyPieceParConteneur, params[i].nbRessources);

        // Récupération des noms des fournisseurs
        if (params[i].nbRessources > 0) 
        {  
            fournisseurs[i] = malloc(params[i].nbRessources * sizeof(char*));
            params[i].ressources = malloc(params[i].nbRessources * sizeof(int[2]));
            // printf("fournisseurs :");
        }
        for (size_t j = 0; j < params[i].nbRessources; j++)
        {
            params[i].ressources[j] = malloc(2 * sizeof(int));
            fournisseurs[i][j] = malloc(30 * sizeof(char));
            fscanf(f, "%s %d", fournisseurs[i][j], &params[i].ressources[j][1]);
            // printf(" %s, %d", fournisseurs[i][j], params[i].ressources[j][1]);
        }
        if (params[i].nbRessources > 0) printf("\n");
        
    }

    for(int i = 0; i < factory.nbAteliers; i++) 
    {
        params[i].nbClients = 0;
        params[i].initConteneurs = 0;
    }

    // On cherche les id des fournisseurs afin de les stocker dans les paramètres
    for(int i = 0; i < factory.nbAteliers; i++) 
    {
        for (int j = 0; j < params[i].nbRessources; j++) 
        {
            for (int k = 0; k < factory.nbAteliers; k++)
            {
                if (strcmp(params[k].nomAtelier, fournisseurs[i][j])==0)
                {
                    params[i].ressources[j][0] = k;
                    params[k].nbClients++;
                }
            }
        }
    }

    // Initialisation de la liste de clients et recherche des id des clients
    for (int i = 0; i < factory.nbAteliers; i++)
    {
        int indexClient = 0;
        params[i].clients = malloc(params[i].nbClients * sizeof(int));
        for (int j = 0; j < factory.nbAteliers; j++)
        {
            for(int k = 0; k < params[j].nbRessources; k++)
            {
                if (params[j].ressources[k][0] == i) {
                    params[i].clients[indexClient] = j;
                    indexClient++;
                }
            }
        }   
    }


    // L'atelier qui n'a pas de client est considéré comme celui en bout de file (celui à qui l'utilisateur demande des éléments directement)
    int idAtelierCommande = 0;
    for (int i = 0; i < factory.nbAteliers; i++)
    {
        if (params[i].nbClients == 0) {
            idAtelierCommande = i;
        }

        // affichage des paramètres des ateliers
        printf("idAtelier : %d, nomAtelier : %s, tpsProd : %d, qtyPieceParConteneur : %d, nbRessources : %d, nbClients : %d \n", params[i].idAtelier, params[i].nomAtelier, params[i].tpsProd, params[i].qtyPieceParConteneur, params[i].nbRessources, params[i].nbClients);
        
        for (int j = 0; j < params[i].nbRessources; j++) 
        {
            printf("Num Fournisseur : %d, quantité de nécessaire : %d \n",params[i].ressources[j][0], params[i].ressources[j][1]);
        }

        for (int j = 0; j < params[i].nbClients; j++) 
        {
            printf("Num Client %d \n",params[i].clients[j]);
        }
        printf("---------------------\n");
    }

    // initialisation de la factory
    init_factory(&mon_usine, pas);

    fprintf(stderr, "########## Init factory OK##################################\n");

    // affichage de la factory
    status_factory_full();
    status_aire_de_collecte();

    
    // Attendre fin de l'initialisation
    sleep(5);


    // Création du client (se comporte comme un atelier sans vraiment en être un)
    struct ParamAtelier paClient;
    paClient.nomAtelier = malloc(sizeof("Client") * sizeof(char));
    strcpy(paClient.nomAtelier, "Client");
    paClient.initConteneurs = 0;
    paClient.nbRessources = 1;
    paClient.nbClients = 0;
    paClient.ressources = malloc(sizeof(int *));
    paClient.ressources[0] = malloc(2 * sizeof(int));
    paClient.ressources[0][0] = idAtelierCommande; // Type de ressource
    paClient.ressources[0][1] = 0;       // Quantité de ressource
    paClient.conteneur = malloc(paClient.nbRessources * sizeof(struct Conteneur));

    /// --- Fin de l'initialisation ---



    /// --- Boucle principale du programme permettant à l'utilisateur de faire certaines actions ---

    int choix = 0;
    int quantiteComposantVoulut = 0;
    while(1)
    {
        printf("Veuillez saisir une action parmit les suivantes :  \n  1 -> Demande du composant principal \n  2 -> Affichage de la factory \n  3 -> Arréter la factory \n");
        scanf("%d", &choix);
        if (choix == 1)
        {
            printf("Veuillez saisir la quantité de composant voulut : \n");
            scanf("%d", &quantiteComposantVoulut);
            paClient.ressources[0][1] = quantiteComposantVoulut;
            client_job(paClient);
            printf("Le client a bien reçut %d composants \n", quantiteComposantVoulut);
        } else if (choix == 2)
        {
            system("clear");
            status_factory_short();
            status_aire_de_collecte();
        } else if (choix == 3)
        {
            clear_factory();
            
            for (int i = 0; i < mon_usine.nbAteliers; i++)
            {
                pthread_join(tid[i], NULL);
            }
            pthread_join(homme_flux_tid, NULL);
            
            return 0;
        }
    }
}