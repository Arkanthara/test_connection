
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include <stdlib.h>
#include "freertos/semphr.h"
#include "esp_http_client.h"

// Définit le nombre maximum de fois qu'on va essayer à nouveau de se connecter
#define MAX_CONNECTION 3

// Définit le temps maximal d'attente de la sémaphore avant de tout arrêter. Ce temps est défini en nano secondes
#define BLOCKTIME 10000

// On défini le nombre maximal de données lues pour notre esp_http_client_read()
#define MAX_READ 256

// Indique combien de fois on a essayer de se reconnecter
int current_connection = 0;

// Sémaphore utilisée pour attendre qu'on a obtenu une adresse ip
SemaphoreHandle_t semaphore = NULL;

// Tableau permettant de recueillir les données d'un échange http, avec sa taille...
char * buffer;
int len_buffer = 1;


// Fonction permettant de gérer les erreurs de WIFI_EVENT et de IP_EVENT
// Tous les paramètres en champ sont remplis automatiquement lorsqu'un signal est reçu
void event_handler(void * event_handler_arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
	// Si le wifi est allumé, alors on essaye de le connecter
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		esp_wifi_connect();
	}

	// Si le wifi est déconnecté, on essaye de le reconnecter. On a alors un nombre maximum d'essais (MAX_CONNECTION)
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		ESP_LOGE("Status connection", "Disconnected");
		if (current_connection < MAX_CONNECTION)
		{
			esp_wifi_connect();
			current_connection ++;
		}
	}

	// Si le wifi est connecté, on affiche simplement un message.
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
	{
		ESP_LOGI("Status connection", "Connected");
	}

	// Si on a obtenu une adresse ip, on affiche un message, notre adresse ip, et on incrémente la sémaphore pour permettre au processus de continuer
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ESP_LOGI("IP", "We have an IP address !");
		ip_event_got_ip_t * event = (ip_event_got_ip_t *) event_data;
		ESP_LOGI("IP", "The ip address is: " IPSTR, IP2STR(&event->ip_info.ip));
		if (xSemaphoreGive(semaphore) != pdTRUE)
		{
			ESP_LOGE("Semaphore", "Can't give semaphore up");
		}

		// Release a semphore
	}
}

// Fonction permettant de gérer tous les signaux envoyés par le protocole http. On ne traîte ici pas tous les signaux...
esp_err_t http_event_handler(esp_http_client_event_t * event)
{
	switch(event->event_id)
	{
		// Si on est connecté au serveur, on affiche un message.
		case HTTP_EVENT_ON_CONNECTED:
			ESP_LOGI("HTTP Status", "Connected");
			break;

		// Si on est déconnecté du serveur, on affiche un message
		case HTTP_EVENT_DISCONNECTED:
			ESP_LOGI("HTTP Status", "Disconnected");
			if (len_buffer != 1)
			{
				ESP_LOGE("HTTP Protocol", "We don't print the data obtained");
				ESP_LOGI("Data", "Good lecture");
				write(1, buffer, len_buffer);
				free(buffer);
				buffer = NULL;
				len_buffer = 1;
			}
			break;

		// Si on reçoit des données et que celles-ci sont chunked, alors on effectue des actions...
		// TODO: S'occuper du cas ou les données ne sont pas chunked...
		case HTTP_EVENT_ON_DATA:
			ESP_LOGI("HTTP Data", "Data received");
			if (esp_http_client_is_chunked_response(event->client))
			{
				ESP_LOGI("HTTP Data", "Data are chunked response");
				ESP_LOGI("HTTP Data", "Length of data: %d", event->data_len);
				printf("len_buffer: %d\n", len_buffer);

				// On indique que le tableau est plus grand
				len_buffer += event->data_len;
				printf("len_buffer_post_add: %d\n", len_buffer);
				
				// On ajoute de l'espace au tableau
				buffer = realloc(buffer, sizeof(char) * len_buffer);

				// On ajoute les données reçues dans le tableau
				for (int i = 0; i < event->data_len; i++)
				{
					buffer[len_buffer - 1 - event->data_len + i] = ((char *) event->data)[i];
				}

				// On indique que notre dernier caractère est '\n'
				// C'est pour cette raison que j'initialise la taille du tableau à 1 et qu'il y a un -1 dans la ligne 101...
				buffer[len_buffer - 1] = '\n';
			}

			//Ici, il faudrait définir une action à faire dans le cas ou les données ne sont pas chunked...
			// Comme les données ne sont pas chunked, on peut normalement utiliser la fonction esp_http_client_content_length(client)
			// pour avoir la longueur des données reçues... Mais je ne sais pas si c'est vraiment utile de faire la distinction entre
			// chunked response et l'autre cas...
			else
			{
				ESP_LOGI("HTTP Data", "Data aren't chunked response. Do something");
			}
			break;

		// Lorsqu'on a fini de recevoir les données, on affiche celles-ci, on libère l'espace occupé par le tableau et on réinitialise la longueur du tableau
		// On aurait également pu faire autre chose avec notre tableau de données...
		case HTTP_EVENT_ON_FINISH:
			// ESP_LOG_BUFFER_CHAR("Result http method", buffer, len_buffer);
			write(1, buffer, len_buffer);
			printf("%d \n", len_buffer);
			free(buffer);
			buffer = NULL;
			len_buffer = 1;
			break;

		// Par défaut, on ne fait rien
		default:
			break;
	}

	// On retourne ESP_OK. Faut-il retourner autre chose à la place de ESP_OK ???
	return ESP_OK;
}

int http_test(void)
{
	// Wait for sempahore to be released 

	// Initialisation handler pour gestion signaux
	esp_event_handler_instance_t http_handler;
	esp_event_handler_instance_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, event_handler, NULL, &http_handler);

	// Initialisation structure de configuration pour échange http
	esp_http_client_config_t http_configuration = {
	//	.url = "http://20.103.43.247/prv/healthz",
		.url = "http://20.103.43.247/cmp/api/v1/Sim",
		.event_handler = http_event_handler,
	};

	// Création de la connection
	esp_http_client_handle_t client = esp_http_client_init(&http_configuration);
	if (client == NULL)
	{
		ESP_LOGE("Initialisation connection http","Erreur lors de l'initialisation de la connection http !");
		return -1;
	}

	int error = esp_http_client_open(client, 0);
	if (error != ESP_OK)
	{
		ESP_LOGE("Client http", "Connection échouée: %s", esp_err_to_name(error));
		esp_http_client_cleanup(client);
		return -1;
	}
	error = esp_http_client_fetch_headers(client);
	if (error != ESP_OK && error != ESP_ERR_HTTP_EAGAIN)
	{
		ESP_LOGE("HTTP Protocol", "Error when trying fetch headers: %s", esp_err_to_name(error));
		esp_http_client_cleanup(client);
		return -1;
	}
	int * test = malloc (sizeof(int));
	error = esp_http_client_flush_response(client, test);
	if (error != ESP_OK)
	{
		ESP_LOGE("HTTP Protocol", "Error when trying get response of server");
		return -1;
	}
	printf("len obtain by flush: %d\n", *test);
	if (esp_http_client_is_complete_data_received(client))
	{
		ESP_LOGI("HTTP Data", "Toutes les données ont été reçues");
	}

	// while (1)
	// {
	// 	error = esp_http_client_read(client, buffer_2, MAX_READ);
	// 	if (error == ESP_ERR_HTTP_EAGAIN)
	// 	{
	// 		ESP_LOGE("HTTP Protocol", "Délai dépassé... On renvoie donc ce qu'on a reçu");
	// 		break;
	// 	}
	// 	// printf("Content_size: %s\n", strerror(error));
	// }

	// while (!esp_http_client_is_complete_data_received(client))
	// {
	// 	ESP_LOGE("Coucou", "Coucou");
	// 	error = esp_http_client_fetch_headers(client);
	// 	if (error != ESP_OK)
	// 	{
	// 		ESP_LOGE("HTTP Protocol", "Error when trying fetch headers 2");
	// 		esp_http_client_cleanup(client);
	// 		return -1;
	// 	}
	// }
	// printf("%s\n", buffer);
	// write(1, buffer, len_buffer);

	// Fermeture de la connection et libération des ressources
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
	ESP_LOGI("OK", "Tout s'est bien passé jusqu'ici ");
	return 0;
}


int connect_to_wifi(void)
// PENSER À LIBÉRER LES RESSOURCES !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
{
	// Initialisation de notre sémaphore
	semaphore = xSemaphoreCreateBinary();
	if (semaphore == NULL)
	{
		ESP_LOGE("Semaphore", "Erreur lors de la création d'une sémaphore");
		return -1;
	}

	// Initialisation d'une network stack
	int error = esp_netif_init();
	if (error != ESP_OK)
	{
		ESP_LOGE("Initialisation network stack", "Error: %s", esp_err_to_name(error));
		return -1;
	}

	// Création d'un default event loop
	error = esp_event_loop_create_default();
	if (error != ESP_OK)
	{
		ESP_LOGE("Initialisation wifi", "Erreur lors de l'allocution des ressources: %s", esp_err_to_name(error));
		return -1;
	}

	// Création de la network stack
	esp_netif_t * netif = esp_netif_create_default_wifi_sta();

	// Enregistrement des handlers
	esp_event_handler_instance_t any_id;
	esp_event_handler_instance_t got_ip;
	// Handler exécuté pour chaque event du type spécifié
	esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL, &any_id);
	esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL, &got_ip);

	// Initialisation du wifi (Allocution de resources...)
	wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
	error = esp_wifi_init(&config);
	if (error != ESP_OK)
	{
		ESP_LOGE("Initialisation wifi", "Erreur lors de l'allocution des ressources: %s", esp_err_to_name(error));
		return -1;
	}

	// Définition du mode de fonctionnement du wifi. Dans notre cas, cela correspont au mode station
	error = esp_wifi_set_mode(WIFI_MODE_STA);
	if (error != ESP_OK)
	{
		ESP_LOGE("Mode wifi", "Erreur: %s", esp_err_to_name(error));
		ESP_ERROR_CHECK(esp_wifi_deinit());
		return -1;
	}

	// Démarrage du wifi
	error = esp_wifi_start();
	if (error != ESP_OK)
	{
		ESP_LOGE("Démarrage wifi", "Erreur: %s", esp_err_to_name(error));
		ESP_ERROR_CHECK(esp_wifi_deinit());
		return -1;
	}
	// Permet de scanner les réseaux...
	/* Cette partie (scan) n'est plus fonctionnelle car dès qu'on a activé le wifi, on se connecte directement à un réseau wifi... Le scan ne marche donc plus...
	// En effet, on ne peut pas se connecter et effectuer un scan en même temps...
	// Scan des réseaux
	error = esp_wifi_scan_start(NULL, true);
	if (error != ESP_OK)
	{
		ESP_LOGE("Scan wifi", "Erreur: %s\n", esp_err_to_name(error));
	}

	uint16_t  number = 10;
	wifi_ap_record_t ap_records[10];
	uint16_t ap_count = 0;

	error = esp_wifi_scan_get_ap_records(&number, ap_records);
	if (error != ESP_OK)
	{
		ESP_LOGE("Résultats scan wifi", "Erreur: %s\n", esp_err_to_name(error));
	}

	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

	for (int i = 0; i < ap_count; i++)
	{
		ESP_LOGI("Réseau", "ssid: %s\n", ap_records[i].ssid);
	}

	*/

	// Configuration du wifi
	wifi_config_t wifi = {
		.sta = {
			.ssid = "Livebox-4130",
			//.password = "428fdcf3d44d5e92a54d1ca5579d21416be03291895184d724abf652f24a",
			.password = "LrKkE5HeSixXowpGgb",
		},
	};
	error = esp_wifi_set_config(WIFI_IF_STA, &wifi);
	if (error != ESP_OK)
	{
		ESP_LOGE("Configuration wifi", "Erreur: %s", esp_err_to_name(error));
		ESP_ERROR_CHECK(esp_wifi_stop());
		ESP_ERROR_CHECK(esp_wifi_deinit());
		return -1;
	}

	// Attente de notre sémaphore pour continuer (attente de l'attribution de l'adresse ip...)
	if (xSemaphoreTake(semaphore, (TickType_t) BLOCKTIME / portTICK_PERIOD_MS) != pdTRUE)
	{
		ESP_LOGE("Semaphore", "Timeout for connection");
		ESP_ERROR_CHECK(esp_wifi_stop());
		ESP_ERROR_CHECK(esp_wifi_deinit());
		return -1;
	}

	// Exécution fonction testant des requêtes http
	http_test();

	// Libération des ressources
	ESP_LOGI("OK", "Tout se passe bien jusqu'ici");
	ESP_ERROR_CHECK(esp_wifi_stop());
	ESP_ERROR_CHECK(esp_wifi_deinit());
	esp_netif_destroy_default_wifi(netif);
	ESP_ERROR_CHECK(esp_event_loop_delete_default());
	return 0;

}

/*
int http_test(void)
{
	// Initialisation structure de configuration pour échange http
	esp_http_client_config_t http_configuration = {
		.url = "http://20.103.43.247/cmp/api/v1/Sim",
		.event_handler = http_event_handler,
	};

	// Création de la connection
	esp_http_client_handle_t client = esp_http_client_init(&http_configuration);
	if (client == NULL)
	{
		ESP_LOGE("Initialisation connection http","Erreur lors de l'initialisation de la connection http !");
		return -1;
	}

	// Fonction qui appelle toutes les fonctions nécessaires pour un transfert http
	// Pas réussi à utiliser esp_http_client_open, esp_http_client_fetch_headers, esp_http_client_read à la place...
	// Faut-il utiliser qu'une seule fois fetch_headers et ensuite boucler tant qu'il reste des données et faire read ????
	int error = esp_http_client_perform(client);
	if (error != ESP_OK)
	{
		ESP_LOGE("HTTP Protocol", "Failed to perform");
		esp_http_client_cleanup(client);
		return -1;
	}

	// Connection à un autre serveur http pour établir une autre connection http
	error = esp_http_client_set_url(client, "http://20.103.43.247/prv/healthz");
	if (error != ESP_OK)
	{
		ESP_LOGE("Initialisation connection http", "Erreur lors de l'initialisation de la connection http !");
		esp_http_client_cleanup(client);
		return -1;
	}

	// Idem que pour au-dessus
	error = esp_http_client_perform(client);
	if (error != ESP_OK)
	{
		ESP_LOGE("HTTP Protocol", "Failed to perform");
		esp_http_client_cleanup(client);
		return -1;
	}

	// Fermeture de la connection et libération des ressources
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
	ESP_LOGI("OK", "Tout s'est bien passé jusqu'ici ");
	return 0;
}
*/




/*

                 |          (A) USER CODE                 |
                 |                                        |
    .............| init          settings      events     |
    .            +----------------------------------------+
    .               .                |           *
    .               .                |           *
--------+        +===========================+   *     +-----------------------+
        |        | new/config     get/set    |   *     |                       |
        |        |                           |...*.....| init                  |
        |        |---------------------------|   *     |                       |
  init  |        |                           |****     |                       |
  start |********|  event handler            |*********|  DHCP                 |
  stop  |        |                           |         |                       |
        |        |---------------------------|         |                       |
        |        |                           |         |    NETIF              |
  +-----|        |                           |         +-----------------+     |
  | glue|----<---|  esp_netif_transmit       |--<------| netif_output    |     |
  |     |        |                           |         |                 |     |
  |     |---->---|  esp_netif_receive        |-->------| netif_input     |     |
  |     |        |                           |         + ----------------+     |
  |     |....<...|  esp_netif_free_rx_buffer |...<.....| packet buffer         |
  +-----|        |                           |         |                       |
        |        |                           |         |         (D)           |
  (B)   |        |          (C)              |         +-----------------------+
--------+        +===========================+
communication                                                NETWORK STACK
DRIVER                   ESP-NETIF
*/