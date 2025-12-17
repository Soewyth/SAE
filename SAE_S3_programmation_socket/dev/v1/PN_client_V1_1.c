#include <stdio.h>
#include <stdlib.h>     /* pour exit */
#include <unistd.h>     /* pour read, write, close, sleep */
#include <sys/types.h>  /* pour les types systèmes */
#include <sys/socket.h> /* pour les fonctions de socket */
#include <string.h>     /* pour memset, strlen, strncpy */
#include <netinet/in.h> /* pour struct sockaddr_in (adresses réseau) */
#include <arpa/inet.h>  /* pour htons et inet_aton (conversion d'adresses) */
#include <ctype.h>      /* pour toupper et isalpha (très utile pour les lettres) */
#include <strings.h>    /* pour strcasecmp (comparaison de chaînes sans casse) */

#define LG_MESSAGE 256
#define MAX_WORD_LENGTH 20 // On augmente un peu la taille pour le Pendu

int main(int argc, char *argv[])
{
    // --- Déclarations des variables réseau et de base ---
    int descripteurSocket;
    struct sockaddr_in sockaddrDistant;
    socklen_t longueurAdresse;

    char messageReponse[LG_MESSAGE]; // Buffer pour la réponse que nous recevons du serveur
    char lettre_choisie[LG_MESSAGE]; // Buffer pour la lettre proposée par l'utilisateur
    int nb;                          /* Nombre d’octets écrits ou lus */

    char ip_dest[16]; // Adresse IP du serveur
    int port_dest;    // Port du serveur

    // --- Variables d'état du jeu (le client doit les gérer pour l'affichage) ---
    char mot_affiche[MAX_WORD_LENGTH]; // Le mot avec les tirets et les lettres trouvées (ex: T__T)
    int longueur_mot = 0;              // La longueur du mot (définie par la ligne de commande)
    int partie_en_cours = 0;           // Vaut 1 si on joue, 0 si c'est fini

    char status_mot[20];     // Pour lire le début du message du serveur (ex: "start", "victoire")
    int essais_restants = 0; // Nombre d'essais restants (information donnée par le serveur)

    // Le client a besoin de l'IP, du port et de la longueur du mot à deviner.
    if (argc > 2) // On vérifie si on a 3 arguments après le nom du programme (argv[0])
    {
        strncpy(ip_dest, argv[1], 16);     // Récupération de l'adresse IP (argv[1])
        sscanf(argv[2], "%d", &port_dest); // Récupération du port (argv[2])
    }
    else
    {
        // Message pour expliquer comment lancer le programme
        printf("USAGE : %s <ip> <port>\n", argv[0]);
        exit(-1);
    }

    // socket
    descripteurSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (descripteurSocket < 0)
    {
        perror("Erreur en création de la socket...");
        exit(-1);
    }
    printf("Socket créée! (%d)\n", descripteurSocket);

    // Remplissage de sockaddrDistant (structure sockaddr_in identifiant la machine distante)
    // Obtient la longueur en octets de la structure sockaddr_in
    longueurAdresse = sizeof(sockaddrDistant);       // on stock la taille de la structure sockaddr_in
    memset(&sockaddrDistant, 0x00, longueurAdresse); // memset met à zero la structure
    sockaddrDistant.sin_family = AF_INET;            // Domaine Internet
    sockaddrDistant.sin_port = htons(port_dest);     // Numéro de port du serveur
    inet_aton(ip_dest, &sockaddrDistant.sin_addr);   // Conversion de l’adresse IP

    // Débute la connexion vers le processus serveur distant
    if ((connect(descripteurSocket, (struct sockaddr *)&sockaddrDistant, longueurAdresse)) == -1)
    {
        perror("Erreur de connection avec le serveur distant...");
        close(descripteurSocket);
        exit(-2); // On sort en indiquant un code erreur
    }
    printf("Connexion au serveur %s:%d réussie!\n", ip_dest, port_dest);

    // Réception du message de démarrage du serveur
    memset(messageReponse, 0x00, LG_MESSAGE);
    nb = recv(descripteurSocket, messageReponse, LG_MESSAGE - 1, 0);

    if (nb < 0)
    {
        perror("Erreur de réception du message de démarrage...");
        close(descripteurSocket);
        exit(-3);
    }
    if (nb == 0)
    {
        fprintf(stderr, "Le serveur a coupé la connexion avant le début du jeu.\n");
        close(descripteurSocket);
        exit(-4);
    }
    messageReponse[nb] = '\0';
    printf("[MESSAGE RECU] : %s\n", messageReponse);

    // Le serveur doit répondre : "start [longueur] [masque]" (ex: "start 4 ____")
    if (sscanf(messageReponse, "%s %d %s", status_mot, &longueur_mot, mot_affiche) == 3)
    {

        partie_en_cours = 1;
        printf("Mot initial : %s\n", mot_affiche);
    }
    else
    {
        fprintf(stderr, "Erreur: Signal de demarrage incorrect reçu du serveur. (%s)\n", messageReponse);
        partie_en_cours = 0;
    }

    // Boucle du jeu
    while (partie_en_cours != 0)
    {
        memset(messageReponse, 0x00, LG_MESSAGE);
        nb = recv(descripteurSocket, messageReponse, LG_MESSAGE - 1, 0);

        if (nb <= 0)
        {
            printf("Connexion perdue.\n");
            break;
        }

        messageReponse[nb] = '\0';
        printf("[MESSAGE RECU] : %s\n", messageReponse);

        // Victoire / Défaite ?
        if (strcasestr(messageReponse, "VICTOIRE") != NULL ||
            strcasestr(messageReponse, "DEFAITE") != NULL)
        {
            partie_en_cours = 0;
            continue;
        }

        // Si ce n'est PAS notre tour → continuer à écouter
        if (strcasestr(messageReponse, "TOUR A VOUS") == NULL)
            continue;

        // Ici : c'est notre tour !
        printf("Proposez une lettre : ");
        if (fgets(lettre_choisie, sizeof(lettre_choisie), stdin) == NULL)
            continue;

        send(descripteurSocket, lettre_choisie, strlen(lettre_choisie), 0);
    }

    printf("\nPartie terminee. Fermeture de la socket.\n");
    close(descripteurSocket);
    return 0;
}

