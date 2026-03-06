#include "ControlBoardProxy.hpp"
#include "esp_log.h"

static const char* TAG = "ControlBoardProxy";

// 1. Definición del diccionario de parámetros (Mapa de registros)
// Asumimos un mapeo base para el esclavo; luego se pueden ajustar direcciones/terminales específicos.
const mb_parameter_descriptor_t ControlBoardProxy::m_deviceParameters[] = {
	// CID, Nombre, Unidad, Esclavo (por defecto), Tipo Modbus, Dirección, Longitud
	{ 0, (char*)"Status",      (char*)"min", 1, MB_PARAM_INPUT,   0x0000, 1, 0, PARAM_TYPE_U16, 2, {0}, PAR_PERMS_READ },
	{ 1, (char*)"Energy",      (char*)"W",   1, MB_PARAM_INPUT,   0x0001, 1, 0, PARAM_TYPE_U16, 2, {0}, PAR_PERMS_READ },
	{ 2, (char*)"StartCharge", (char*)"min", 1, MB_PARAM_HOLDING, 0x0002, 1, 0, PARAM_TYPE_U16, 2, {0}, PAR_PERMS_READ_WRITE }
};

const size_t ControlBoardProxy::m_numParameters = sizeof(m_deviceParameters) / sizeof(m_deviceParameters[0]);

ControlBoardProxy::ControlBoardProxy(uart_port_t uartNum, uint8_t slaveAddr)
	: m_uart(uartNum), m_slaveAddr(slaveAddr), m_masterHandle(nullptr) {}

ControlBoardProxy::~ControlBoardProxy() {
	stop();
	if (m_masterHandle) {
		mbc_master_delete(m_masterHandle);
		m_masterHandle = nullptr;
	}
}

esp_err_t ControlBoardProxy::init(int txPin, int rxPin, uint32_t baudrate) {
	// Configuración de comunicación basada en el ejemplo serial_master.c
	mb_communication_info_t comm = {};
	comm.ser_opts.port = this->m_uart;
	comm.ser_opts.mode = MB_RTU;
	comm.ser_opts.baudrate = baudrate;
	comm.ser_opts.parity = MB_PARITY_NONE;
	comm.ser_opts.uid = 0; // UID 0 para maestro
	comm.ser_opts.response_tout_ms = 1000;
	comm.ser_opts.data_bits = UART_DATA_8_BITS;
	comm.ser_opts.stop_bits = UART_STOP_BITS_1;

	// Inicializar controlador maestro
	esp_err_t err = mbc_master_create_serial(&comm, &m_masterHandle);
	if (err != ESP_OK) return err;

	// Configurar pines UART
	err = uart_set_pin(m_uart, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	if (err != ESP_OK) return err;

	// Configurar modo UART básico
	err = uart_set_mode(m_uart, UART_MODE_UART);
	if (err != ESP_OK) return err;

	// Registrar el diccionario de parámetros
	err = mbc_master_set_descriptor(m_masterHandle, m_deviceParameters, m_numParameters);
	if (err != ESP_OK) return err;

	return ESP_OK;
}

uint16_t ControlBoardProxy::readStatus(uint8_t terminalId) {
	uint16_t remaining_minutes = 0;
	uint8_t type = 0;
	// En una implementación final se usarán CIDs para cada terminal o mbc_master_send_request()
	// Aquí usamos el CID 0 (Status) del diccionario para efectos de demostración según la petición
	esp_err_t err = mbc_master_get_parameter(m_masterHandle, 0, (uint8_t*)&remaining_minutes, &type);
    
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error al leer Status (terminal ID %d): %s", terminalId, esp_err_to_name(err));
		return 0;
	}
	return remaining_minutes;
}

uint16_t ControlBoardProxy::readEnergy(uint8_t terminalId) {
	uint16_t watts = 0;
	uint8_t type = 0;
	esp_err_t err = mbc_master_get_parameter(m_masterHandle, 1, (uint8_t*)&watts, &type);
    
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error al leer Energy (terminal ID %d): %s", terminalId, esp_err_to_name(err));
		return 0;
	}
	return watts;
}

esp_err_t ControlBoardProxy::sendStartCharge(uint8_t terminalId, uint16_t minutes) {
	uint8_t type = 0;
	// Escribir el valor de minutos en el Holding Register definido (CID 2)
	esp_err_t err = mbc_master_set_parameter(m_masterHandle, 2, (uint8_t*)&minutes, &type);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error enviando StartCharge (terminal ID %d): %s", terminalId, esp_err_to_name(err));
	}
	return err;
}
