#include "CommandManager.hpp"
#include <sstream>

std::map<std::string, std::unique_ptr<Command>> CommandManager::_commands;

void CommandManager::addCommand(const std::string& name, std::unique_ptr<Command> cmd) {
    _commands[name] = std::move(cmd);
}

std::string CommandManager::run(std::string input) {
    sanitize(input);
    if (input.empty()) return "";

    auto parts = split(input, '.'); // Separador por puntos
    if (parts.empty()) return "ERROR: Empty input";

    std::string name = parts[0];
    if (_commands.count(name)) {
        return _commands[name]->execute(parts); // Aquí ocurre el polimorfismo
    }
    return "ERROR: Command '" + name + "' not found";
}

std::vector<std::string> CommandManager::split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) tokens.push_back(token);
    return tokens;
}

void CommandManager::sanitize(std::string &s) {
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
}