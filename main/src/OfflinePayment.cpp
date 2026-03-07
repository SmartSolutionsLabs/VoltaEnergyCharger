#include "OfflinePayment.hpp"


OfflinePayment::OfflinePayment() {
    
}

OfflinePayment::~OfflinePayment() {
    
}   


void OfflinePayment::buildSignatureAsync(uint16_t id, uint16_t min, uint32_t price, uint8_t* sig_out, uint16_t* status_out) {
    if (done_flag) *status_out = false;

    // Empaquetamos los argumentos
    TaskArgs* args = new TaskArgs{
        .instance = this,
        .id = id,
        .min = min,
        .price = price,
        .sig_out = sig_out,
        .done_flag = status_out
    };

    // Lanzamos la tarea de FreeRTOS
    xTaskCreate(
        OfflinePayment::internalTaskWrapper, 
        "PayAsync", 
        8192,         // Stack aumentado para mbedtls segura
        args, 
        5, 
        NULL
    );
}

void OfflinePayment::internalTaskWrapper(void* pvParameters) {
    TaskArgs* args = (TaskArgs*)pvParameters;
    this->work_status = SignatureWorkStatus::PROCESSING;
    args->work_status = this->work_status;
    // Llamamos al método original de la instancia
    bool success = args->instance->buildSignature(args->id, args->min, args->price, args->sig_out); // 350 ms

    // Si hay una bandera de "listo", la activamos
    if(success){
        this->work_status = SignatureWorkStatus::DONE;
        args->work_status = this->work_status;
    }
    else{
        this->work_status = SignatureWorkStatus::ERROR;
        args->work_status = this->work_status;
    }

    // Limpieza de memoria y eliminación de la tarea
    delete args;
    vTaskDelete(NULL);
}

// Función de Firma Real (Sin simulaciones)
bool OfflinePayment::buildSignature(uint16_t id, uint16_t min, uint32_t price, uint8_t* sig_out) {
    mbedtls_ecdsa_context ecdsa;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ecdsa_init(&ecdsa);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"v_rng", 5);
    
    mbedtls_ecp_group_load(&ecdsa.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    mbedtls_mpi_read_binary(&ecdsa.MBEDTLS_PRIVATE(d), this->LLAVE_MAESTRA, 32);
    
    uint8_t hash[32];
    mbedtls_sha256((const unsigned char*)this->IDENTIDAD, strlen(this->IDENTIDAD), hash, 0);

    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r); mbedtls_mpi_init(&s);
    int ret = mbedtls_ecdsa_sign(&ecdsa.MBEDTLS_PRIVATE(grp), &r, &s, &ecdsa.MBEDTLS_PRIVATE(d), hash, 32, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret == 0) {
        mbedtls_mpi_write_binary(&r, sig_out, 32);
        mbedtls_mpi_write_binary(&s, sig_out + 32, 32);
    }
    mbedtls_ecdsa_free(&ecdsa); 
    mbedtls_ctr_drbg_free(&ctr_drbg); 
    mbedtls_entropy_free(&entropy);
    return (ret == 0);
}

bool OfflinePayment::getOTP(uint16_t id, uint16_t min, uint32_t price, uint8_t* otp_out) {
    // 1. Construir el payload exacto que usa el maestro
    uint32_t otp = esp_random() % 1000000;
    memcpy(otp_out, &otp, sizeof(otp));

    return true;
}

SignatureWorkStatus OfflinePayment::getWorkStatus(){
    return this->work_status;
}