# LO41_Kanban

> -   Pour compiler le jeu de test :
>     -   gcc test.c -pthread -o jeu_de_test

> -   Pour compiler (developpeur)
>     -   gcc test.c -pthread -Wall -Wextra -Werror

> -   Pour lancer le jeu de test :
>     -   Lancer 2 terminaux : sur le 2eme terminal faire la commande : tty
>     -   Récupérer ce tty du genre "/dev/pts/X"
>     -   Puis lancer le jeu avec la commande suivante : ./jeu_de_test 2>/dev/pts/X
