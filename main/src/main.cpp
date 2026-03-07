#define MBEDTLS_ALLOW_PRIVATE_ACCESS 
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "SystemManager.hpp"
#include "OfflinePayment.hpp"

extern "C" void app_main() {
    nvs_flash_init();
    
    // 1. Instanciar dependencias
    OfflinePayment paymentProcessor;

    // 2. Instanciar y configurar el SystemManager
    SystemManager systemManager(&paymentProcessor);
    
    // 3. Inicializar hardware y dependencias
    systemManager.init();

    // 4. Bucle principal (bloqueante)
    systemManager.run();
}