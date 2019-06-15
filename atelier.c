#include "atelier.h"

void init_boite_aux_lettres()
{
    key_t key; /* valeur de retour de la creation de la clé */

    /****************************************************************/
    /*  Creation de la clé IPC 					*/
    /*  key_t ftok(const char *pathname, int proj_id);              */
    /****************************************************************/
    if ((key = ftok("/tmp", 'a')) == -1)
    {
        fprintf(stderr, "ERREUR de creation de la clé de la boite aux lettres\n");
        exit(1);
    }

    /****************************************************************/
    /* Creation de la file de message 				*/
    /* int msgget(key_t key, int msgflg);				*/
    /****************************************************************/
    if ((msgid = msgget(key, IPC_CREAT | 0600)) == -1)
    {
        fprintf(stderr, "ERREUR de creation de la boite aux lettres\n");
        exit(1);
    }
}

void *homme_flux()
{
    int msg;

    while (1)
    {
        fprintf(stderr, "# HOMME FLUX EN ATTENTE DE MESSAGE\n");
        // Attente d'une carte magnétique d'un atelier
        if ((msg = msgrcv(msgid, &carteCourante, sizeof(struct CarteMagnetique) - sizeof(long), 0, 0)) == -1)
        {
            fprintf(stderr, "HOMME-FLUX : Erreur de lecture message \n");
            exit(1);
        }

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

void *atelier_job(void *arg)
{
    struct ParamAtelier *params = (struct ParamAtelier *)arg;
    sleep(3);

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
        }

        // L'atelier check les composants nécessaires
        // (sauf si c'est l'atelier de base avec ressources infinies)
        if (params->nbRessources > 0)
        {
            checkComposants(params);
        }
        produire(params);

        // On a produit un composant
        printf("======= STATUS atelier %d = %d BEFORE LOCK\n", params->idAtelier, statusAteliers[params->idAtelier]);
        pthread_mutex_lock(&mutexStatus);
        printf("======= STATUS atelier %d = %d AVANT\n", params->idAtelier, statusAteliers[params->idAtelier]);
        statusAteliers[params->idAtelier]--;
        printf("======= STATUS atelier %d = %d APRES\n", params->idAtelier, statusAteliers[params->idAtelier]);
        pthread_mutex_unlock(&mutexStatus);
    }
}

void produire(struct ParamAtelier *params)
{
    // Cherche un conteneur vide
    pthread_mutex_lock(&mutexAireCollecte);
    struct Conteneur contEnProduction;

    // On l'enlève de l'aire de collecte
    aireDeCollecte.nbConteneurVideActuel--;
    memcpy(&aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel], &contEnProduction, sizeof(struct Conteneur));
    // free(&aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel]);

    pthread_mutex_unlock(&mutexAireCollecte);

    // On attache la carte de l'homme-flux
    contEnProduction.cartePresente = 1;
    memcpy(&contEnProduction.carte, &carteCourante, sizeof(struct CarteMagnetique));

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
        pthread_cond_wait(&conditions[params->clients[i]], &mutex[params->clients[i]]);
        // Dévérouille l'accès à l'atelier
        pthread_mutex_unlock(&mutex[params->clients[i]]);
    }
}

// Aller chercher un conteneur dans l'aire de collecte de type typeComposant
// Si pas de conteneur dispo, mise en attente de l'atelier
void prendreConteneurPleinAireDeCollecte(struct ParamAtelier *params, int typeComposant, int indexConteneur)
{
    fprintf(stderr, "## Atelier %s prend un conteneur %d dans l'aire de collecte\n", params->nomAtelier, typeComposant);
    // Tant qu'il n'y a pas de conteneur contenant le matériaux voulu
    while (aireDeCollecte.nbConteneurPleinParAtelier[typeComposant] == 0)
    {
        // Vérouille l'accès à l'atelier
        pthread_mutex_lock(&mutex[params->idAtelier]);
        // Mise en attente de l'atelier
        pthread_cond_wait(&conditions[params->idAtelier], &mutex[params->idAtelier]);

        pthread_mutex_unlock(&mutex[params->idAtelier]);
    }

    // Prendre un nouveau conteneur correspondant a typeComposant
    pthread_mutex_lock(&mutexAireCollecte);

    aireDeCollecte.nbConteneurPleinParAtelier[typeComposant]--;
    int indexConteneurPleinAPrendre = aireDeCollecte.nbConteneurPleinParAtelier[typeComposant];

    // Stocker le conteneur plein dans les conteneurs de l'atelier
    memcpy(&params->conteneur[indexConteneur],
           &aireDeCollecte.conteneursPlein[typeComposant][indexConteneurPleinAPrendre], sizeof(struct Conteneur));

    // Enlever ce conteneur de l'aire de collecte
    // free(&aireDeCollecte.conteneursPlein[typeComposant][indexConteneurPleinAPrendre]);

    pthread_mutex_unlock(&mutexAireCollecte);
}

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

        // On cherche si l'atelier a un conteneur de la ressource necéssaire
        // Si c'est le cas
        // Si il y a assez de composants dans le conteneur
        if (params->conteneur[i].qte > qtyComposant)
        {
            // Prendre les composants nécessaires
            params->conteneur[i].qte -= qtyComposant;

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

            // Stock le conteneur vide de l'atelier dans l'aire de collecte
            pthread_mutex_lock(&mutexAireCollecte);

            memcpy(&aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel], &params->conteneur[i], sizeof(struct Conteneur));
            aireDeCollecte.nbConteneurVideActuel++;

            pthread_mutex_unlock(&mutexAireCollecte);

            // Enlever le conteneur vide de l'atelier
            // free(&params->conteneur[indexConteneur]);

            // Prendre un nouveau conteneur correspondant a typeComposant
            prendreConteneurPleinAireDeCollecte(params, typeComposant, i);

            // Prendre le nombre de composant restant nécessaire à la production
            params->conteneur[i].qte -= qtyRestante;

            // Prévenier l'homme-flux
            envoiCarteMagnetique(&params->conteneur[i]);
        }
    }
}

void envoiCarteMagnetique(struct Conteneur *arg)
{
    if (&arg->cartePresente != 0)
    {
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

void status_factory_short()
{
    for (int i = 0; i < param_factory->nbAteliers; i++)
    {
        status_atelier_short(params_ateliers[i]);
    }
}

void status_factory_full()
{
    for (int i = 0; i < param_factory->nbAteliers; i++)
    {
        status_atelier_full(params_ateliers[i]);
    }
}

void status_aire_de_collecte()
{
    printf("####### AIRE DE COLLECTE #######\n");
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

void init_factory(struct ParamFactory *pf, struct ParamAtelier **pas)
{
    // Configure le signal SIGINT (Ctrl-C) pour nettoyer l'usine
    signal(SIGINT, traitantSIGINT);

    // Stockage des paramètres de tous les ateliers
    params_ateliers = pas;
    // Stockage des paramètres de l'usine
    param_factory = pf;

    // Init tableau des threads : un par atelier
    tid = malloc(param_factory->nbAteliers * sizeof(pthread_t));
    // Init tableau des mutexs pour chaque atelier
    mutex = malloc(param_factory->nbAteliers * sizeof(pthread_mutex_t));
    // Init tableau de conditions : une par atelier
    conditions = malloc(param_factory->nbAteliers * sizeof(pthread_cond_t));
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

void clear_factory()
{
    // Termine les threads ateliers
    // pthread_join()

    // Suppression de l"homme flux

    // Supprime la file de message de l'homme-flux
    msgctl(msgid, IPC_RMID, NULL);
    fprintf(stderr, "## File de message de l'homme-flux %d supprimée\n", msgid);

    // Suppression de toutes les allocations
}

void traitantSIGINT()
{
    clear_factory();
    exit(0);
}
