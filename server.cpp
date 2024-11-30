#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <map>
#include <thread>
#include <fstream>
#include <algorithm>

#include "lib/db.h"
#include "lib/logger.hpp"
#include "lib/ini.h"


const int PORT = 2023;
const int BUF_SZ = 1024;


static Logger logger("server", "server.log");

std::vector <std::string> logged_users;

std::vector<std::exception_ptr> exceptions;

struct userconf {
    sockaddr_in addr;
    std::string opponent_username;
    char player_symbol;
};

std::map <std::string, userconf> clients;


int create_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        logger.log("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    int res = bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr));
    if (res < 0) {
        perror("bind failed");
        logger.log("Bind failed");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

/* Функция для разбиения сообщения на отдельные слова */
std::vector <std::string> split_buffer(const std::string &in) {
    std::string tmp;
    std::vector <std::string> words;
    for (const auto &c: in) {
        if (c == ' ') {
            if (!tmp.empty()) {
                words.push_back(tmp);
            }
            tmp.clear();
        }
        else {
            tmp += c;
        }
    }
    if (!tmp.empty()) {
        words.push_back(tmp);
    }
    logger.log("Buffer parsed successfully.");
    return words;
}

/* Отключение игроков от игры, но не от сервера, после окончания игры */
void disable_user_after_the_game(DB &db, const std::string &username) {
    db.save();
    std::string opponent_username = clients[username].opponent_username;
    if (clients.find(username) != clients.end()) {
        clients.erase(username);
        //logger.log(username + " erased");
        logger.log(username, " erased");
    }
    if (clients.find(opponent_username) != clients.end()) {
        clients.erase(opponent_username);
        logger.log(opponent_username + " erased");
    }
    logger.log("Clients size: ", std::to_string(clients.size()));

    std::vector<std::string>::iterator pos = std::find(logged_users.begin(), logged_users.end(), username);

    if (pos != logged_users.end()) {
        logged_users.erase(pos);
    }

    std::vector<std::string>::iterator pos_opponent = std::find(logged_users.begin(), logged_users.end(), opponent_username);
    if (pos_opponent != logged_users.end()) {
        logged_users.erase(pos_opponent);
    }
}

void send_message(int socket, sockaddr_in cliaddr, const std::string &msg) {
    logger.log("Sending message: ", msg);
    logger.log("Address: ", std::to_string(cliaddr.sin_addr.s_addr));
    sendto(socket, msg.c_str(), msg.size(), 0, (const struct sockaddr *) &cliaddr, sizeof(cliaddr));
}


void init_game_for_both_users(int socket, const std::string &first_player_, const std::string &second_player_) {
    srand(time(0));
    int first_player_move = rand() % 2;
    const std::string &x_player_username = first_player_move == 0 ? first_player_ : second_player_;
    const std::string &o_player_username = first_player_move == 1 ? first_player_ : second_player_;

    clients[x_player_username].player_symbol = 'X';
    clients[x_player_username].opponent_username = o_player_username;

    clients[o_player_username].player_symbol = 'O';
    clients[o_player_username].opponent_username = x_player_username;

    /* F - первый ход, X / O - каждый игрок узнаёт за кого он играет */
    send_message(socket, clients[x_player_username].addr, "FX");
    send_message(socket, clients[o_player_username].addr, "FO");
}

void process_client_message(DB &db, int move_time, int socket, sockaddr_in cliaddr, const std::string &message) {
    try {
        std::vector <std::string> splitted_buffer = split_buffer(message);
        if (splitted_buffer[0] == "GET") {
            /* Отправляем клиенту время хода из конфигурационного файла */
            send_message(socket, cliaddr, std::to_string(move_time));
        }
        else if (splitted_buffer.size() == 2) {
            if (splitted_buffer[0] == "NEWGAME") {
                if (logged_users.empty() || (!logged_users.empty() && (logged_users[0] != splitted_buffer[1]))) {
                    clients[splitted_buffer[1]].addr = cliaddr;
                    logged_users.push_back(splitted_buffer[1]);
                    logger.log("Clients size: ", std::to_string(clients.size()));
                    if (clients.size() % 2 == 0 && !clients.empty()) {
                        init_game_for_both_users(socket, logged_users[logged_users.size() - 2], logged_users.back());
                    }
                    else {
                        send_message(socket, cliaddr, "Waiting user."); // Второй игрок еще не подключился к игре
                    }
                }
                else {
                    //Этот игрок уже в игре, нельзя подключить его второй раз.
                    send_message(socket, cliaddr, "USER_ALREADY_ONLINE");
                }
            }
        }
        else if (splitted_buffer.size() == 3) {
            if (splitted_buffer[0] == "SIGNUP") {
                bool result = db.add_record(splitted_buffer[1], std::stoll(splitted_buffer[2]));
                if (result) {
                    send_message(socket, cliaddr, "Registered successfully");
                    send_message(socket, cliaddr, "Waiting user.");
                    clients[splitted_buffer[1]].addr = cliaddr;
                    logged_users.push_back(splitted_buffer[1]);
                    if (clients.size() % 2 == 0 && !clients.empty()) {
                        init_game_for_both_users(socket, logged_users[logged_users.size() - 2], logged_users.back());
                    }
                }
                else {
                    std::string msg = "USER_ALREADY_EXISTS";
                    send_message(socket, cliaddr, msg);
                }
            }
            else if (splitted_buffer[0] == "SIGNIN") {
                bool result = db.ask_existance(splitted_buffer[1], std::stoll(splitted_buffer[2]));
                if (result) {
                    if (logged_users.empty() || (!logged_users.empty() && logged_users[0] != splitted_buffer[1])) {
                        send_message(socket, cliaddr, "Logged in successfully");
                        send_message(socket, cliaddr, "Waiting user.");
                        clients[splitted_buffer[1]].addr = cliaddr;
                        logged_users.push_back(splitted_buffer[1]);
                        if (clients.size() % 2 == 0 && !clients.empty()) {
                            init_game_for_both_users(socket, logged_users[logged_users.size() - 2], logged_users.back());
                        }
                    }
                    else {
                        //Если этот игрок уже в игре, то не даем ему снова войти
                        send_message(socket, cliaddr, "USER_ALREADY_ONLINE");
                    }
                }
                else {
                    logger.log("USER_NOT_FOUND");
                    send_message(socket, cliaddr, "USER_NOT_FOUND");
                }
            }
            else if (splitted_buffer[0] == "END") {
                int result = splitted_buffer[1][0] - '0';

                char player = splitted_buffer[2].back();
                splitted_buffer[2].pop_back();

                std::string username = splitted_buffer[2];

                sockaddr_in x_player_addr = (player == 'X' ? clients[username].addr : clients[clients[username].opponent_username].addr);
                sockaddr_in o_player_addr = (player == 'O' ? clients[username].addr : clients[clients[username].opponent_username].addr);

                if (result == 1) { // X - won
                    send_message(socket, x_player_addr, "Won");
                    send_message(socket, o_player_addr, "Lost");
                }
                else if (result == 2) { // O - won
                    send_message(socket, o_player_addr, "Won");
                    send_message(socket, x_player_addr, "Lost");
                }
                else if (result == 3) { // Draw
                    send_message(socket, x_player_addr, "Draw");
                    send_message(socket, o_player_addr, "Draw");
                }
                else if (result == 4) { // X - lost by time
                    send_message(socket, o_player_addr, "WonBT");
                    send_message(socket, x_player_addr, "LostBT");
                }
                else if (result == 5) { // O - lost by time
                    send_message(socket, x_player_addr, "WonBT");
                    send_message(socket, o_player_addr, "LostBT");
                } // BT - by time
                disable_user_after_the_game(db, username);
                logger.log("Disabled users");
            }
        }
        else if (splitted_buffer.size() == 5 && splitted_buffer[0] == "BOARD") { //back : client.username
            if (splitted_buffer[1] == "X") { //X done his move
                send_message(socket, clients[clients[splitted_buffer.back()].opponent_username].addr, splitted_buffer[2] + " " + splitted_buffer[3] + "O");
            }
            else if (splitted_buffer[1] == "O") { //O done his move
                send_message(socket, clients[clients[splitted_buffer.back()].opponent_username].addr, splitted_buffer[2] + " " + splitted_buffer[3] + "X");
            }
        }
        else {
            send_message(socket, cliaddr, "Invalid command");
        }
    }
    catch (...) {
        exceptions.push_back(std::current_exception());
    }
}

void stream_incoming_messages(DB &db, int move_time, int socket) {
    try {
        char buffer[BUF_SZ];
        sockaddr_in cliaddr;
        socklen_t len = sizeof(cliaddr);

        while (true) {
            int n = recvfrom(socket, buffer, BUF_SZ, 0, (struct sockaddr *) &cliaddr, &len);
            if (n < 0) {
                perror("recvfrom failed");
                continue;
            }

            buffer[n] = '\0';
            std::string message(buffer);
            logger.log("Message from client: ", message);
            std::thread process(process_client_message, std::ref(db), move_time, socket, cliaddr, message);
            process.detach();
        }
    }
    catch (...) {
        exceptions.push_back(std::current_exception());
    }
}

int main(int argc, char** argv) {
    std::string config_file;

    if (argc < 2) {
        std::cerr << "Usage: " + std::string(argv[0]) + "<path/to/config_file";
        return 1;
    } else {
        config_file = argv[1];
    }
    try {
        /* Чтение конфигурационного файла */
        mINI::INIFile file(config_file);
        mINI::INIStructure ini;
        file.read(ini);
        std::string &address = ini["game"]["move_time"];
        int move_time = std::stoi(address);

        DB db(ini["game"]["database_file"]);
        int socket = create_socket();
        logger.log("Server is running...");

        std::thread listener(stream_incoming_messages, std::ref(db), move_time, socket);
        listener.join();
        for (auto exception : exceptions) {
            try {
                std::rethrow_exception(exception);
            }
            catch (const std::exception& e) {
                logger.log(e.what());
            }
            catch (...) {
                logger.log("Unknown error");
            }
        }
    }
    /* Обработка исключений основного потока */
    catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "Unexpected error" << std::endl;
    }
    return 0;
}
