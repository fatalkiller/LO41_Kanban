# LO41_Kanban

> -   Pour compiler le jeu de test :
>     -   gcc kankan.c -pthread -o kanban

> -   Pour compiler (developpeur)
>     -   gcc kanban.c -pthread -Wall -Wextra -Werror

> -   Pour lancer le jeu de test :
>     -   Lancer 2 terminaux : sur le 2eme terminal faire la commande : tty
>     -   Récupérer ce tty du genre "/dev/pts/X"
>     -   Puis lancer le jeu avec la commande suivante : ./kanban [nom_fichier_param] 2>/dev/pts/X
