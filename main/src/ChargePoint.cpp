#include "ChargePoint.hpp"

ChargePoint::ChargePoint(uint8_t id)
	: m_id(id)
	, m_status(ChargePointStatus::AVAILABLE)
	, m_selectedMinutes(TIME_MIN_MIN)
{}

uint8_t ChargePoint::getId() const { return m_id; }

ChargePointStatus ChargePoint::getStatus() const { return m_status; }

void ChargePoint::setStatus(ChargePointStatus s) { m_status = s; }

bool ChargePoint::isAvailable() const {
	return m_status == ChargePointStatus::AVAILABLE;
}

bool ChargePoint::incrementTime() {
	if (m_selectedMinutes >= TIME_MAX_MIN) return false;
	m_selectedMinutes += TIME_STEP_MIN;
	return true;
}

bool ChargePoint::decrementTime() {
	if (m_selectedMinutes <= TIME_MIN_MIN) return false;
	m_selectedMinutes -= TIME_STEP_MIN;
	return true;
}

uint16_t ChargePoint::getSelectedMinutes() const { return m_selectedMinutes; }

void ChargePoint::resetTime() { m_selectedMinutes = TIME_MIN_MIN; }
