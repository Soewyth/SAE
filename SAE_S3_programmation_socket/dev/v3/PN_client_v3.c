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
char DEVINER_MOT[MAX_WORD_LENGTH + 1];
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
    int role;         // gestion du rôle pour le joueur 1 -> deviner le mot / 2 -> propose lettre

    // --- Variables d'état du jeu (le client doit les gérer pour l'affichage) ---

    char status_mot[20];                 // Pour lire le début du message du serveur (ex: "start", "victoire")
    int essais_restants = NB_ESSAIS_MAX; // nombre d'essais restants

    // Le client a besoin de l'IP, du port et de la longueur du mot à deviner.
    if (argc > 3) // On vérifie si on a 3 arguments après le nom du programme (argv[0])
    {
        strncpy(ip_dest, argv[1], 16);     // Récupération de l'adresse IP (argv[1])
        sscanf(argv[2], "%d", &port_dest); // Récupération du port (argv[2])
        role = atoi(argv[3]);              // Récupération du rôle (argv[3]) atoi -> conversion str en int
    }
    else
    {
        // Message pour expliquer comment lancer le programme
        printf("USAGE : %s <ip> <port> <role>\n", argv[0]);
        exit(-1);
    }

    if (role != 1 && role != 2)
    {
        printf("Le rôle doit être 1 (joueur 1) ou 2 (joueur 2)\n");
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
    // ===================== ROLE 1 : JOUEUR QUI FAIT DEVINER LE MOT=====================  //

    if (role == 1)
    {
        //  demander le mot à faire deviner
        char bufferMot[MAX_WORD_LENGTH + 2]; // +2 pour le '\n' et le '\0'

        printf("Entrez le mot à faire deviner (max %d lettres) : ", MAX_WORD_LENGTH);
        if (fgets(bufferMot, sizeof(bufferMot), stdin) == NULL)
        {
            fprintf(stderr, "Erreur de lecture du mot à faire deviner.\n");
            close(descripteurSocket);
            exit(-3);
        }

        // Remplace le '\n' par '\0' (\n du fgets par fin de chaîne (\0))
        bufferMot[strcspn(bufferMot, "\n")] = '\0';

        // Vérifier longueur > 0
        if (strlen(bufferMot) == 0)
        {
            fprintf(stderr, "Le mot ne peut pas être vide.\n");
            close(descripteurSocket);
            exit(-4);
        }

        // Copier dans DEVINER_MOT pour le jeu
        strncpy(DEVINER_MOT, bufferMot, MAX_WORD_LENGTH);

        // Mettre le mot en majuscules car on fait toupper sur les propositions
        for (int i = 0; DEVINER_MOT[i] != '\0'; i++)
        {
            DEVINER_MOT[i] = (char)toupper(DEVINER_MOT[i]);
        }

        // ================= PARTIE PENDU ================= //

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
    // ===================== ROLE 2 : JOUEUR QUI PROPOSE LES LETTRES =====================  //

    else
    {
        char lettre_choisie[LG_MESSAGE];   // Buffer pour la lettre proposée par l'utilisateur
        char mot_affiche[MAX_WORD_LENGTH]; // Le mot avec les tirets et les lettres trouvées (ex: T__T)
        char status_mot[20];               // Pour lire le début du message du serveur (ex: "start", "victoire")
        int longueur_mot = 0;              // La longueur du mot (définie par la ligne de commande)
        int partie_en_cours = 0;           // Vaut 1 si on joue, 0 si c'est fini

        memset(messageRecu, 0x00, LG_MESSAGE);
        lus = recv(descripteurSocket, messageRecu, LG_MESSAGE - 1, 0);

        if (lus < 0)
        {
            perror("Erreur de réception du message de démarrage...");
            close(descripteurSocket);
            exit(-3);
        }
        if (lus == 0)
        {
            fprintf(stderr, "Le serveur a coupé la connexion avant le début du jeu.\n");
            close(descripteurSocket);
            exit(-4);
        }
        messageRecu[lus] = '\0';
        printf("[MESSAGE RECU] : %s\n", messageRecu);

        // Le serveur doit répondre : "start [longueur] [masque]" (ex: "start 4 ____")
        if (sscanf(messageRecu, "%s %d %s", status_mot, &longueur_mot, mot_affiche) == 3) // 3 car on vérifie que kes 3 elem sont parsés
        {

            partie_en_cours = 1;
            printf("Mot initial : %s\n", mot_affiche);
        }
        else
        {
            fprintf(stderr, "Erreur: Signal de demarrage incorrect reçu du serveur. (%s)\n", messageRecu);
            partie_en_cours = 0;
        }

        // Boucle du jeu
        while (partie_en_cours)
        {

            printf("Proposez une lettre (et appuyez sur Entree) : ");
            // En lecture de la lettre choisie par l'utilisateur
            if (fgets(lettre_choisie, sizeof(lettre_choisie), stdin) == NULL)
            {
                fprintf(stderr, "Erreur de lecture de la lettre.\n");
                continue; // Recommence la boucle
            }

            lus = send(descripteurSocket, lettre_choisie, strlen(lettre_choisie), 0); // On envoie la lettre + '\0'
            printf("[MESSAGE ENVOYE] Lettre envoyée: %s\n", lettre_choisie);

            memset(messageRecu, 0x00, LG_MESSAGE);
            lus = recv(descripteurSocket, messageRecu, LG_MESSAGE - 1, 0);

            if (lus <= 0)
            {
                fprintf(stderr, "Connexion perdue pendant le jeu.\n");
                partie_en_cours = 0;
                break;
            }
            messageRecu[lus] = '\0';
            printf("[MESSAGE RECU] : %s\n", messageRecu);

            //  GEstion victoire
            // strcasestr permet de chercher une sous-chaîne (insensitive case)
            if (strcasestr(messageRecu, "VICTOIRE") != NULL)
            {
                printf("\nVous avez gagné !\n");
                partie_en_cours = 0;
                break;
            }
            // GEstion défaite
            // strcasestr permet de chercher une sous-chaîne (insensitive case)
            if (strcasestr(messageRecu, "DEFAITE") != NULL)
            {
                printf("\n Vous avez perdu...\n");
                partie_en_cours = 0;
                break;
            }
        }

        printf("\nPartie terminee. Fermeture de la socket.\n");
        close(descripteurSocket);

        return 0;
    }
}
