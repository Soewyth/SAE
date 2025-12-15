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

/* ================== MAIN ================== */

int main(int argc, char *argv[])
{
	int socketEcoute;
	struct sockaddr_in pointDeRencontreLocal; // Adresse + port d’écoute du serveur
	socklen_t longueurAdresse;				  // stocker la taille de l’adresse du socket
	socklen_t longueurAdresseJ1;			  // stocker la taille de l’adresse du socketJ1
	socklen_t longueurAdresseJ2;			  // stocker la taille de l’adresse du socketJ2

	// Crée un socket de communication
	socketEcoute = socket(AF_INET, SOCK_STREAM, 0); // Domaine Internet (IPV4), type de socket (TCPIP)
	// Teste la valeur renvoyée par l’appel système socket()
	if (socketEcoute < 0)
	{
		perror("socket"); // Affiche le message d’erreur
		exit(-1);		  // On sort en indiquant un code erreur
	}
	printf("Socket créée avec succès ! (%d)\n", socketEcoute); // On prépare l’adresse d’attachement locale

	// Remplissage de sockaddrDistant (structure sockaddr_in identifiant le point d'écoute local)
	longueurAdresse = sizeof(pointDeRencontreLocal); // on stock la taille de la structure sockaddr_in

	memset(&pointDeRencontreLocal, 0x00, longueurAdresse);	   // memset met à zero la structure
	pointDeRencontreLocal.sin_family = AF_INET;				   // Domaine Internet
	pointDeRencontreLocal.sin_addr.s_addr = htonl(INADDR_ANY); // attaché à toutes les interfaces locales disponibles
	pointDeRencontreLocal.sin_port = htons(PORT);			   // port = 5000 ou plus

	// On demande l’attachement local de la socket, pour dire a la socket sur quel port écouter
	if ((bind(socketEcoute, (struct sockaddr *)&pointDeRencontreLocal, longueurAdresse)) < 0)
	{
		perror("bind");
		exit(-2);
	}
	printf("Socket attachée avec succès !\n");

	// On fixe la taille de la file d’attente à 5 (pour les demandes de connexion non encore traitées)
	// On transforme la socket en socket d’écoute
	if (listen(socketEcoute, 5) < 0)
	{
		perror("listen");
		exit(-3);
	}
	printf("Socket placée en écoute passive ...\n");

	// Boucle infinie : le serveur ne gère qu'une partie à la fois, mais peut enchaîner les parties
	while (1)
	{
		int socketJ1, socketJ2;			   // Sockets de dialogue avec les clients
		struct sockaddr_in addrJ1, addrJ2; // structures pour les adresses des joueurs
		char bufferGeneral[LG_MESSAGE];	   // Buffer pour stocker le message envoyé
		int lus, ecrits;				   // nb d’octets lus et écrits et envoies les messages

		printf("Attente des connexions des deux joueurs...\n");

		longueurAdresseJ1 = sizeof(addrJ1);
		memset(&addrJ1, 0x00, longueurAdresseJ1);	// memset met à zero la structure
		addrJ1.sin_family = AF_INET;				// Domaine Internet
		addrJ1.sin_addr.s_addr = htonl(INADDR_ANY); // attaché à toutes les interfaces locales disponibles
		addrJ1.sin_port = htons(PORT);				// port = 5000 ou plus

		socketJ1 = accept(socketEcoute, (struct sockaddr *)&addrJ1, &longueurAdresseJ1);
		if (socketJ1 < 0)
		{
			perror("Nouvelle connexion J1 impossible");
			continue; // continue pour relancer la boucle sans fermé le serveur
		}
		printf("[SERVEUR] Joueur 1 connecté !\n");

		longueurAdresseJ2 = sizeof(addrJ2);
		memset(&addrJ2, 0x00, longueurAdresseJ2);	// memset met à zero la structure
		addrJ2.sin_family = AF_INET;				// Domaine Internet
		addrJ2.sin_addr.s_addr = htonl(INADDR_ANY); // attaché à toutes les interfaces locales disponibles
		addrJ2.sin_port = htons(PORT);				// port = 5000 ou plus
		socketJ2 = accept(socketEcoute, (struct sockaddr *)&addrJ2, &longueurAdresseJ2);
		if (socketJ2 < 0)
		{
			perror("Nouvelle connexion J2 impossible");
			close(socketJ1);
			continue; // continue pour relancer la boucle sans fermé le serveur
		}
		printf("[SERVEUR] Joueur 2 connecté !\n");

		// V3 gère la partie dans un processus fils
		// Le proccesssus père lui va attendre une nouvelle partie
		// -1 erreur de fork
		pid_t pid;
		pid = fork();

		switch (pid)
		{
		case -1:
			perror("Erreur de fork");
			close(socketJ1);
			close(socketJ2);
			continue; // On repasse à une nouvelle partie retour début boucle
		case 0:
			// Processus fils : gestion de la partie
			printf("[SERVEUR - FILS - PID : %d] Démarrage de la partie : \n", getpid());

			// Le fils a pas besoin d'écouter car il njoue
			close(socketEcoute);
			// ====================  RECUPERER MOT A DEVINER ====================
			// On reçoit le mot à deviner du joueur 1
			memset(bufferGeneral, 0x00, LG_MESSAGE);
			lus = recv(socketJ1, bufferGeneral, LG_MESSAGE - 1, 0);
			if (lus <= 0)
			{
				perror("Erreur de réception du mot à deviner depuis J1");
				close(socketJ1);
				close(socketJ2);
				exit(-1); // On repasse à une nouvelle partie retour début boucle
			}
			bufferGeneral[lus] = '\0'; // On termine la chaîne reçue
			printf("[SERVEUR - FILS - PID : %d] Mot à deviner reçu de J1 : %s\n",
				   getpid(), bufferGeneral);

			ecrits = send(socketJ2, bufferGeneral, strlen(bufferGeneral), 0);
			if (ecrits <= 0)
			{
				perror("Erreur d'envoi du mot à deviner à J2");
				close(socketJ1);
				close(socketJ2);
				close(socketJ2);
				exit(-2); // On repasse à une nouvelle partie retour début boucle
			}
			printf("[SERVEUR - FILS - PID : %d] Mot à deviner envoyé à J2\n", getpid());

			// ====================  BOUCLE RECEPTION - ENVOI MESSAGE -  BOUCLE DE JEU  ====================
			int partie_terminee = 0;
			while (!partie_terminee)
			{
				// J2 -> Serveur -> J1 (proposition dlelettre)
				// Réception de la proposition de lettre du joueur 2
				memset(bufferGeneral, 0x00, LG_MESSAGE);
				lus = recv(socketJ2, bufferGeneral, LG_MESSAGE - 1, 0);
				// Erreur de réception
				if (lus <= 0)
				{
					perror("Erreur de réception de la proposition de lettre depuis J2");
					break; // On repasse à une nouvelle partie retour début boucle
				}
				// Réception réussie
				bufferGeneral[lus] = '\0'; // On termine la chaîne reçue
				printf("\n[SERVEUR - FILS - PID : %d] Lettre proposée reçue de J2 : %s\n",
					   getpid(), bufferGeneral);
				// Envoi de la proposition de lettre au joueur 1
				ecrits = send(socketJ1, bufferGeneral, strlen(bufferGeneral), 0);
				// Erreur d'envoi
				if (ecrits <= 0)
				{
					perror("Erreur d'envoi de la proposition de lettre à J1");
					break; // On repasse à une nouvelle partie retour début boucle
				}
				// Envoi réussi
				printf("[SERVEUR - FILS - PID : %d] Lettre proposée envoyée à J1\n", getpid());

				// J1 -> serveur -> J2 (résultat de la proposition)

				// Réception du résultat de la proposition de lettre du joueur 1
				memset(bufferGeneral, 0x00, LG_MESSAGE);
				lus = recv(socketJ1, bufferGeneral, LG_MESSAGE - 1, 0);
				// Erreur de réception
				if (lus <= 0)
				{
					perror("Erreur de réception du résultat de la proposition depuis J1");
					break; // On repasse à une nouvelle partie retour début boucle
				}
				// Réception réussie
				bufferGeneral[lus] = '\0'; // On termine la chaîne reçue
				printf("[SERVEUR - FILS - PID : %d] Résultat de la proposition reçue de J1 : %s\n",
					   getpid(), bufferGeneral);

				if (strcasestr(bufferGeneral, "VICTOIRE") != NULL || strcasestr(bufferGeneral, "DEFAITE") != NULL)
				{
					printf("[SERVEUR - FILS - PID : %d] La partie est terminée avec le résultat : %s\n", getpid(), bufferGeneral);
					partie_terminee = 1;
				}
				// Envoi du résultat de la proposition au joueur 2
				// memset(bufferGeneral, 0x00, LG_MESSAGE); // A retirer car vide avant lenvoie ?????
				ecrits = send(socketJ2, bufferGeneral, strlen(bufferGeneral), 0);
				// Erreur d'envoi
				if (ecrits <= 0)
				{
					perror("Erreur d'envoi du résultat de la proposition à J2");
					break; // NOuvelle partie
				}
				// Envoi réussi
				printf("[SERVEUR - FILS - PID : %d] Résultat de la proposition envoyée à J2\n", getpid());

			} // Fin de la boucle de jeu

			// ON ferme les sockets car le père en a plus besoin
			close(socketJ1);
			close(socketJ2);
			exit(0); // terminer programme fils

		default:
			// ON ferme les sockets car le père en a plus besoin
			close(socketJ1);
			close(socketJ2);
			continue; // On repasse à une nouvelle partie retour début boucle
		}
	}
	close(socketEcoute);
	return 0;
}