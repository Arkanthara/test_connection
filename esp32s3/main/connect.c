
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include <stdlib.h>

#include "lwip/sys.h"
#include "lwip/err.h"

#include "esp_http_client.h"

#define MAX_CONNECTION 3

bool connection_ok = false;

int current_connection = 0;

char * buffer;
int len_buffer = 1;

// TODO: regarder comment attendre que la connection s'établisse...

void event_handler(void * event_handler_arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		ESP_LOGE("Status connection", "Disconnected");
		if (current_connection < MAX_CONNECTION)
		{
			esp_wifi_connect();
			current_connection ++;
		}
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
	{
		ESP_LOGI("Status connection", "Connected");
		// esp_netif_action_start(NULL, event_base, event_id, event_data);
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ESP_LOGI("IP", "We have an IP address !");
		ip_event_got_ip_t * event = (ip_event_got_ip_t *) event_data;
		ESP_LOGI("IP", "The ip address is: " IPSTR, IP2STR(&event->ip_info.ip));
		connection_ok = true;
	}
/*
	// Gestion des messages http !!!!
	else if (event_base == ESP_EVENT_ANY_BASE && event_id == HTTP_EVENT_ON_CONNECTED)
	{
		ESP_LOGI("HTTP Status", "Connected");
	}
	else if (event_base == ESP_EVENT_ANY_BASE && event_id == HTTP_EVENT_DISCONNECTED)
	{
		ESP_LOGI("HTTP Status", "Disconnected");
	}
	else if (event_base == ESP_EVENT_ANY_BASE && event_id == HTTP_EVENT_ON_FINISH)
	{
		ESP_LOGI("HTTP Status", "Protocol terminated");
	}
	*/
}

esp_err_t http_event_handler(esp_http_client_event_t * event)
{
	switch(event->event_id)
	{
		case HTTP_EVENT_ON_CONNECTED:
			ESP_LOGI("HTTP Status", "Connected");
			break;
		case HTTP_EVENT_DISCONNECTED:
			ESP_LOGI("HTTP Status", "Disconnected");
			break;
		case HTTP_EVENT_ON_DATA:
			ESP_LOGI("HTTP Data", "Data received");
			if (esp_http_client_is_chunked_response(event->client))
			{
				ESP_LOGI("HTTP Data", "Data are chunked response");
				ESP_LOGI("HTTP Data", "Length of data: %d", event->data_len);
				printf("len_buffer: %d\n", len_buffer);
				len_buffer += event->data_len;
				printf("len_buffer_post_add: %d\n", len_buffer);
				buffer = realloc(buffer, sizeof(char) * len_buffer);
				for (int i = 0; i < event->data_len; i++)
				{
					buffer[len_buffer - 1 - event->data_len + i] = ((char *) event->data)[i];
				}
				buffer[len_buffer - 1] = '\n';
			}
			else
			{
				ESP_LOGI("HTTP Data", "Data aren't chunked response. Do something");
			}
			break;
		case HTTP_EVENT_ON_HEADER:
			ESP_LOGI("HTTP Header", "Header received");
			break;
		default:
			ESP_LOGI("Handler", "Executed one time");
			break;

	}
	return ESP_OK;
}

void error_exit(esp_http_client_handle_t client)
{
	esp_http_client_cleanup(client);
}

int http_test(void)
{
	// Initialisation handler pour gestion signaux
	esp_event_handler_instance_t http_handler;
	esp_event_handler_instance_register(ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, event_handler, NULL, &http_handler);

	// Initialisation structure de configuration pour échange http
	esp_http_client_config_t http_configuration = {
		.url = "http://20.103.43.247/prv/healthz",
		.event_handler = http_event_handler,
	};

	// Création de la connection
	esp_http_client_handle_t client = esp_http_client_init(&http_configuration);
	if (client == NULL)
	{
		ESP_LOGE("Initialisation connection http","Erreur lors de l'initialisation de la connection http !");
		return -1;
	}

/*	// Test ici
	int error = esp_http_client_perform(client);
	if (error != ESP_OK)
	{
		ESP_LOGE("Transfert http", "Le transfert http n'a pas fonctionné correctement");
	}
	int content_length = esp_http_client_get_content_length(client);
*/

	int error = esp_http_client_open(client, 0);
	if (error != ESP_OK)
	{
		ESP_LOGE("Client http", "Connection échouée: %s", esp_err_to_name(error));
		error_exit(client);
		return -1;
	}

	error = esp_http_client_fetch_headers(client);
	if (error != ESP_OK)
	{
		ESP_LOGE("HTTP Protocol", "Error when trying fetch headers");
		error_exit(client);
		return -1;
	}

	// // On indique au server qu'on est là
	// error = esp_http_client_write(client, "Coucou !", 8);
	// if (error == -1)
	// {
	// 	ESP_LOGE("HTTP Protocol", "Failed to send message to server !");
	// }

	// On commence par définir ou
	// int content_length_chunk;
	// error = esp_http_client_get_chunk_length(client, &content_length_chunk);
	// if (error != ESP_OK)
	// {
	// 	ESP_LOGE("HTTP Protocol", "Error when trying to get content length of chunked encoding message");
	// }
	
	/*
	// On fait correspondre nos headers à ceux du server
	int content_length = esp_http_client_fetch_headers(client);
	if (content_length == ESP_FAIL)
	{
		ESP_LOGE("HTTP Protocol", "Failed to fetch header");
		error_exit(client);
		return -1;
	}
	else if (content_length == 0)
	{
		if (esp_http_client_is_chunked_response(client))
		{
			ESP_LOGI("HTTP Protocol", "Response is chunked");
			content_length = content_length_chunk;

		}
		else
		{
			ESP_LOGE("HTTP Protocol", "Stream doesn't contain content-length header");
			error_exit(client);
			return -1;
		}
	}
	printf("content_length = %d\n", content_length);

	if (content_length == 0)
	{
		ESP_LOGE("ERROR", "content_length = 0");
		error_exit(client);
		return -1;
	}

	// Ouverture de la connection et lecture du message reçu
	// ESP_ERROR_CHECK(esp_http_client_open(client, 0));
	char * buffer = malloc(content_length * sizeof(char));
	error = esp_http_client_read(client, buffer, content_length);
	if (error == -1)
	{
		ESP_LOGE("Lecture données","Une erreur est survenue lors de la réception du message du server !");
		error_exit(client);
		return -1;
	}

	*/
	// printf("%s\n", buffer);
	write(1, buffer, len_buffer);

	// Fermeture de la connection et libération des ressources
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
	ESP_LOGI("OK", "Tout s'est bien passé jusqu'ici ");
	return 0;
}

void connect_to_wifi(void)
{
	// Initialisation d'une network stack
	int error = esp_netif_init();
	if (error != ESP_OK)
	{
		ESP_LOGE("Initialisation network stack", "Error: %s", esp_err_to_name(error));
	}

	// Création d'un default event loop
	error = esp_event_loop_create_default();
	if (error != ESP_OK)
	{
		ESP_LOGE("Initialisation wifi", "Erreur lors de l'allocution des ressources: %s", esp_err_to_name(error));
	}

	// Création de la network stack
	esp_netif_create_default_wifi_sta();

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
	}

	error = esp_wifi_set_mode(WIFI_MODE_STA);
	if (error != ESP_OK)
	{
		ESP_LOGE("Mode wifi", "Erreur: %s", esp_err_to_name(error));
	}

	error = esp_wifi_start();
	if (error != ESP_OK)
	{
		ESP_LOGE("Démarrage wifi", "Erreur: %s", esp_err_to_name(error));
	}
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
	// Besoin de définir le protocole ???
	// Configuration du wifi
	wifi_config_t wifi = {
		.sta = {
			.ssid = "WIFI_Mobile",
			.password = "428fdcf3d44d5e92a54d1ca5579d21416be03291895184d724abf652f24a",
		},
	};
	error = esp_wifi_set_config(WIFI_IF_STA, &wifi);
	if (error != ESP_OK)
	{
		ESP_LOGE("Configuration wifi", "Erreur: %s", esp_err_to_name(error));
	}
	while (!connection_ok)
	{}
	if (connection_ok)
	{
		http_test();
	}

	// Connection
	// error = esp_wifi_connect();
	// if (error != ESP_OK)
	// {
	// 	ESP_LOGE("Connect wifi", "Erreur: %s\n", esp_err_to_name(error));
	// }
	ESP_LOGI("OK", "Tout se passe bien jusqu'ici");

}

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

// void http_test(void)
// {
// 	// Initialisation de notre connection http
// 	esp_http_client_config_t config = {
// 		.url = "http://20.103.43.247/prv/healthz",
// 	};
// 	esp_http_client_handle_t client = esp_http_client_init(&config);
// 	if (client == NULL)
// 	{
// 		ESP_LOGE("Initialisation", "Erreur lors de l'initialisation de la connection http");
// 	}
// 
// 	int error = esp_http_client_open(client, 0);
// 	if (error != ESP_OK)
// 	{
// 		ESP_LOGE("Connection", "Erreur: %s\n", esp_err_to_name(error));
// 	}
// 
// 
// 
// }




/*
int create_socket(const char * host, int port)
{
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	if (inet_pton(AF_INET, host, &address.sin_addr) != 1)
	{
		//fprintf(stderr, "Erreur\n");
		printf("ERREUR\n");
		return -1;
	}
	address.sin_port = htons(port);
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(sock, (struct sockaddr *) &address, sizeof(address)) != 0)
	{
		fprintf(stderr, "Erreur lors de la connection du socket: %s\n", strerror(errno));
		return -1;
	}
	return sock;
}

void prepare_adress(struct sockaddr_in * address, const char * host, int port)
{
	memset(address, 0, sizeof(struct sockaddr_in));
	address->sin_family = AF_INET;
	if (inet_pton(AF_INET, host, &address->sin_addr) != 1)
	{
		fprintf(stderr, "Erreur\n");
		exit(EXIT_FAILURE);
	}
	address->sin_port = htons(port);

}
*/
