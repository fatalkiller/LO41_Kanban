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
        perror("Erreur de creation de la clé \n");
        exit(1);
    }

    /****************************************************************/
    /* Creation de la file de message 				*/
    /* int msgget(key_t key, int msgflg);				*/
    /****************************************************************/
    if ((msgid = msgget(key, IPC_CREAT | 0600)) == -1)
    {
        perror("Erreur de creation de la file requete\n");
        exit(1);
    }
}

void *homme_flux(void *arg)
{
    int msg;
    struct CarteMagnetique cm;

    while (1)
    {
        // Attente d'une carte magnétique d'un atelier
        if ((msg = msgrcv(msgid, &cm, sizeof(cm), 0, 0)) == -1)
        {
            perror("HOMME-FLUX : Erreur de lecture message \n");
            exit(1);
        }

        // On demande la production d'un composant dans l'atelier fournisseur
        // de la carte magnétique
        pthread_mutex_lock(&mutexStatus);
        statusAteliers[cm.idAtelierFournisseur]++;
        pthread_mutex_unlock(&mutexStatus);

        // Vérouille l'accès à l'atelier
        pthread_mutex_lock(&mutex[cm.idAtelierFournisseur]);
        // On réveille l'atelier de production fournisseur
        pthread_cond_signal(&conditions[cm.idAtelierFournisseur]);
        pthread_mutex_unlock(&mutex[cm.idAtelierFournisseur]);
    }
}

void *atelier_job(void *arg)
{
    struct ParamAtelier params = *(struct ParamAtelier *)arg;
    while (1)
    {
        // Vérouille l'accès au status de l'atelier
        pthread_mutex_lock(&mutexStatus);
        // Tant que l'atelier doit attendre
        while (statusAteliers[params.idAtelier] == 0)
        {
            pthread_mutex_unlock(&mutexStatus);

            // Vérouille l'accès à l'atelier
            pthread_mutex_lock(&mutex[params.idAtelier]);
            // Mise en attente de l'atelier
            pthread_cond_wait(&conditions[params.idAtelier], &mutex[params.idAtelier]);

            pthread_mutex_unlock(&mutex[params.idAtelier]);
        }

        // L'atelier check les composants nécessaires
        // (sauf si c'est l'atelier de base avec ressources infinies)
        if (params.idAtelier > 0)
        {
            checkComposants(&params);
        }

        // On a produit un composant
        pthread_mutex_lock(&mutexStatus);
        statusAteliers[params.idAtelier]--;
        pthread_mutex_unlock(&mutexStatus);
    }
}

void produire(struct ParamAtelier *params)
{
    // Cherche un conteneur vide
    pthread_mutex_lock(&mutexAireCollecte);
    aireDeCollecte.nbConteneurVideActuel--;
    struct Conteneur *contEnProduction = aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel];
    // On l'enlève de l'aire de collecte
    aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel] = NULL;
    pthread_mutex_unlock(&mutexAireCollecte);

    // On attache la carte de l'homme-flux
    contEnProduction->carte = carteCourante;

    // On attend la production d'un composant
    sleep(params->tpsProd);

    // On remplit le conteneur
    contEnProduction->qte = contEnProduction->carte->qtyMax;

    // On range le conteneur plein sur l'aire de collecte
    pthread_mutex_lock(&mutexAireCollecte);
    int indexDernierConteneur = sizeof(aireDeCollecte.conteneursPlein[params->idAtelier]) - 1;
    aireDeCollecte.conteneursPlein[params->idAtelier][indexDernierConteneur] = contEnProduction;
    pthread_mutex_unlock(&mutexAireCollecte);

    // On reveille les ateliers qui attendent la ressource produite par cet atelier
    for (int i = 0; i < sizeof(params->clients); i++)
    {
        // Vérouille l'accès à l'atelier
        pthread_mutex_lock(&mutex[params->clients[i]]);
        // Mise en attente de l'atelier
        pthread_cond_wait(&conditions[params->clients[i]], &mutex[params->clients[i]]);
        // Dévérouille l'accès à l'atelier
        pthread_mutex_unlock(&mutex[params->clients[i]]);
    }
}

void checkComposants(struct ParamAtelier *params)
{
    for (int i = 0; i < sizeof(params->ressources); i++)
    {
        int typeComposant = params->ressources[i][0];
        int qtyComposant = params->ressources[i][1];
        // Si il y a asses de composant dans le conteneur
        if (params->conteneur[i]->qte > qtyComposant)
        {
            // Prendre les composants nécessaires
            params->conteneur[i]->qte -= qtyComposant;

            // Prévenier l'homme-flux
            envoiCarteMagnetique(params->conteneur[i]);
        }
        else
        {
            // Quantité de composants nécessaires restant
            int qtyRestante = params->conteneur[i]->qte - qtyComposant;
            params->conteneur[i]->qte = 0;

            // Stock le conteneur vide de l'atelier dans l'aire de collecte
            pthread_mutex_lock(&mutexAireCollecte);
            aireDeCollecte.nbConteneurVideActuel++;
            aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel] = params->conteneur[i];
            pthread_mutex_unlock(&mutexAireCollecte);
            params->conteneur[i] = NULL;

            // Tant qu'il n'y a pas de conteneur contenant le matériaux voulu
            while (sizeof(aireDeCollecte.conteneursPlein[typeComposant]) == 0)
            {
                // Vérouille l'accès à l'atelier
                pthread_mutex_lock(&mutex[params->idAtelier]);
                // Mise en attente de l'atelier
                pthread_cond_wait(&conditions[params->idAtelier], &mutex[params->idAtelier]);

                pthread_mutex_unlock(&mutex[params->idAtelier]);
            }

            // Prendre un nouveau conteneur correspondant à l'atelier d'après
            pthread_mutex_lock(&mutexAireCollecte);
            int indexConteneurPleinAPrendre = sizeof(aireDeCollecte.conteneursPlein[typeComposant]) - 1;
            params->conteneur[i] = aireDeCollecte.conteneursPlein[typeComposant][indexConteneurPleinAPrendre];
            // Enlever ce conteneur de l'aire de collecte
            aireDeCollecte.conteneursPlein[typeComposant][indexConteneurPleinAPrendre] = NULL;
            pthread_mutex_unlock(&mutexAireCollecte);

            // Prendre le nombre de composant restant nécessaire à la production
            params->conteneur[i]->qte -= qtyRestante;

            // Prévenier l'homme-flux
            envoiCarteMagnetique(params->conteneur[i]);
        }
    }
}

void envoiCarteMagnetique(struct Conteneur *arg)
{
    if (&arg->carte != NULL)
    {
        // Envoyer la carte magnétique dans la file de message de l'homme flux
        if (msgsnd(msgid, &arg->carte, sizeof(arg->carte), 0) == -1)
        {
            perror("CHECK COMPOSANTS : Erreur envoi requete à l'homme-flux \n");
            exit(1);
        }
        // On efface la carte magnétique du conteneur
        arg->carte = NULL;
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
    printf("Liste des clients :\n");
    for (int i = 0; i < sizeof(pa->clients) / 8; i++)
    {
        printf("  - Atelier %d\n", pa->clients[i]);
    }

    // Affiche les ressources nécessaires à cet atelier
    printf("Ressources nécessaires : \n");
    printf("   _____________\n");
    printf("  | TypeR | Qty |\n");

    for (int i = 0; i < sizeof(pa->ressources); i++)
    {
        printf("  |   %d   |  %d  |\n", pa->ressources[i][0], pa->ressources[i][1]);
    }
    printf("   ____________\n");

    // Affiche la liste des conteneuers actuellement dans l'atelier
    printf("Conteneurs dans l'atelier :\n");
    printf("   _____________\n");
    printf("  | TypeR | Qty |\n");

    for (int i = 0; i < sizeof(pa->conteneur); i++)
    {
        printf("  |   %d   |  %d  |\n", pa->conteneur[i]->carte->idAtelierFournisseur, pa->conteneur[i]->qte);
    }
    printf("   _____________\n");

    printf("##########################\n");
}

void status_atelier_short(struct ParamAtelier *pa)
{
    printf("####### ATELIER %s #######\n", pa->nomAtelier);
    printf("Temps de production : %d sec\n", pa->tpsProd);
    printf("Qty de pièces produites/boucle : %d\n", pa->qtyPieceParConteneur);
    printf("=> Nb pièces à produire : %d\n", statusAteliers[pa->idAtelier]);

    // Affiche la liste des conteneuers actuellement dans l'atelier
    printf("Conteneurs dans l'atelier :\n");
    printf("   _____________\n");
    printf("  | TypeR | Qty |\n");

    for (int i = 0; i < sizeof(pa->conteneur); i++)
    {
        printf("  |   %d   |  %d  |\n", pa->conteneur[i]->carte->idAtelierFournisseur, pa->conteneur[i]->qte);
    }
    printf("   _____________\n");

    printf("##########################\n");
}

void status_factory_short()
{
    for (int i = 0; i < sizeof(params_ateliers); i++)
    {
        status_atelier_short(&params_ateliers[i]);
    }
}

void status_factory_full()
{
    for (int i = 0; i < sizeof(params_ateliers); i++)
    {
        status_atelier_full(&params_ateliers[i]);
    }
}

void init_factory(struct ParamFactory *pf, struct ParamAtelier *pas)
{
    // Stockage des paramètres de tous les ateliers
    params_ateliers = pas;
    // Init tableau des threads : un par atelier
    tid = malloc(pf->nbAteliers * sizeof(pthread_t));
    // Init tableau des mutexs pour chaque atelier
    mutex = malloc(pf->nbAteliers * sizeof(pthread_mutex_t));
    // Init tableau de conditions : une par atelier
    conditions = malloc(pf->nbAteliers * sizeof(pthread_cond_t));
    /*
        Init status des ateliers
    n = Nb d'élements à produire
    0 = En attente
    */
    statusAteliers = malloc(pf->nbAteliers * sizeof(int));

    // Création de la file de message
    // (boite au lettre de l'homme-flux)
    init_boite_aux_lettres();

    // Initialisation aire de collecte
    aireDeCollecte.nbConteneurVideActuel = pf->nbConteneursVide;
    // Initialisation des listes des conteneurs plein
    aireDeCollecte.conteneursPlein = malloc(pf->nbAteliers * sizeof(struct Conteneur **));

    // Initialisation des listes des conteneurs vide
    aireDeCollecte.conteneursVide = malloc(aireDeCollecte.nbConteneurVideActuel * sizeof(struct Conteneur *));

    // Création des conteneurs vides
    for (size_t i = 0; i < aireDeCollecte.nbConteneurVideActuel; i++)
    {
        struct Conteneur c;
        c.carte = NULL;
        c.qte = i;
        aireDeCollecte.conteneursVide[i] = &c;
    }

    // Initialisation des ateliers
    for (size_t i = 0; i < pf->nbAteliers; i++)
    {
        // Assignation du numéro d'atelier
        pas[i].idAtelier = auto_idAtelier;
        // Incrémentation du numéro d'atelier, pour le suivant
        auto_idAtelier++;

        // Création de son aire de conteneur plein
        aireDeCollecte.conteneursPlein[i] = malloc(pf->nbConteneursParClient * sizeof(struct Conteneur *));

        // Création de pf->nbConteneursParClient conteneurs plein pour cet atelier par client
        for (size_t j = 0; j < pf->nbConteneursParClient; j++)
        {
            // Création carte magnétique
            struct CarteMagnetique cm;
            cm.idAtelierFournisseur = pas[i].idAtelier;
            cm.qtyMax = pas[i].qtyPieceParConteneur;
            cm.nomPiece = pas[i].nomAtelier;

            // Création du conteneur
            // Attache de la carte magnétique
            struct Conteneur c;
            c.carte = &cm;
            c.qte = cm.qtyMax;

            // Stockage du conteneur
            aireDeCollecte.conteneursPlein[i][j] = &c;
        }

        // Création du thread atelier
        if (pthread_create(&tid[pas[i].idAtelier], NULL, atelier_job, (void *)&pas[i]) < 0)
        {
            fprintf(stderr, "Erreur création thread atelier %d\n", pas[i].idAtelier);
            exit(1);
        }
    }

    // Création de l'homme-flux
    if (pthread_create(&homme_flux_tid, NULL, homme_flux, NULL) < 0)
    {
        fprintf(stderr, "Erreur création thread homme-flux\n");
        exit(1);
    }
}