#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/ipc.h>

#include "custom_types.h"

FILE *status;
FILE *messages;

int auto_idAtelier = 0; // ID atelier => auto incrémenté à la création d'un atelier

pthread_t *tid;           // Tableau des threads : un par atelier
pthread_t homme_flux_tid; // Id thread homme-flux

pthread_mutex_t *mutex; // Tableau des mutexs pour chaque atelier
pthread_mutex_t mutexStatus;
pthread_mutex_t mutexAireCollecte;

pthread_cond_t *conditions; // Tableau de conditions : une par atelier

/*
    n = Nb d'élements à produire
    0 = En attente
 */
int *statusAteliers;

int msgid; // Id de la file de message de l'homme-flux

// Aire de collecte (où se trouve les conteneurs plein et vide)
struct AireDeCollecte aireDeCollecte;

struct CarteMagnetique carteCourante;

// Liste des paramètres atelier
struct ParamAtelier **params_ateliers;
struct ParamFactory *param_factory;

void init_boite_aux_lettres();
void *homme_flux();
void *atelier_job(void *);
void client_job(struct ParamAtelier params);
void produire(struct ParamAtelier *);
void checkComposants(struct ParamAtelier *);
void prendreConteneurPleinAireDeCollecte(struct ParamAtelier *params, int typeComposant, int indexConteneur);
void envoiCarteMagnetique(struct Conteneur *);
void init_factory(struct ParamFactory *, struct ParamAtelier **);

void traitantSIGINT();
void clear_factory();

void status_atelier_full(struct ParamAtelier *);
void status_atelier_short(struct ParamAtelier *);
void status_factory_short();
void status_factory_full();
void status_aire_de_collecte();
