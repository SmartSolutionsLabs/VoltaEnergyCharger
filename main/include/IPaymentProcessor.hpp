#ifndef I_PAYMENT_PROCESSOR_HPP
#define I_PAYMENT_PROCESSOR_HPP

#include <stdint.h>

enum class SignatureWorkStatus : uint16_t{
    IDLE = 1,
    PROCESSING = 2,
    DONE = 3,
    ERROR = 4 
};

class IPaymentProcessor {
public:
    virtual ~IPaymentProcessor() = default;

    /**
     * @brief Inicia el proceso de firma de forma asíncrona.
     * 
     * @param id ID de la terminal.
     * @param min Minutos solicitados.
     * @param price Precio calculado.
     * @param sig_out Puntero al buffer donde se escribirá la firma (64 bytes).
     * @param status_out Puntero al estado de trabajo que se actualizará cuando termine.
     */
    virtual void buildSignatureAsync(uint16_t id, uint16_t min, uint32_t price, uint8_t* sig_out, uint16_t* status_out) = 0;

    /**
     * @brief Obtiene el estado de trabajo actual del procesador.
     */
    virtual SignatureWorkStatus getWorkStatus() = 0;
};

#endif // I_PAYMENT_PROCESSOR_HPP
