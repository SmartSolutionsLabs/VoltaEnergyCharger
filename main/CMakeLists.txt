idf_component_register(
    SRCS 
        "src/main.cpp"
        "src/ChargePoint.cpp"
        "src/ControlBoardProxy.cpp"
        "src/ModbusSlave.cpp"
        "src/MCP23017.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES espressif__esp-modbus driver mbedtls esp_timer efuse nvs_flash
)