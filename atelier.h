#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "custom_types.h"

#define nbAtelier 10 //Nombre d'ateliers (nb de poste de conion)

pthread_t tid[nbAtelier]; // Tableau des threads : un par atelier
pthread_t homme_flux_tid; // Id thread homme-flux

pthread_mutex_t mutex[nbAtelier]; // Tableau des mutexs pour chaque atelier
pthread_mutex_t mutexStatus;
pthread_mutex_t mutexAireCollecte;

pthread_cond_t conditions[nbAtelier]; // Tableau de conditions : une par atelier

/*
    n = Nb d'élements à produire
    0 = En attente
 */
int statusAteliers[nbAtelier];

int msgid; // Id de la file de message de l'homme-flux

// Aire de collecte (où se trouve les conteneurs plein et vide)
struct AireDeCollecte aireDeCollecte;

struct CarteMagnetique *carteCourante;

void *atelier_job(void *arg);
void checkComposants(struct ParamAtelier *params);
void envoiCarteMagnetique(struct Conteneur *arg);