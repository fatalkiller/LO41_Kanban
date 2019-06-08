#include "atelier.h"

void *atelier_job(void *arg)
{
    struct ParamAteliers params = *(struct ParamAteliers*)arg;
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
        if (params.idAtelier > 0) {
            checkComposants(&params);
        }

        // On a produit un composant
        pthread_mutex_lock(&mutexStatus);
        statusAteliers[params.idAtelier]--;
        pthread_mutex_unlock(&mutexStatus);
    }
}

void produire(struct ParamAteliers *params) {
    // Cherche un conteneur vide
    pthread_mutex_lock(&mutexAireCollecte);
    aireDeCollecte.nbConteneurVideActuel--;
    struct Conteneur *contEnProduction = aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel];
    // On l'enlève de l'aire de collecte
    aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel] = NULL;
    pthread_mutex_unlock(&mutexAireCollecte);

    // On attache la carte de l'homme-flux
    contEnProduction->carte = carteCourante;
    // TODO: reveille homme-flux

    // On attend la production d'un composant
    sleep(params->tpsProd);

    // On remplit le conteneur
    contEnProduction->qte = contEnProduction->carte->qtyMax;

    // On range le conteneur plein sur l'aire de collecte
    pthread_mutex_lock(&mutexAireCollecte);    
    int indexDernierConteneur = sizeof(aireDeCollecte.conteneursPlein[params->idAtelier]) - 1;
    aireDeCollecte.conteneursPlein[params->idAtelier][indexDernierConteneur] = contEnProduction;
    pthread_mutex_unlock(&mutexAireCollecte);

    // On reveille l'atelier en aval
        // Vérouille l'accès à l'atelier
    pthread_mutex_lock(&mutex[params->idAtelier + 1]);
        // Mise en attente de l'atelier
    pthread_cond_wait(&conditions[params->idAtelier + 1], &mutex[params->idAtelier + 1]);
        // Dévérouille l'accès à l'atelier
    pthread_mutex_unlock(&mutex[params->idAtelier + 1]);
}

void checkComposants(struct ParamAteliers *params)
{
    // Si la quantité de composants
    if (params->conteneur->qte > params->qtyComposant)
    {
        // Prendre les composants nécessaires
        params->conteneur->qte -= params->qtyComposant;

        // Prévenier l'homme-flux
        envoiCarteMagnetique(params->conteneur);
    }
    else
    {
        // Quantité de composants nécessaires restant
        int qtyRestante = params->conteneur->qte - params->qtyComposant;
        params->conteneur->qte = 0;

        // Stock le conteneur vide de l'atelier dans l'aire de collecte
        pthread_mutex_lock(&mutexAireCollecte);        
        aireDeCollecte.nbConteneurVideActuel++;
        aireDeCollecte.conteneursVide[aireDeCollecte.nbConteneurVideActuel] = params->conteneur;
        pthread_mutex_unlock(&mutexAireCollecte);
        params->conteneur = NULL;

        // Tant qu'il n'y a pas de conteneur contenant le matériaux voulu
        while (sizeof(aireDeCollecte.conteneursPlein[params->idAtelier]) == 0)
        {
            // Vérouille l'accès à l'atelier
            pthread_mutex_lock(&mutex[params->idAtelier]);
            // Mise en attente de l'atelier
            pthread_cond_wait(&conditions[params->idAtelier], &mutex[params->idAtelier]);

            pthread_mutex_unlock(&mutex[params->idAtelier]);
        }

        // Prendre un nouveau conteneur correspondant à l'atelier d'après
        pthread_mutex_lock(&mutexAireCollecte);                
        int indexConteneurPleinAPrendre = sizeof(aireDeCollecte.conteneursPlein[params->idAtelier - 1]) - 1;
        params->conteneur = aireDeCollecte.conteneursPlein[params->idAtelier - 1][indexConteneurPleinAPrendre];
        // Enlever ce conteneur de l'aire de collecte
        aireDeCollecte.conteneursPlein[params->idAtelier - 1][indexConteneurPleinAPrendre] = NULL;
        pthread_mutex_unlock(&mutexAireCollecte);

        // Prendre le nombre de composant restant nécessaire à la production
        params->conteneur->qte -= qtyRestante;

        // Prévenier l'homme-flux
        envoiCarteMagnetique(params->conteneur);
    }
}

void envoiCarteMagnetique(struct Conteneur *arg)
{
    if (&arg->carte != NULL)
    {
        // TODO: Envoyer la carte magnétique dans la file de message de l'homme flux
        // arg->carte
        // arg->carte = NULL
        // TODO: Réveiller l'homme-flux
    }
}

// if (pthread_create(&thr, &thread_attr, fonction_thread, NULL) < 0)
// {
//     fprintf(stderr, "pthread_create error for thread 1\n");
//     exit(1);
// }

int main() {
    return 0;
}