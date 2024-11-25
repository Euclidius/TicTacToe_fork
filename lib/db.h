//
// Created by cezar on 08.10.24.
//

#ifndef TICTACTOE_DBINTERFACE_H
#define TICTACTOE_DBINTERFACE_H

#include <string_view>
#include "logger.hpp"
#include <iostream>
#include <map>

class DB {
    std::map<std::string, long long> users;
    std::string db_filename;
public:
    DB(const std::string& filename);
    [[nodiscard]] bool add_record(const std::string& username, long long hash);
    [[nodiscard]] bool ask_existance(const std::string& username, long long hash);
    void save();
    ~DB();
};

#endif //TICTACTOE_DBINTERFACE_H
