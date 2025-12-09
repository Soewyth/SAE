#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour read, write, close, sleep */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>		/* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h>	/* pour htons et inet_aton */
#include <ctype.h>		/* pour tolower */

/* ================== DEFINE  ================== */
#define PORT 5000		//(ports >= 5000 réservés pour usage explicite)
#define LG_MESSAGE 256	// buffer qui gère les messages
#define NB_ESSAIS_MAX 6 // NOmbres d'essais maximum pour deviner le mot

/* ================== VAr globale ================== */
const char *DEVINER_MOT = "TEST";

/* ================== MAIN ================== */

int main(int argc, char *argv[])
{
	int socketEcoute;
	struct sockaddr_in pointDeRencontreLocal;
	socklen_t longueurAdresse;

	int socketDialogue;
	struct sockaddr_in pointDeRencontreDistant;
	char messageRecu[LG_MESSAGE];	/* le message de la couche Application ! */
	char messageEnvoye[LG_MESSAGE]; /* le message de la couche Application ! */
	int ecrits, lus;				/* nb d’octets ecrits et lus */

	// Crée un socket de communication
	socketEcoute = socket(AF_INET, SOCK_STREAM, 0);
	// Teste la valeur renvoyée par l’appel système socket()
	if (socketEcoute < 0)
	{
		perror("socket"); // Affiche le message d’erreur
		exit(-1);		  // On sort en indiquant un code erreur
	}
	printf("Socket créée avec succès ! (%d)\n", socketEcoute); // On prépare l’adresse d’attachement locale
	// setsockopt()

	// Remplissage de sockaddrDistant (structure sockaddr_in identifiant le point d'écoute local)
	longueurAdresse = sizeof(pointDeRencontreLocal);
	// memset sert à faire une copie d'un octet n fois à partir d'une adresse mémoire donnée
	// ici l'octet 0 est recopié longueurAdresse fois à partir de l'adresse &pointDeRencontreLocal
	memset(&pointDeRencontreLocal, 0x00, longueurAdresse);
	pointDeRencontreLocal.sin_family = PF_INET;
	pointDeRencontreLocal.sin_addr.s_addr = htonl(INADDR_ANY); // attaché à toutes les interfaces locales disponibles
	pointDeRencontreLocal.sin_port = htons(PORT);			   // = 5000 ou plus

	// On demande l’attachement local de la socket
	if ((bind(socketEcoute, (struct sockaddr *)&pointDeRencontreLocal, longueurAdresse)) < 0)
	{
		perror("bind");
		exit(-2);
	}
	printf("Socket attachée avec succès !\n");

	// On fixe la taille de la file d’attente à 5 (pour les demandes de connexion non encore traitées)
	if (listen(socketEcoute, 5) < 0)
	{
		perror("listen");
		exit(-3);
	}
	printf("Socket placée en écoute passive ...\n");

	// boucle d’attente de connexion : en théorie, un serveur attend indéfiniment !
	while (1)
	{
		memset(messageRecu, 'a', LG_MESSAGE * sizeof(char));
		printf("Attente d’une demande de connexion (quitter avec Ctrl-C)\n\n");

		// c’est un appel bloquant
		socketDialogue = accept(socketEcoute, (struct sockaddr *)&pointDeRencontreDistant, &longueurAdresse);
		if (socketDialogue < 0)
		{
			perror("accept");
			close(socketDialogue);
			close(socketEcoute);
			exit(-4);
		}
		printf("Connexion acceptee d’un client ! (socket dialogue : %d)\n", socketDialogue);

		/* *
		 *	 GESTION PARTIE PENDU
		 *
		 */

		// déclaration variables partie pendu
		int length_mot = strlen(DEVINER_MOT); // longueur du mot à deviner
		int essais_restants = NB_ESSAIS_MAX;  // nombre d'essais restants
		char masque_mot[LG_MESSAGE];		  // mot masqué envoyé au client
		int lettres_essayees[26] = {0};		  // init tableau 26 cases à 0

		// INitialisation du mot masqué
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
		ecrits = send(socketDialogue, messageEnvoye, strlen(messageEnvoye), 0);

		// Gestion erreur envoi message start
		switch (ecrits)
		{
		case -1:
			perror("Erreur du message");
			close(socketDialogue);
			continue; // continue pour relancer la boucle sans fermé le serveur
		case 0:
			fprintf(stderr, "La socket a été fermée par le client !\n\n");
			close(socketDialogue);
			continue;
		default:
			printf("Message envoyé : %s (%d octets)\n\n", messageEnvoye, ecrits);
		}

		// Boucle du jeu pendu --> Réception / envoi client

		int partie_terminee = 0;
		while (!partie_terminee && essais_restants > 0)
		{
			memset(messageRecu, 0x00, LG_MESSAGE);
			lus = recv(socketDialogue, messageRecu, LG_MESSAGE - 1, 0);
			// Erreur de réception
			switch (lus)
			{
			case -1: /* une erreur ! */
				perror("read");
				close(socketDialogue);
				partie_terminee = 1;
				continue;
			case 0: /* la socket est fermée */
				fprintf(stderr, "La socket a été fermée par le client !\n\n");
				close(socketDialogue);
				partie_terminee = 1;
				continue;
			default: /* réception de n octets */
				printf("Message reçu : %s (%d octets)\n\n", messageRecu, lus);
			}
			// Gestion message reçu du client
			char proposition = messageRecu[0];
			proposition = (char)toupper(proposition); // to lower case de la la lettre proposée
			printf("Lettre proposée par le client : %c\n", proposition);

			// GEstion hors tableau des lettres proposées
			if (proposition < 'A' || proposition > 'Z')
			{
				memset(messageEnvoye, 0x00, LG_MESSAGE);
				snprintf(messageEnvoye, LG_MESSAGE, "Lettre invalide. Veuillez proposer une lettre entre A et Z.");
				ecrits = send(socketDialogue, messageEnvoye, strlen(messageEnvoye), 0);
				continue; // revient au début de la boucle

			} 
			
			// Calculer l’index dans le tableau 0..25 car les nombres sont en ASCII -> A=65 ... Z=90
			// A=0, B=1, C=2 ... Z=25 
			int idx = proposition - 'A'; 

			// Gestion lettre déjà proposée 
			if (lettres_essayees[idx] == 1)
			{
				memset(messageEnvoye, 0x00, LG_MESSAGE);
				snprintf(messageEnvoye, LG_MESSAGE,
						 "Lettre déjà proposée. Veuillez en proposer une autre.");
				ecrits = send(socketDialogue, messageEnvoye, strlen(messageEnvoye), 0);
				continue;
			}
			// L'index met à 1 la valeur du tableau correspondant à la lettre proposée
			lettres_essayees[idx] = 1;

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
				snprintf(messageEnvoye, LG_MESSAGE, "VICTOIRE LE mot était %s", DEVINER_MOT);
				ecrits = send(socketDialogue, messageEnvoye, strlen(messageEnvoye), 0);
				partie_terminee = 1;
			}
			// Vérification si partie perdue
			else if (essais_restants == 0)
			{
				memset(messageEnvoye, 0x00, LG_MESSAGE);
				snprintf(messageEnvoye, LG_MESSAGE, "DEFAITE Le mot était %s", DEVINER_MOT);
				ecrits = send(socketDialogue, messageEnvoye, strlen(messageEnvoye), 0);
				partie_terminee = 1;
			}
			// COntinuer la partie
			else
			{
				memset(messageEnvoye, 0x00, LG_MESSAGE);
				snprintf(messageEnvoye, LG_MESSAGE, "Il vous reste %d essais restants. %s", essais_restants, masque_mot);
				ecrits = send(socketDialogue, messageEnvoye, strlen(messageEnvoye), 0);
			}
		}
	}
	// On ferme la ressource avant de quitter
	close(socketEcoute);
	return 0;
}
