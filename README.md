# LO41_Kanban

> -   Pour compiler le jeu de test 
>     ```gcc kankan.c -pthread -o kanban```

> -   Pour compiler (developpeur) ```gcc kanban.c -pthread -Wall -Wextra -Werror```

> - Pour créer le fichier de données : ```touch nom_fichier``` puis ```vim nom_fichier```
> - Les données doivent être de la forme :
> 
> 	```c
> 	Nombre_d'atelier Nombre_de_conteneur_par_client Nombre_de_conteneur_vide
> 	Nom_atelier_1 temps_de_production quantité_de_piece_par_conteneur nombre_de_fournisseur nom_fournisseur_1 nom_fournisseur_2
> 	// ATTENTION, il ne peut y avoir qu'un seul atelier qui n'a pas de client, se sera l'atelier qui reçoit les commandes
> 	```
> 	```
> 	Exemple :
> 	2 2 2
> 	Atelier1 2 3 1 Atelier2 3
> 	Atelier2 2 2 0
> 	```


> -   Pour lancer le jeu de test :
>     -   Lancer 2 terminaux : sur le 2eme terminal faire la commande : tty
>     -   Récupérer ce tty qui ressemblera à "/dev/pts/X"
>     -   Puis lancer le jeu avec la commande suivante : ./kanban [nom_fichier_param] 2>/dev/pts/X
