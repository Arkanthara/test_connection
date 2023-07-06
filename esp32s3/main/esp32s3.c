#include <stdio.h>
#include "connect.h"
#include <unistd.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_log.h"

void app_main(void)
{
	// Initialisation mémoire non volatile NVS
	int error = nvs_flash_init();
	if (error != ESP_OK)
	{
		ESP_LOGE("Initialisation mémoire nvs", "Erreur: %s", esp_err_to_name(error));
	}

	// Appel de la fonction créée dans le fichier connect.c
	connect_to_wifi();
	
	// Message indiquant qu'on a tout fini
	ESP_LOGI("FIN", "On est arrivé à la fin du programme !");

}
