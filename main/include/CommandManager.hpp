#ifndef COMMAND_MANAGER_HPP
#define COMMAND_MANAGER_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>

/**
 * @brief Interfaz abstracta (Clase Base).
 * Cualquier comando nuevo debe heredar de aquí.
 */
class Command {
public:
    virtual ~Command() {}
    // El método que cada comando implementará con su propia lógica
    virtual std::string execute(const std::vector<std::string>& args) = 0;
};

class CommandManager {
public:
    /**
     * @brief Registra un comando.
     * @param name Palabra clave (ej: "charge")
     * @param cmd Instancia de la clase que ejecuta la lógica.
     */
    static void addCommand(const std::string& name, std::unique_ptr<Command> cmd);
    
    /**
     * @brief Procesa texto desde cualquier canal (Serial, Web, BT).
     */
    static std::string run(std::string input);

private:
    static std::map<std::string, std::unique_ptr<Command>> _commands;
    static std::vector<std::string> split(const std::string& s, char delimiter);
    static void sanitize(std::string &s);
};

#endif