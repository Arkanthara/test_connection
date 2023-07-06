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
	connect_to_wifi();
	// sleep(5000);
	// int sock = create_socket("192.168.0.97", 27015);
	// if (sock == -1)
	// {
	// 	exit(FEXIT_FAILURE);
	// }
	// write(sock, "Coucou", 8);
	ESP_LOGI("FIN", "On est arrivé à la fin du programme !");

}
