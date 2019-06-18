#include "atelier.h"

/*
    Création de la file de message 
    Correspond à la boite aux letttres de l'homme-flux
 */
void init_boite_aux_lettres()
{
    key_t key;

    // Création de la clé
    if ((key = ftok("/tmp", 'a')) == -1)
    {
        fprintf(stderr, "ERREUR de creation de la clé de la boite aux lettres\n");
        exit(1);
    }

    // Création file de message
    if ((msgid = msgget(key, IPC_CREAT | 0600)) == -1)
    {
        fprintf(stderr, "ERREUR de creation de la boite aux lettres\n");
        exit(1);
    }
}

/*
    Fonction executée par le thread de l'homme-flux
 */
void *homme_flux()
{
    // Code de retour de la reception de message
    int msg;
    struct CarteMagnetique carteCourante;

    while (1)
    {
        fprintf(stderr, "# HOMME-FLUX : En attente de message\n");
        // Attente d'une carte magnétique d'un atelier
        if ((msg = msgrcv(msgid, &carteCourante, sizeof(struct CarteMagnetique) - sizeof(long), 0, 0)) == -1)
        {
            fprintf(stderr, "HOMME-FLUX : Erreur de lecture message \n");
            exit(1);
        }
        memcpy(&cartesCourantes[carteCourante.idAtelierFournisseur], &carteCourante, sizeof(struct CarteMagnetique));

        // on vérifie que l'atelier ne soit pas déjà en production
        pthread_mutex_lock(&mutexStatus);
        while (statusAteliers[carteCourante.idAtelierFournisseur] == 1)
        {
            pthread_mutex_unlock(&mutexStatus);
            pthread_mutex_lock(&mutex_homme_flux);
            pthread_cond_wait(&condition_homme_flux, &mutex_homme_flux);
            pthread_mutex_unlock(&mutex_homme_flux);
        }
        pthread_mutex_unlock(&mutexStatus);

        // On demande la production d'un composant dans l'atelier fournisseur
        // de la carte magnétique
        pthread_mutex_lock(&mutexStatus);
        statusAteliers[carteCourante.idAtelierFournisseur]++;
        pthread_mutex_unlock(&mutexStatus);

        // Vérouille l'accès à l'atelier
        pthread_mutex_lock(&mutex[carteCourante.idAtelierFournisseur]);
        // On réveille l'atelier de production fournisseur
        pthread_cond_signal(&conditions[carteCourante.idAtelierFournisseur]);
        pthread_mutex_unlock(&mutex[carteCourante.idAtelierFournisseur]);
    }
}

/*
    Fonction executée par le client 
    qui veut passer des commandes de pièces
 */
void *client_job(void *arg)
{
    struct ParamAtelier *params = (struct ParamAtelier *)arg;

    key_t key;

    // Création de la clé
    if ((key = ftok("/tmp", 'b')) == -1)
    {
        fprintf(stderr, "ERREUR de creation de la clé de la file de message client\n");
        exit(1);
    }

    // Création file de message
    if ((msgid_client = msgget(key, IPC_CREAT | 0600)) == -1)
    {
        fprintf(stderr, "ERREUR de creation de la file de message client\n");
        exit(1);
    }

    int msg;
    struct CommandeClient cc;
    while (1)
    {
        if ((msg = msgrcv(msgid_client, &cc, sizeof(struct CommandeClient) - sizeof(long), 0, 0)) == -1)
        {
            fprintf(stderr, "CLIENT : Erreur de lecture message \n");
            exit(1);
        }
        params->ressources[0][1] = cc.qtyCmd;

        fprintf(stderr, "== Client envoie une commande de type %d, qty %d\n", params->ressources[0][0], params->ressources[0][1]);
        checkComposants(params);
        fprintf(stderr, "\n\n====== Client a bien reçu %d composants =======\n\n\n", params->ressources[0][1]);
    }
}

/*
    Fonction executée par les threads ateliers
    - Se mets en attente de commande
    - Dès la reception d'un ordre de commande de l'homme-flux, 
      l'atelier se mets à produire
      Il vérifie l'accessibilité des ressources dont il a besoin 
      puis produit
    void *arg : ParamAtelier : structure des paramètres de l'atelier
 */
void *atelier_job(void *arg)
{
    struct ParamAtelier *params = (struct ParamAtelier *)arg;
    sleep(1);

    while (1)
    {
        // Vérouille l'accès au status de l'atelier
        pthread_mutex_lock(&mutexStatus);
        // Tant que l'atelier doit attendre
        while (statusAteliers[params->idAtelier] == 0)
        {
            fprintf(stderr, "\n## Atelier %d en attente d'une commande\n", params->idAtelier);
            pthread_mutex_unlock(&mutexStatus);

            // Vérouille l'accès à l'atelier
            pthread_mutex_lock(&mutex[params->idAtelier]);
            // Mise en attente de l'atelier
            pthread_cond_wait(&conditions[params->idAtelier], &mutex[params->idAtelier]);

            pthread_mutex_unlock(&mutex[params->idAtelier]);
            pthread_mutex_lock(&mutexStatus);
        }
        pthread_mutex_unlock(&mutexStatus);

        // L'atelier check les composants nécessaires
        // (sauf si c'est l'atelier de base avec ressources infinies)
        if (params->nbRessources > 0)
        {
            checkComposants(params);
        }
        produire(params);
        // On prévient l'homme flux qu'on a terminé la production (pour le cas où il a reçu une carte de cette atelier avant la fin de sa)
        pthread_mutex_lock(&mutex_homme_flux);
        pthread_cond_signal(&condition_homme_flux);
        pthread_mutex_unlock(&mutex_homme_flux);
        // On a produit un composant
        pthread_mutex_lock(&mutexStatus);
        statusAteliers[params->idAtelier]--;
        pthread_mutex_unlock(&mutexStatus);
    }
}

/*
    Fonction de production 
    struct ParamAtelier *params : structure des paramètres de l'atelier
 */
void produire(struct ParamAtelier *params)
{
    // Cherche un conteneur vide
    struct Conteneur contEnProduction = prendreConteneurVideAireDeCollecte(params);

    // On attache la carte de l'homme-flux
    contEnProduction.cartePresente = 1;
    memcpy(&contEnProduction.carte, &cartesCourantes[params->idAtelier], sizeof(struct CarteMagnetique));

    // On attend la production d'un composant
    fprintf(stderr, "## PRODUCTION de l'Atelier %s pour %d sec\n", params->nomAtelier, params->tpsProd);

    sleep(params->tpsProd);

    fprintf(stderr, "## FIN PRODUCTION de l'Atelier %s\n", params->nomAtelier);

    // On remplit le conteneur
    contEnProduction.qte = contEnProduction.carte.qtyMax;

    // On range le conteneur plein sur l'aire de collecte
    pthread_mutex_lock(&mutexAireCollecte);

    memcpy(&aireDeCollecte.conteneursPlein[params->idAtelier][aireDeCollecte.nbConteneurPleinParAtelier[params->idAtelier] - 1], &contEnProduction, sizeof(struct Conteneur));
    aireDeCollecte.nbConteneurPleinParAtelier[params->idAtelier]++;

    pthread_mutex_unlock(&mutexAireCollecte);

    // On reveille les ateliers qui attendent la ressource produite par cet atelier
    for (int i = 0; i < params->nbClients; i++)
    {
        // Vérouille l'accès à l'atelier
        pthread_mutex_lock(&mutex[params->clients[i]]);
        // Mise en attente de l'atelier
        pthread_cond_signal(&conditions[params->clients[i]]);
        // Dévérouille l'accès à l'atelier
        pthread_mutex_unlock(&mutex[params->clients[i]]);
    }

    // On révéille le client si c'est l'atelier a qui on a commandé
    if (params->nbClients == 0)
    {
        // Vérouille l'accès à l'atelier
        pthread_mutex_lock(&mutex[param_factory->nbAteliers]);
        // Mise en attente de l'atelier
        pthread_cond_signal(&conditions[param_factory->nbAteliers]);
        // Dévérouille l'accès à l'atelier
        pthread_mutex_unlock(&mutex[param_factory->nbAteliers]);
    }
}

/*
    Cherche un conteneur dans l'aire de collecte
    Si pas de conteneur dispo, mise en attente de l'atelier
      struct ParamAtelier *params : structure des paramètres de l'atelier
      int typeComposant : Type de composant du conteneur à chercher
      int indexConteneur : Index dans lequel placé le conteneur dans la liste des conteneurs de l'atelier
*/
void prendreConteneurPleinAireDeCollecte(struct ParamAtelier *params, int typeComposant, int indexConteneur)
{
    // Tant qu'il n'y a pas de conteneur contenant le matériaux voulu
    while (aireDeCollecte.nbConteneurPleinParAtelier[typeComposant] == 0)
    {
        // Vérouille l'accès à l'atelier
        pthread_mutex_lock(&mutex[params->idAtelier]);
        // Mise en attente de l'atelier
        pthread_cond_wait(&conditions[params->idAtelier], &mutex[params->idAtelier]);

        pthread_mutex_unlock(&mutex[params->idAtelier]);
    }

    fprintf(stderr, "## Atelier %s prend un conteneur %d dans l'aire de collecte\n", params->nomAtelier, typeComposant);
    // Prendre un nouveau conteneur correspondant a typeComposant
    pthread_mutex_lock(&mutexAireCollecte);

    aireDeCollecte.nbConteneurPleinParAtelier[typeComposant]--;
    int indexConteneurPleinAPrendre = aireDeCollecte.nbConteneurPleinParAtelier[typeComposant];

    // Stocker le conteneur plein dans les conteneurs de l'atelier
    memcpy(&params->conteneur[indexConteneur],
           &aireDeCollecte.conteneursPlein[typeComposant][indexConteneurPleinAPrendre], sizeof(struct Conteneur));

    pthread_mutex_unlock(&mutexAireCollecte);
}

/*
    Cherche un conteneur vide dans l'aire de collecte
    Si pas de conteneur dispo, mise en attente de l'atelier
      struct ParamAtelier *params : structure des paramètres de l'atelier
*/
struct Conteneur prendreConteneurVideAireDeCollecte(struct ParamAtelier *params)
{
    // Cherche un conteneur vide
    struct Conteneur contVide;
    pthread_mutex_lock(&mutexAireCollecte);

    // Vérifier si un conteneur vide est dispo
    while (aireDeCollecte.nbConteneurVideActuel == 0)
    {
        fprintf(stderr, "## Atelier %s se met en attente d'un conteneur vide\n", params->nomAtelier);
        // Mise en attente de l'atelier d'un conteneur vide
        pthread_mutex_unlock(&mutexAireCollecte);

        // Vérouille l'accès à l'atelier
        pthread_mutex_lock(&mutex[params->idAtelier]);
        // Mise en attente de l'atelier
        pthread_cond_wait(&conditions[params->idAtelier], &mutex[params->idAtelier]);

        pthread_mutex_unlock(&mutex[params->idAtelier]);
    }

    // On l'enlève de l'aire de collecte
    aireDeCollecte.nbConteneurVideActuel--;
    memcpy(&contVide, &aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel], sizeof(struct Conteneur));

    pthread_mutex_unlock(&mutexAireCollecte);

    return contVide;
}

/*
    - Vérifie les ressources demandées par l'atelier en amont
    - Cherche les conteneurs plein de composants dans l'aire de collecte si besoin
    - Déduis les ressources necéssaires des conteneurs de l'atelier
        struct ParamAtelier *params : structure des paramètres de l'atelier
 */
void checkComposants(struct ParamAtelier *params)
{
    // Si l'atelier n'a pas encore de conteneurs plein (juste après l'initialisation de ceux-ci)
    // Alors récupérer un conteneur de chaque ressource
    if (params->initConteneurs == 0)
    {

        // Pour chaque ressource nécessaire à la production
        for (int i = 0; i < params->nbRessources; i++)
        {
            int typeComposant = params->ressources[i][0];
            // Prendre un nouveau conteneur correspondant a typeComposant
            prendreConteneurPleinAireDeCollecte(params, typeComposant, i);
        }
        params->initConteneurs = 1;
    }
    // Pour chaque ressource nécessaire à la production
    for (int i = 0; i < params->nbRessources; i++)
    {
        int typeComposant = params->ressources[i][0];
        int qtyComposant = params->ressources[i][1];

        // Tant qu'on a pas atteint le nombre de ressource voulu
        while (qtyComposant > 0)
        {
            // On cherche si l'atelier a un conteneur de la ressource necéssaire
            // Si c'est le cas
            // Si il y a assez de composants dans le conteneur
            if (params->conteneur[i].qte > qtyComposant)
            {
                // Prendre les composants nécessaires
                params->conteneur[i].qte -= qtyComposant;

                qtyComposant = 0;

                // Prévenier l'homme-flux
                fprintf(stderr, "# Envoi carte magnetique de l'atelier %d\n", params->idAtelier);
                envoiCarteMagnetique(&params->conteneur[i]);
            }
            // Si il n'y en a pas assez, on prend qd meme le reste
            // contenu dans le conteneur, et on déplace le conteneur
            // dans l'aire de collecte des conteneurs vide
            else
            {
                // On prends le reste des composants du conteneur
                // Quantité de composants nécessaires restant
                int qtyRestante = qtyComposant - params->conteneur[i].qte;
                params->conteneur[i].qte = 0;

                // Prévenier l'homme-flux
                fprintf(stderr, "# Envoi carte magnetique de l'atelier %d\n", params->idAtelier);
                envoiCarteMagnetique(&params->conteneur[i]);

                // Stock le conteneur vide de l'atelier dans l'aire de collecte
                pthread_mutex_lock(&mutexAireCollecte);

                memcpy(&aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel], &params->conteneur[i], sizeof(struct Conteneur));
                aireDeCollecte.nbConteneurVideActuel++;

                pthread_mutex_unlock(&mutexAireCollecte);

                // Réveille tous les ateliers potentiellement en attente de conteneur vide
                for (int i = 0; i < param_factory->nbAteliers; i++)
                {
                    // Vérouille l'accès à l'atelier
                    pthread_mutex_lock(&mutex[params_ateliers[i]->idAtelier]);
                    // Mise en attente de l'atelier
                    pthread_cond_signal(&conditions[params_ateliers[i]->idAtelier]);

                    pthread_mutex_unlock(&mutex[params_ateliers[i]->idAtelier]);
                }

                // Enlever le conteneur vide de l'atelier

                // Prendre un nouveau conteneur correspondant a typeComposant
                prendreConteneurPleinAireDeCollecte(params, typeComposant, i);

                qtyComposant = qtyRestante - params->conteneur[i].qte;
                // Prendre le nombre de composant restant nécessaire à la production
                params->conteneur[i].qte -= qtyRestante;
                // Prévenier l'homme-flux
                envoiCarteMagnetique(&params->conteneur[i]);
            }
        }
    }
}

/*
    Détache la carte du conteneur passé en paramètre
    puis l'envoi à l'homme-flux dans sa boite aux lettres
      struct Conteneur *arg : Pointeur vers un conteneur
 */
void envoiCarteMagnetique(struct Conteneur *arg)
{
    if (arg->cartePresente != 0)
    {
        fprintf(stderr, "# Envoi carte magnetique de l'atelier %d\n", arg->carte.idAtelierFournisseur);
        // Modifie le type du message envoyé (doit être différent de 0)
        arg->carte.type = 1;
        // Envoyer la carte magnétique dans la file de message de l'homme flux
        if (msgsnd(msgid, &arg->carte, sizeof(arg->carte) - sizeof(long), 0) == -1)
        {
            fprintf(stderr, "CHECK COMPOSANTS : Erreur envoi requete à l'homme-flux \n");
            exit(1);
        }

        // On efface la carte magnétique du conteneur
        arg->cartePresente = 0;
        // free(&arg->carte);
    }
}

/*
    Affiche le statut étendu de l'atelier passé en paramètre
        struct ParamAtelier *params : structure des paramètres de l'atelier
 */
void status_atelier_full(struct ParamAtelier *pa)
{
    printf("####### ATELIER %s #######\n", pa->nomAtelier);
    printf("Id de l'atelier : %d\n", pa->idAtelier);
    printf("Temps de production : %d sec\n", pa->tpsProd);
    printf("Qty de pièces produites/boucle : %d\n", pa->qtyPieceParConteneur);
    printf("=> Nb pièces à produire : %d\n", statusAteliers[pa->idAtelier]);

    // Affiche la liste des ateliers clients de cet atelier
    if (pa->nbClients > 0)
    {
        printf("Liste des clients :\n");
        for (int i = 0; i < pa->nbClients; i++)
        {
            printf("  - Atelier %d\n", pa->clients[i]);
        }
    }

    if (pa->nbRessources > 0)
    {
        // Affiche les ressources nécessaires à cet atelier
        printf("Ressources nécessaires : \n");
        printf("   TypeR | Qty \n");

        for (int i = 0; i < pa->nbRessources; i++)
        {
            printf("     %d   | %d  \n", pa->ressources[i][0], pa->ressources[i][1]);
        }
    }
    if (pa->initConteneurs > 0)
    {
        // Affiche la liste des conteneuers actuellement dans l'atelier
        printf("Conteneurs dans l'atelier :\n");
        printf("   TypeR | Qty \n");

        for (int i = 0; i < pa->nbRessources; i++)
        {
            printf("     %d   |  %d  \n", pa->conteneur[i].carte.idAtelierFournisseur, pa->conteneur[i].qte);
        }
    }

    printf("##########################\n");
}

/*
    Affiche le statut réduit de l'atelier passé en paramètre
        struct ParamAtelier *params : structure des paramètres de l'atelier
 */
void status_atelier_short(struct ParamAtelier *pa)
{
    printf("####### ATELIER %s #######\n", pa->nomAtelier);
    printf("Id de l'atelier : %d\n", pa->idAtelier);
    printf("Temps de production : %d sec\n", pa->tpsProd);
    printf("Qty de pièces produites/boucle : %d\n", pa->qtyPieceParConteneur);
    printf("=> Nb pièces à produire : %d\n", statusAteliers[pa->idAtelier]);

    if (pa->initConteneurs > 0)
    {
        // Affiche la liste des conteneuers actuellement dans l'atelier
        printf("Conteneurs dans l'atelier :\n");
        printf("   TypeR | Qty \n");
        for (int i = 0; i < pa->nbRessources; i++)
        {
            printf("     %d   |  %d  \n", pa->conteneur[i].carte.idAtelierFournisseur, pa->conteneur[i].qte);
        }
    }
    printf("##########################\n");
}

/*
    Affiche le statut réduit de l'usine
    (statut de tous les ateliers + aire de collecte)
 */
void status_factory_short()
{
    for (int i = 0; i < param_factory->nbAteliers; i++)
    {
        status_atelier_short(params_ateliers[i]);
    }
    status_aire_de_collecte();
}

/*
    Affiche le statut étendu de l'usine
    (statut de tous les ateliers + aire de collecte)
 */
void status_factory_full()
{
    for (int i = 0; i < param_factory->nbAteliers; i++)
    {
        status_atelier_full(params_ateliers[i]);
    }
    status_aire_de_collecte();
}

void status_client(struct ParamAtelier *client)
{
    printf("### Client ###\n");
    printf("Conteneurs dispo client :\n");
    printf("   TypeR | Qty \n");
    printf("     %d  |  %d\n", client->conteneur->carte.idAtelierFournisseur, client->conteneur->qte);
}

/*
    Affiche le statut de l'aire de collecte 
    en affichant les conteneurs plein de tous les ateliers
 */
void status_aire_de_collecte()
{
    printf("####### AIRE DE COLLECTE #######\n");
    printf("Nombre de conteneurs vide : %d\n", aireDeCollecte.nbConteneurVideActuel);
    for (int i = 0; i < param_factory->nbAteliers; i++)
    {
        printf("# Atelier %d\n", i);
        for (int j = 0; j < aireDeCollecte.nbConteneurPleinParAtelier[i]; j++)
        {
            printf("  Conteneur %d : ", j);
            printf("%s, %d", aireDeCollecte.conteneursPlein[i][j].carte.nomPiece, aireDeCollecte.conteneursPlein[i][j].qte);
            printf("\n");
        }
    }
    printf("################################\n");
}

/*
    Initialisation de l'usine
      struct ParamFactory *pf : Pointeur vers les paramètres de l'usine
      struct ParamAtelier **pas : Tableau de paramètres des différents ateliers

    - Création des mutex
    - Création des conditions
    - Création des status des ateliers
    - Création de la boite aux lettres
    - Création de l'aire de collecte (conteneurs vide + conteneurs pleins)
    - Création des threads atelier
    - Création de l'homme-flux
 */
void init_factory(struct ParamFactory *pf, struct ParamAtelier **pas)
{
    // Configure le signal SIGINT (Ctrl-C) pour nettoyer l'usine
    signal(SIGTSTP, traitantSIGINT);

    // Stockage des paramètres de tous les ateliers
    params_ateliers = pas;
    // Stockage des paramètres de l'usine
    param_factory = pf;

    // Init tableau des threads : un par atelier
    tid = malloc(param_factory->nbAteliers * sizeof(pthread_t));
    // Init tableau des mutexs pour chaque atelier
    mutex = malloc((param_factory->nbAteliers + 1) * sizeof(pthread_mutex_t));
    // Init tableau de conditions : une par atelier
    conditions = malloc((param_factory->nbAteliers + 1) * sizeof(pthread_cond_t));
    /*
        Init status des ateliers
        n = Nb d'élements à produire
        0 = En attente
    */
    statusAteliers = malloc(param_factory->nbAteliers * sizeof(int));

    // Création de la file de message
    // (boite au lettre de l'homme-flux)
    init_boite_aux_lettres();

    // Initialisation aire de collecte
    aireDeCollecte.nbConteneurVideActuel = param_factory->nbConteneursVide;
    // Initialisation des listes des conteneurs plein
    aireDeCollecte.conteneursPlein = malloc(param_factory->nbAteliers * sizeof(struct Conteneur *));
    // et des nombres de conteneurs plein
    aireDeCollecte.nbConteneurPleinParAtelier = malloc(param_factory->nbAteliers * sizeof(int));

    // Initialisation des listes des conteneurs vide
    aireDeCollecte.conteneursVide = malloc(aireDeCollecte.nbConteneurVideActuel * sizeof(struct Conteneur));

    // Initialisation des listes des cartes magnétiques
    cartesCourantes = malloc(param_factory->nbAteliers * sizeof(struct CarteMagnetique));

    // Création des conteneurs vides
    for (int i = 0; i < aireDeCollecte.nbConteneurVideActuel; i++)
    {
        struct Conteneur c;
        c.cartePresente = 0;
        c.qte = 0;
        memcpy(&aireDeCollecte.conteneursVide[i], &c, sizeof(struct Conteneur));
    }

    // Initialisation des ateliers
    for (int i = 0; i < param_factory->nbAteliers; i++)
    {
        // Assignation du numéro d'atelier
        params_ateliers[i]->idAtelier = auto_idAtelier;
        // Incrémentation du numéro d'atelier, pour le suivant
        auto_idAtelier++;
        fprintf(stderr, "# Initialisation ATELIER %d...", params_ateliers[i]->idAtelier);

        if (params_ateliers[i]->nbRessources > 0)
        {
            // Création du tableaux de conteneurs de l'atelier
            params_ateliers[i]->conteneur = malloc(params_ateliers[i]->nbRessources * sizeof(struct Conteneur));
        }
        // Mise à zéro du nombre de conteneur
        // car les ateliers sont initialisés sans conteneur
        params_ateliers[i]->initConteneurs = 0;

        // Création de son aire de conteneur plein
        aireDeCollecte.conteneursPlein[i] = malloc(param_factory->nbConteneursParClient * sizeof(struct Conteneur));
        // Nombre de conteneur plein pour chaque atelier = nbConteneursParClient
        aireDeCollecte.nbConteneurPleinParAtelier[i] = param_factory->nbConteneursParClient;

        // Création de param_factory->nbConteneursParClient conteneurs plein pour cet atelier par client
        for (int j = 0; j < param_factory->nbConteneursParClient; j++)
        {
            // Création carte magnétique
            struct CarteMagnetique cm;
            cm.idAtelierFournisseur = params_ateliers[i]->idAtelier;
            cm.qtyMax = params_ateliers[i]->qtyPieceParConteneur;
            cm.nomPiece = malloc(sizeof(params_ateliers[i]->nomAtelier) * sizeof(char));
            strcpy(cm.nomPiece, params_ateliers[i]->nomAtelier);
            // Création du conteneur
            // Attache de la carte magnétique
            struct Conteneur c;
            c.cartePresente = 1;
            memcpy(&c.carte, &cm, sizeof(struct CarteMagnetique));
            c.qte = c.carte.qtyMax;

            // Stockage du conteneur
            memcpy(&aireDeCollecte.conteneursPlein[i][j], &c, sizeof(struct Conteneur));
        }
        fprintf(stderr, "Ok\n");

        fprintf(stderr, "  Création du THREAD.........");
        // Création du thread atelier
        if (pthread_create(&tid[params_ateliers[i]->idAtelier], NULL, atelier_job, (void *)params_ateliers[i]) < 0)
        {
            fprintf(stderr, "ERREUR création thread atelier %d\n", params_ateliers[i]->idAtelier);
            exit(1);
        }
        fprintf(stderr, "Ok\n");
    }

    // Création de l'homme-flux
    if (pthread_create(&homme_flux_tid, NULL, homme_flux, NULL) < 0)
    {
        fprintf(stderr, "ERREUR création thread homme-flux\n");
        exit(1);
    }
}

/*
    Nettoyage de l'usine 
    - Free toutes les zones mémoires allouées
    - Suppression du tableau de thread
    - Suppression de l'aire de collecte
    - Suppression de la file de message (boite aux lettres)
 */
void clear_factory()
{
    // Supprime la file de message de l'homme-flux

    pthread_cancel(homme_flux_tid);
    pthread_cancel(client_tid);
    msgctl(msgid, IPC_RMID, NULL);
    msgctl(msgid_client, IPC_RMID, NULL);
    fprintf(stderr, "## File de message de l'homme-flux %d supprimée\n", msgid);
    fprintf(stderr, "## File de message de client %d supprimée\n", msgid_client);

    // Suppression de toutes les allocations
    fprintf(stderr, "# Libération de toutes les ressources allouées...");

    for (int i = 0; i < param_factory->nbAteliers; i++)
    {
        free(params_ateliers[i]->conteneur);
        free(aireDeCollecte.conteneursPlein[i]);
    }

    free(aireDeCollecte.conteneursPlein);
    free(aireDeCollecte.nbConteneurPleinParAtelier);

    free(cartesCourantes);
    free(params_ateliers);

    free(tid);
    free(mutex);
    free(conditions);
    free(statusAteliers);

    fprintf(stderr, "Ok\n");
}

/*
    Appel de la fonction de nettoyage mémoire de l'usine 
    à la reception du signal de Ctrl+c
 */
void traitantSIGINT()
{
    clear_factory();
    exit(0);
}
