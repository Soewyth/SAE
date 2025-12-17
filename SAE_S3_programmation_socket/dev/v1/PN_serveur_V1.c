#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour read, write, close, sleep */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>        /* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h>    /* pour htons et inet_aton */
#include <ctype.h>        /* pour tolower */

/* ================== DEFINE  ================== */
#define PORT 5000        //(ports >= 5000 réservés pour usage explicite)
#define LG_MESSAGE 256    // buffer qui gère les messages
#define NB_ESSAIS_MAX 6 // NOmbres d'essais maximum pour deviner le mot

/* ================== VAr globale ================== */
const char *DEVINER_MOT = "TEST";

/* ================== MAIN ================== */

int main(int argc, char *argv[])
{
    int socketEcoute;
    struct sockaddr_in pointDeRencontreLocal; // Adresse + port d’écoute du serveur
    socklen_t longueurAdresse;                  // stocker la taille de l’adresse du socket
    struct sockaddr_in pointDeRencontreDistant; // Adresse + port du client

    int socketJ1, socketJ2; // AJOUT : Sockets des deux joueurs

    char messageRecu[LG_MESSAGE];
    char messageEnvoye[LG_MESSAGE];

    int ecritsj1, ecritsj2, lus; /* nb d’octets ecrits et lus */

    // Crée un socket de communication
    socketEcoute = socket(AF_INET, SOCK_STREAM, 0); // Domaine Internet (IPV4), type de socket (TCPIP)
    // Teste la valeur renvoyée par l’appel système socket()
    if (socketEcoute < 0)
    {
        perror("socket"); // Affiche le message d’erreur
        exit(-1);          // On sort en indiquant un code erreur
    }
    printf("Socket créée avec succès ! (%d)\n", socketEcoute); // On prépare l’adresse d’attachement locale

    // Remplissage de sockaddrDistant (structure sockaddr_in identifiant le point d'écoute local)
    longueurAdresse = sizeof(pointDeRencontreLocal); // on stock la taille de la structure sockaddr_in

    memset(&pointDeRencontreLocal, 0x00, longueurAdresse);       // memset met à zero la structure
    pointDeRencontreLocal.sin_family = AF_INET;                   // Domaine Internet
    pointDeRencontreLocal.sin_addr.s_addr = htonl(INADDR_ANY); // attaché à toutes les interfaces locales disponibles
    pointDeRencontreLocal.sin_port = htons(PORT);               // port = 5000 ou plus

    // On demande l’attachement local de la socket, pour dire a la socket sur quel port écouter
    if ((bind(socketEcoute, (struct sockaddr *)&pointDeRencontreLocal, longueurAdresse)) < 0)
    {
        perror("bind");
        exit(-2);
    }
    printf("Socket attachée avec succès !\n");

    // On fixe la taille de la file d’attente à 5 (pour les demandes de connexion non encore traitées)
    // On transforme la socket en socket d’écoute
    if (listen(socketEcoute, 5) < 0)
    {
        perror("listen");
        exit(-3);
    }
    printf("Socket placée en écoute passive ...\n");

    // boucle d’attente de connexion : en théorie, un serveur attend indéfiniment !
    while (1)
    {
        printf("Attente d’une demande de connexion (quitter avec Ctrl-C)\n\n");

        // c’est un appel bloquant
        socketJ1 = accept(socketEcoute,(struct sockaddr *)&pointDeRencontreDistant, &longueurAdresse);
        if (socketJ1 < 0)
        {
            perror("accept");
            exit(-4);
        }
        printf("Joueur 1 connecté !\n");

        printf("Connexion acceptee d’un client ! (socket dialogue : %d)\n", socketJ1);
        printf("En attente joueur 2\n");
        socketJ2 = accept(socketEcoute, (struct sockaddr *)&pointDeRencontreDistant, &longueurAdresse);
        if (socketJ2 < 0)
        {
            perror("accept");
            close(socketJ1);
            continue;
        }
        printf("Connexion acceptee d’un client ! (socket dialogue : %d)\n", socketJ2);

        /* *
         * GESTION PARTIE PENDU
         *
         */

        // déclaration variables partie pendu
        int length_mot = strlen(DEVINER_MOT); // longueur du mot à deviner
        int essais_j1 = NB_ESSAIS_MAX;  // nombre d'essais restants joueur 1
        int essais_j2 = NB_ESSAIS_MAX;  // nombre d'essais restants joueur 2
        char masque_mot[LG_MESSAGE];          // mot masqué envoyé au client
        int lettres_essayees[26] = {0};          // init tableau 26 cases à 0

        // INitialisation du mot masqué
        for (int i = 0; i < length_mot; i++)
        {
            masque_mot[i] = '_'; // initialisation du mot masqué avec des underscores
        }
        masque_mot[length_mot] = '\0'; // terminer la chaine de caractères

        // Envoie du message start
        memset(messageEnvoye, 0x00, LG_MESSAGE);
        // int snprintf(char *buffer, taille_max, const char *format, taille_mot, masque_mot);
        // FOnction sécurisé car il vérifie la taille max du buffer (lgmessage)
        snprintf(messageEnvoye, LG_MESSAGE, "START %d %s\n", length_mot, masque_mot);
        ecritsj1 = send(socketJ1, messageEnvoye, strlen(messageEnvoye), 0);
        ecritsj2 = send(socketJ2, messageEnvoye, strlen(messageEnvoye), 0);
        
        // Début du jeu : c'est au joueur 1 de jouer
        usleep(100000); // Pause pour éviter le collage des paquets
        memset(messageEnvoye, 0x00, LG_MESSAGE);
        snprintf(messageEnvoye, LG_MESSAGE, "TOUR A VOUS\n");
        ecritsj1 = send(socketJ1, messageEnvoye, strlen(messageEnvoye), 0);

        // Gestion erreur envoi message start
        if (ecritsj1 <= 0)
        {
            if (ecritsj1 == 0)
                fprintf(stderr, "La socket J1 a été fermée par le client !\n\n");
            else
                perror("Erreur du message vers J1");
            close(socketJ1);
            close(socketJ2);
            continue;
        }
        printf("[MESSAGE ENVOYE] vers J1 : %s (%d octets)\n\n", messageEnvoye, ecritsj1);

        if (ecritsj2 <= 0)
        {
            if (ecritsj2 == 0)
                fprintf(stderr, "La socket J2 a été fermée par le client !\n\n");
            else
                perror("Erreur du message vers J2");
            close(socketJ1);
            close(socketJ2);
            continue;
        }
        printf("[MESSAGE ENVOYE] vers J2 : %s (%d octets)\n\n", messageEnvoye, ecritsj2);
        
        // Boucle du jeu pendu
        int partie_terminee = 0;
        int tour = 1; // 1 = joueur 1, 2 = joueur 2

        while (!partie_terminee)
        {
        int socketActif;
        int socketPassif;
        int *essaisActif;
        // Sélection du joueur actif et passif
        if (tour == 1)
        {
            socketActif = socketJ1;
            socketPassif = socketJ2;
            essaisActif = &essais_j1;
        }
        else
        {
            socketActif = socketJ2;
            socketPassif = socketJ1;
            essaisActif = &essais_j2;
        }

        // Si le joueur actif n'a plus d'essais → passer au suivant
        if (*essaisActif <= 0)
        {
            // Si les deux n'ont plus d'essais = partie perdue
            if (essais_j1 <= 0 && essais_j2 <= 0)
            {
                memset(messageEnvoye, 0, LG_MESSAGE);
                snprintf(messageEnvoye, LG_MESSAGE, "DEFAITE - Le mot etait %s\n", DEVINER_MOT);
                send(socketJ1, messageEnvoye, strlen(messageEnvoye), 0);
                send(socketJ2, messageEnvoye, strlen(messageEnvoye), 0);
                partie_terminee = 1;
                break;
            }
            // Sinon on change simplement de joueur
            if (tour == 1)
                tour = 2;
            else
                tour = 1;    
            //Pour celui qui survie
            int socketSurvivant;
            int essaisSurvivant;
            if (tour == 1) {
                socketSurvivant = socketJ1;
                essaisSurvivant = essais_j1;
            } else {
                socketSurvivant = socketJ2;
                essaisSurvivant = essais_j2;
            }
            memset(messageEnvoye, 0, LG_MESSAGE);
            // On prévient le survivant qu'il doit rejouer
            snprintf(messageEnvoye, LG_MESSAGE, "L'autre joueur est elimine. TOUR A VOUS. %d essais restants. %s\n", essaisSurvivant, masque_mot);
            send(socketSurvivant, messageEnvoye, strlen(messageEnvoye), 0);    
            continue; // On recommence la boucle immédiatement
            }

            // Attente lettre joueur
            memset(messageRecu, 0, LG_MESSAGE);
            lus = recv(socketActif, messageRecu, LG_MESSAGE - 1, 0);

            if (lus <= 0)
            {
                perror("recv");
                partie_terminee = 1;
                break;
            }

            messageRecu[lus] = '\0';

            char proposition = toupper(messageRecu[0]);

            // Vérification caractère valide
            if (proposition < 'A' || proposition > 'Z')
            {
                // Ajout de TOUR A VOUS pour débloquer le client
                snprintf(messageEnvoye, LG_MESSAGE, "Lettre invalide. Entrez A-Z \nTOUR A VOUS\n");
                send(socketActif, messageEnvoye, strlen(messageEnvoye), 0);
                continue;
            }

            // Lettre déjà tentée ?
            int indexTableau = proposition - 'A';
            if (lettres_essayees[indexTableau] == 1)
            {
                // Ajout de TOUR A VOUS pour débloquer le client
                snprintf(messageEnvoye, LG_MESSAGE, "Lettre deja proposee\nTOUR A VOUS\n");
                send(socketActif, messageEnvoye, strlen(messageEnvoye), 0);
                continue;
            }

            lettres_essayees[indexTableau] = 1;

            // Vérification dans le mot
            int bonne_lettre = 0;
            for (int i = 0; i < length_mot; i++)
            {
                if (DEVINER_MOT[i] == proposition)
                {
                    masque_mot[i] = proposition;
                    bonne_lettre = 1;
                }
            }

            if (!bonne_lettre)
            {
                (*essaisActif)--;
            }

            // Victoire ?
            if (strcmp(DEVINER_MOT, masque_mot) == 0)
            {
                snprintf(messageEnvoye, LG_MESSAGE, "VICTOIRE du joueur %d - Le mot etait %s\n", tour, DEVINER_MOT);
                send(socketActif, messageEnvoye, strlen(messageEnvoye), 0);
                send(socketPassif, messageEnvoye, strlen(messageEnvoye), 0);
                partie_terminee = 1;
                break;
            }
        // Défaite joueur actif (pendant son tour) ?
        if (*essaisActif <= 0)
        {
            snprintf(messageEnvoye, LG_MESSAGE, "DEFAITE - 0 essais\n");
            send(socketActif, messageEnvoye, strlen(messageEnvoye), 0);
            // Si l’autre joueur peut encore jouer → continuer
            if ((tour == 1 && essais_j2 > 0) || (tour == 2 && essais_j1 > 0))
            {
                // Ajout de TOUR A VOUS pour le survivant
                snprintf(messageEnvoye, LG_MESSAGE, "%s\nTOUR A VOUS\n", masque_mot);
                send(socketPassif, messageEnvoye, strlen(messageEnvoye), 0);
                // Changement de tour
                if (tour == 1)
                    tour = 2;
                else
                    tour = 1;
                continue;
            }
            else
            {
                // Les deux ont perdu
                snprintf(messageEnvoye, LG_MESSAGE, "DEFAITE \n");
                send(socketJ1, messageEnvoye, strlen(messageEnvoye), 0);
                send(socketJ2, messageEnvoye, strlen(messageEnvoye), 0);

                partie_terminee = 1;
                break;
            }
        }
        // Informer joueur actif de la mise à jour
        snprintf(messageEnvoye, LG_MESSAGE, "TOUR TERMINE. %d essais restants. %s\n", *essaisActif, masque_mot);
        send(socketActif, messageEnvoye, strlen(messageEnvoye), 0);
        // Passer au joueur suivant
        if (tour == 1)
            tour = 2;
        else
            tour = 1;
        // Informer le nouveau joueur actif
        int socketNouveauActif;
        int essaisNouveauActif;
        if (tour == 1)
        {
            socketNouveauActif = socketJ1;
            essaisNouveauActif = essais_j1;
        }
        else
        {
            socketNouveauActif = socketJ2;
            essaisNouveauActif = essais_j2;
        }

        // Message standard de changement de tour
        snprintf(messageEnvoye, LG_MESSAGE, "TOUR A VOUS. %d essais restants. %s\n", essaisNouveauActif, masque_mot);
        send(socketNouveauActif, messageEnvoye, strlen(messageEnvoye), 0);
    }
        
    // Fin de la partie : fermer les sockets des joueurs et continuer le serveur
    close(socketJ1);
    close(socketJ2);
    printf("Partie terminee, sockets fermees. Reprise attente connexion...\n\n");
    }

    close(socketEcoute);
    return 0;
}