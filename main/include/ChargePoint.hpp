#pragma once
#include <stdint.h>

/**
 * @brief Estado de un punto de carga.
 */
enum class ChargePointStatus : uint8_t {
	AVAILABLE,      ///< Libre y listo para usar
	OCCUPIED,       ///< En uso
	FAULT,          ///< Error de hardware
	OUT_OF_SERVICE  ///< Fuera de servicio / deshabilitado
};

/**
 * @brief Modelo de dominio de un punto de carga.
 *
 * Encapsula el estado y la lógica de selección de tiempo de un
 * punto de carga individual. No tiene dependencias de LVGL ni ESP-IDF.
 *
 * Reglas de tiempo:
 *  - Paso fijo: 15 minutos
 *  - Mínimo    : 15 minutos
 *  - Máximo    : 120 minutos (2 horas)
 */
class ChargePoint {
public:
	explicit ChargePoint(uint8_t id);

	// --- Identidad y estado ---
	uint8_t           getId()     const;
	ChargePointStatus getStatus() const;
	void              setStatus(ChargePointStatus s);
	bool              isAvailable() const;

	// --- Selección de tiempo ---
	/**
	 * @brief Incrementa el tiempo seleccionado en 15 minutos.
	 * @return true si el incremento fue posible, false si ya estaba al máximo.
	 */
	bool incrementTime();

	/**
	 * @brief Decrementa el tiempo seleccionado en 15 minutos.
	 * @return true si el decremento fue posible, false si ya estaba al mínimo.
	 */
	bool decrementTime();

	uint16_t getSelectedMinutes() const;

	/**
	 * @brief Resetea el tiempo seleccionado al valor mínimo (15 min).
	 */
	void resetTime();

private:
	static constexpr uint16_t TIME_STEP_MIN = 15;
	static constexpr uint16_t TIME_MIN_MIN  = 15;
	static constexpr uint16_t TIME_MAX_MIN  = 120;

	uint8_t           m_id;
	ChargePointStatus m_status;
	uint16_t          m_selectedMinutes;
};
