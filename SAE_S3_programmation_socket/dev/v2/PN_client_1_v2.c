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
const char *DEVINER_MOT = "TEST";
#define NB_ESSAIS_MAX 6

int main(int argc, char *argv[])
{
    // --- Déclarations des variables réseau et de base ---
    int descripteurSocket;
    struct sockaddr_in sockaddrDistant;
    socklen_t longueurAdresse;

    int nb;
    char messageRecu[LG_MESSAGE];   // Buffer pour stocker le message recu
    char messageEnvoye[LG_MESSAGE]; // Buffer pour stocker le message envoyé
    int ecrits, lus;                /* nb d’octets ecrits et lus */
                                    /* Nombre d’octets écrits ou lus */

    char ip_dest[16]; // Adresse IP du serveur
    int port_dest;    // Port du serveur

    // --- Variables d'état du jeu (le client doit les gérer pour l'affichage) ---

    char status_mot[20];                 // Pour lire le début du message du serveur (ex: "start", "victoire")
    int essais_restants = NB_ESSAIS_MAX; // nombre d'essais restants

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
    /* *
     *	 GESTION PARTIE PENDU
     *
     */

    // déclaration variables partie pendu
    int length_mot = strlen(DEVINER_MOT); // longueur du mot à deviner
    char masque_mot[LG_MESSAGE];          // mot masqué envoyé au client
    int lettres_essayees[26] = {0};       // init tableau 26 cases à 0

    // Initialisation du mot masqué
    for (int i = 0; i < length_mot; i++)
    {
        masque_mot[i] = '_'; // initialisation du mot masqué avec des underscores
    }
    masque_mot[length_mot] = '\0'; // terminer la chaine de caractères

    // Envoie du message start
    memset(messageEnvoye, 0x00, LG_MESSAGE);
    // int snprintf(char *buffer, taille_max, const char *format, taille_mot, masque_mot);
    // FOnction sécurisé car il vérifie la taille max du buffer (lgmessage)
    snprintf(messageEnvoye, LG_MESSAGE, "START %d %s", length_mot, masque_mot);
    ecrits = send(descripteurSocket, messageEnvoye, strlen(messageEnvoye), 0);

    // Gestion erreur envoi message start
    switch (ecrits)
    {
    case -1:
        perror("Erreur du message");
        close(descripteurSocket);
        break; // continue pour relancer la boucle sans fermé le serveur
    case 0:
        fprintf(stderr, "La socket a été fermée par le client !\n\n");
        close(descripteurSocket);
        break;
    default:
        printf("[MESSAGE ENVOYE] : %s (%d octets)\n\n", messageEnvoye, ecrits);
    }

    // Boucle du jeu pendu --> Réception / envoi client

    int partie_terminee = 0;
    while (!partie_terminee && essais_restants > 0)
    {
        memset(messageRecu, 0x00, LG_MESSAGE);
        lus = recv(descripteurSocket, messageRecu, LG_MESSAGE - 1, 0);
        // Erreur de réception
        switch (lus)
        {
        case -1: /* une erreur ! */
            perror("read");
            close(descripteurSocket);
            partie_terminee = 1;
            continue;
        case 0: /* la socket est fermée */
            fprintf(stderr, "La socket a été fermée par le client !\n\n");
            close(descripteurSocket);
            partie_terminee = 1;
            continue;
        default: /* réception de n octets */
            printf("[MESSAGE RECU] : Lettre proposée par le client : %s (%d octets)\n\n", messageRecu, lus);
        }
        // Gestion message reçu du client
        char proposition = messageRecu[0];
        proposition = (char)toupper(proposition); // to lower case de la la lettre proposée
        // GEstion hors tableau des lettres proposées
        if (proposition < 'A' || proposition > 'Z')
        {
            memset(messageEnvoye, 0x00, LG_MESSAGE);
            snprintf(messageEnvoye, LG_MESSAGE, "[MESSAGE ENVOYE] : Lettre invalide. Veuillez proposer une lettre entre A et Z.");
            ecrits = send(descripteurSocket, messageEnvoye, strlen(messageEnvoye), 0);
            continue; // revient au début de la boucle
        }

        // Calculer l’index dans le tableau 0..25 car les nombres sont en ASCII -> A=65 ... Z=90
        // A=0, B=1, C=2 ... Z=25
        int indexTableau = proposition - 'A';

        // Gestion lettre déjà proposée
        if (lettres_essayees[indexTableau] == 1)
        {
            memset(messageEnvoye, 0x00, LG_MESSAGE);
            snprintf(messageEnvoye, LG_MESSAGE,
                     "Lettre déjà proposée. Veuillez en proposer une autre."); // message envoyé au client
            ecrits = send(descripteurSocket, messageEnvoye, strlen(messageEnvoye), 0);
            continue;
        }
        // L'index met à 1 la valeur du tableau correspondant à la lettre proposée
        lettres_essayees[indexTableau] = 1;

        //  Vérification si la lettre proposée est dans le mot à deviner
        int bonne_lettre = 0;
        for (int i = 0; i < length_mot; i++)
        {
            if (DEVINER_MOT[i] == proposition && masque_mot[i] == '_')
            {
                masque_mot[i] = proposition;
                bonne_lettre = 1;
            }
        }
        // Si la lettre n'est pas dans le mot, on décrémente les essais restants
        if (!bonne_lettre)
        {
            essais_restants--;
        }
        // Vérification si partie gagnée
        if (strcmp(DEVINER_MOT, masque_mot) == 0)
        {
            memset(messageEnvoye, 0x00, LG_MESSAGE);
            snprintf(messageEnvoye, LG_MESSAGE, "VICTOIRE  - Le mot était %s", DEVINER_MOT);
            ecrits = send(descripteurSocket, messageEnvoye, strlen(messageEnvoye), 0);
            partie_terminee = 1;
        }
        // Vérification si partie perdue
        else if (essais_restants == 0)
        {
            memset(messageEnvoye, 0x00, LG_MESSAGE);
            snprintf(messageEnvoye, LG_MESSAGE, "DEFAITE - Le mot était %s", DEVINER_MOT);
            ecrits = send(descripteurSocket, messageEnvoye, strlen(messageEnvoye), 0);
            partie_terminee = 1;
        }
        // COntinuer la partie
        else
        {
            memset(messageEnvoye, 0x00, LG_MESSAGE);
            snprintf(messageEnvoye, LG_MESSAGE, "Il vous reste %d essais restants. %s", essais_restants, masque_mot);
            ecrits = send(descripteurSocket, messageEnvoye, strlen(messageEnvoye), 0);
        }
    }
    printf("\nPartie terminee. Fermeture de la socket.\n");
    close(descripteurSocket);

    return 0;
}
