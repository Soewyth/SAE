#include <stdio.h>
#include <stdlib.h> /* pour exit */
#include <unistd.h> /* pour read, write, close, sleep */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>		/* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h>	/* pour htons et inet_aton */
#define LG_MESSAGE 256

int main(int argc, char *argv[])
{
	int descripteurSocket;
	struct sockaddr_in sockaddrDistant;
	socklen_t longueurAdresse;

	char messageDemande[LG_MESSAGE]; // buffer stockant le message
	char messageReponse[LG_MESSAGE]; // buffer stockant le message
	int nb;							 /* nb d’octets écrits et lus */

	char ip_dest[16];
	int port_dest;

	// Pour pouvoir contacter le serveur, le client doit connaître son adresse IP et le port de comunication
	// Ces 2 informations sont passées sur la ligne de commande
	// Si le serveur et le client tournent sur la même machine alors l'IP locale fonctionne : 127.0.0.1
	// Le port d'écoute du serveur est 5000 dans cet exemple, donc en local utiliser la commande :
	// ./client_base_tcp 127.0.0.1 5000
	if (argc > 1)
	{ // si il y a au moins 2 arguments passés en ligne de commande, récupération ip et port
		strncpy(ip_dest, argv[1], 16);
		sscanf(argv[2], "%d", &port_dest);
	}
	else
	{
		printf("USAGE : %s ip port\n", argv[0]);
		exit(-1);
	}

	// Crée un socket de communication
	descripteurSocket = socket(AF_INET, SOCK_STREAM, 0);
	// Teste la valeur renvoyée par l’appel système socket()
	if (descripteurSocket < 0)
	{
		perror("Erreur en création de la socket..."); // Affiche le message d’erreur
		exit(-1);									  // On sort en indiquant un code erreur
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

	// Envoi du message
	// switch(nb = write(descripteurSocket, buffer, strlen(buffer))){
	memset(messageDemande, 0x00, LG_MESSAGE);
	printf("Entrez la commande (D pour date, H pour heure) : ");

	if (fgets(messageDemande, LG_MESSAGE, stdin) == NULL)
	{
		perror("Le message est vide");
		close(descripteurSocket);
		exit(-3);
	}

	nb = send(descripteurSocket, messageDemande, strlen(messageDemande) + 1, 0);
	if (nb < 0)
	{
		perror("Erreur d'envoi du message");
		close(descripteurSocket);
		exit(-4);
	}
	printf("Message envoyé au serveur (%d octets) : %s\n", nb, messageDemande);


	memset(messageReponse, 0x00, LG_MESSAGE);
	nb = recv(descripteurSocket, messageReponse, LG_MESSAGE - 1, 0);
	if (nb < 0)
	{
		perror("Erreur de reception du message");
		close(descripteurSocket);
		exit(-5);
	}
	else if (nb == 0)
	{
		printf("Le serveur a fermé la connexion\n");
		close(descripteurSocket);
		exit(0);
	}
	else
	{
		messageReponse[nb] = '\0';
		printf("Message reçu du serveur (%d octets) : %s\n", nb, messageReponse);
	}

	// On ferme la ressource avant de quitter
	close(descripteurSocket);

	return 0;
}
