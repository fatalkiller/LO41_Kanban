# LO41_Kanban

#### Projet

> Il faut pouvoir choisir :
>
> -   le nombre d'atelier de productions
>     ex: prod sièges, armature, roues...
>
> -   le nombre de composant pour fabriquer une pièces (= nb de tour de boucle)
>     ex : pour fabriquer du tissus, il me faut 10x fils
>
> -   choisir la profondeur de chaque prod (combien de composants différents nécessaires)

#### Rapport

> -   Doit contenir les étapes de conceptions, pk avoir choisi des threads, des mutex...
> -   Doit contenir un diagramme réseau de petri

### Demande utilisateur :

> -   Nombre de conteneur maximum
> -   Nombre de chaine
>     -   Nombre d'atelier pour cette chaine
>     -   Nombre de composant nécessaire pour chaque atelier
>     -   Temps de production

> Atelier 0 : tout à gauche de la chaine, ..., Atelier n : dernier de la chaine = produit fini

### Tâches à faire

> -   Menu utilisateur
>     -   Création de n ateliers
>     -   Demande du nombre de composants par atelier
>     -   Demande temps de production

> -   Fonction d'affichage des statistiques
