#include "db.h"
#include <fstream>


static Logger logger("database", "database.log");

DB::DB(const std::string& filename) : db_filename(filename) {
    std::ifstream db(filename);
    if (!db) {
        logger.log("Can not open ", filename);
        try {
            logger.log("Trying to create ", filename);
            std::ofstream(filename).close();
            logger.log(filename, " created successfully");
        }
        catch (...) {
            logger.log("Can not create database file");
            std::cout << "Can not create database file" << std::endl;
        }
    }

    if (db.is_open()) {
        logger.log("Database was opened");

        std::string user;
        long long hash;
        while (db >> user >> hash) {

            db >> user >> hash;
            if (!user.empty()) {
                users.insert({user, hash});
            }
        }
        logger.log("Database was loaded");
        db.close();
    }
}

DB::~DB() {
    save();
}

bool DB::add_record(const std::string& username, long long hash) {
    if (users.find(username) != users.end()) {
        logger.log("User already exist");
        return false;
    }
    users[username] = hash;
    return true;
}

bool DB::ask_existance(const std::string& username, long long hash) {
    if (users.find(username) != users.end() && users[username] == hash) {
        logger.log("User exist");
        return true;
    }
    return false;
}

void DB::save() {
    std::ofstream db(db_filename);
    if (!db) {
        logger.log("Database can not be opened");
        return;
    }
    for (const auto& [user, password] : users) {
        db << user << " " << password << '\n';
    }
    logger.log("Database saved");
    db.close();
}

