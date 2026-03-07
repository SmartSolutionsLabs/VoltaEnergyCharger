#ifndef CONTROL_BOARD_PROXY_HPP
#define CONTROL_BOARD_PROXY_HPP

#include "esp_err.h"
#include "driver/uart.h"
#include "mbcontroller.h" // ESP-Modbus controller

/**
 * @brief Proxy para la comunicación con la placa controladora.
 *
 * Responsabilidades:
 *  - Inicializar la comunicación RS232 (UART1)
 *  - Enviar comandos de inicio de carga
 *  - Leer el estado de los terminales
 */
class ControlBoardProxy {
public:
	ControlBoardProxy(uart_port_t uartNum, uint8_t slaveAddr);
	~ControlBoardProxy();

	// Gestión del Ciclo de Vida
	esp_err_t init(int txPin, int rxPin, uint32_t baudrate);
	esp_err_t inline start() { return mbc_master_start(m_masterHandle); };
	esp_err_t inline stop() { return mbc_master_stop(m_masterHandle); };

	// Comandos de Acción (Escritura)
	esp_err_t sendStartCharge(uint8_t terminalId, uint16_t minutes);

	// Monitoreo (Lectura)
	uint16_t readStatus(uint8_t terminalId);
	uint16_t readEnergy(uint8_t terminalId);

private:
	uart_port_t m_uart;
	uint8_t m_slaveAddr;
	void* m_masterHandle;

	// Diccionario de parámetros para esp-modbus
	static const mb_parameter_descriptor_t m_deviceParameters[];
	static const size_t m_numParameters;
};

#endif // CONTROL_BOARD_PROXY_HPP
