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
#define MAX_WORD_LENGTH 20  // On augmente un peu la taille pour le Pendu
#define MSG_BUFFER_SIZE 512 // Taille pour être sûr de recevoir toute la réponse du serveur

int main(int argc, char *argv[])
{
    // --- Déclarations des variables réseau et de base ---
    int descripteurSocket;
    struct sockaddr_in sockaddrDistant;
    socklen_t longueurAdresse;

    char messageDemande[LG_MESSAGE];      // Buffer pour le message que NOUS envoyons (la lettre ou "start")
    char messageReponse[MSG_BUFFER_SIZE]; // Buffer pour la réponse que NOUS recevons du serveur
    int nb;                               /* Nombre d’octets écrits ou lus */

    char bufferReception[MSG_BUFFER_SIZE]; // Buffer générique (on pourrait utiliser messageReponse)
    char ip_dest[16];                      // Adresse IP du serveur
    int port_dest;                         // Port du serveur

    // --- Variables d'état du jeu (le client doit les gérer pour l'affichage) ---
    char mot_affiche[MAX_WORD_LENGTH]; // Le mot avec les tirets et les lettres trouvées (ex: T_ST)
    char lettre_choisie[20];           // Buffer pour la lettre proposée par l'utilisateur
    int longueur_mot = 0;              // La longueur du mot (définie par la ligne de commande)
    int partie_en_cours = 0;           // Vaut 1 si on joue, 0 si c'est fini

    char status_mot[10];     // Pour lire le début du message du serveur (ex: "start", "victoire")
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
    longueurAdresse = sizeof(sockaddrDistant);
    // Initialise à 0 la structure sockaddr_in
    // memset sert à faire une copie d'un octet n fois à partir d'une adresse mémoire donnée
    // ici l'octet 0 est recopié longueurAdresse fois à partir de l'adresse &sockaddrDistant
    memset(&sockaddrDistant, 0x00, longueurAdresse);
    // Renseigne la structure sockaddr_in avec les informations du serveur distant
    sockaddrDistant.sin_family = AF_INET;
    // On choisit le numéro de port d’écoute du serveur
    sockaddrDistant.sin_port = htons(port_dest);
    // On choisit l’adresse IPv4 du serveur
    inet_aton(ip_dest, &sockaddrDistant.sin_addr);

    // Débute la connexion vers le processus serveur distant
    if ((connect(descripteurSocket, (struct sockaddr *)&sockaddrDistant, longueurAdresse)) == -1)
    {
        perror("Erreur de connection avec le serveur distant...");
        close(descripteurSocket);
        exit(-2); // On sort en indiquant un code erreur
    }
    printf("Connexion au serveur %s:%d réussie!\n", ip_dest, port_dest);

    // Ensuite, le client attend la réponse du serveur pour démarrer
    nb = recv(descripteurSocket, messageReponse, MSG_BUFFER_SIZE - 1, 0);

    if (nb <= 0)
    {
        fprintf(stderr, "Serveur a coupé la connexion ou erreur de réception.\n");
        close(descripteurSocket);
        exit(0);
    }
    messageReponse[nb] = '\0';

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
    while (partie_en_cours)
    {

        printf("Mot actuel : %s\n", mot_affiche);
        printf("Proposez une lettre (et appuyez sur Entree) : ");

        if (fgets(lettre_choisie, sizeof(lettre_choisie), stdin) == NULL)
        {
            fprintf(stderr, "Erreur de lecture de la lettre.\n");
            continue; // Recommence la boucle
        }
    
        // On vérifie que ce que l'utilisateur a tapé est bien une seule lettre

        nb = send(descripteurSocket, lettre_choisie, strlen(lettre_choisie), 0); // On envoie la lettre + '\0'
        printf("Lettre envoyée: %s\n", lettre_choisie);

        memset(messageReponse, 0x00, MSG_BUFFER_SIZE);
        nb = recv(descripteurSocket, messageReponse, MSG_BUFFER_SIZE - 1, 0);

        if (nb <= 0)
        {
            fprintf(stderr, "Connexion perdue pendant le jeu.\n");
            partie_en_cours = 0;
            break;
        }
        messageReponse[nb] = '\0';
        printf("Reponse du Serveur : %s\n", messageReponse);

        // 1. Victoire ou Défaite (messages de fin)
        if (strncasecmp(messageReponse, "victoire", 8) == 0)
        {
            printf("\nVICTOIRE ! Fin du jeu.\n");
            partie_en_cours = 0;
        }
        else if (strncasecmp(messageReponse, "defaite", 7) == 0)
        {
            printf("\nDEFAITE ! Fin du jeu.\n");
            partie_en_cours = 0;
        }
        // Le serveur envoie : "Il vous reste [X] essais restants. [masque]"
        else if (sscanf(messageReponse, "Il vous reste %d essais restants. %s",
                        &essais_restants, mot_affiche) == 2)
        {

            // Le client met à jour son affichage avec le masque envoyé par le serveur
            printf("\n--- %d essais restants. Nouvel état: %s ---\n", essais_restants, mot_affiche);
        }
        else
        {
            printf("\n[ERREUR PROTOCOLE] Reponse serveur inattendue.\n");
        }
    }

    printf("\nPartie terminee. Fermeture de la socket.\n");
    close(descripteurSocket);

    return 0;
}