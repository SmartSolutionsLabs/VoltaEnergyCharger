#ifndef OFFLINE_PAYMENT_HPP
#define OFFLINE_PAYMENT_HPP

// Criptografía
#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "IPaymentProcessor.hpp"

class OfflinePayment : public IPaymentProcessor {
private:
    static inline uint8_t LLAVE_MAESTRA[32] = { 
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38 
    };

    static constexpr const char* IDENTIDAD = "VOLTA_CHG_001";

    volatile SignatureWorkStatus work_status{SignatureWorkStatus::IDLE}; //
    volatile uint8_t signature[64] = {0};
    
    static void internalTaskWrapper(void* pvParameters);

public:
     OfflinePayment();
    ~OfflinePayment() override;
    bool buildSignature(uint16_t id, uint16_t min, uint32_t price, uint8_t* sig_out);
    bool getOTP(uint16_t id, uint16_t min, uint32_t price, uint8_t* otp_out);

    void buildSignatureAsync(uint16_t id, uint16_t min, uint32_t price, uint8_t* sig_out, uint16_t* status_out) override;
    SignatureWorkStatus getWorkStatus() override;


};

#endif