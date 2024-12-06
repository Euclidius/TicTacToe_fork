#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <fstream>
#include <variant>
#include <filesystem>
#include "lib/logger.hpp"
#include "lib/encrypt.hpp"
#include "lib/TSQueue.h"
#include "lib/ini.h"
#include "lib/game.hpp"


const int PORT = 2023;
const int BUF_SZ = 1024;

static Logger logger("client", "client.log");

int SECONDS_PER_MOVE = 20; // Значение по умолчанию. Может быть изменено через конфигурационный файл.

TSQueue<std::string> message_queue; // Thread-safe message queue

TicTacToe game;
bool disable_menu = false;

/* Нужен для того, чтобы завершить все потоки, если нужно выйти из программы */
bool stop_flag = false;

std::vector<std::exception_ptr> exceptions;

struct clientConfiguration {
    char player_symbol = '-';
    std::string username;
} static client_configuration;


void sign_in(int sockfd, sockaddr_in servaddr);

void sign_up(int sockfd, sockaddr_in servaddr);

void menu(int sockfd, sockaddr_in servaddr);


void send_to_server(int sockfd, sockaddr_in servaddr, const std::string &msg_to_send) {
    socklen_t len = sizeof(servaddr);

    sendto(sockfd, msg_to_send.c_str(), msg_to_send.size(), 0, reinterpret_cast<sockaddr *>(&servaddr), len);
    logger.log("Message sent to server: ", msg_to_send);
}


void handle_server_responses(int sockfd, sockaddr_in servaddr) {
    try {
        char buffer[BUF_SZ];
        socklen_t len = sizeof(servaddr);


        struct timeval timeout;
        timeout.tv_sec = 2; // seconds
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof(timeout));


        while (!stop_flag) {
            int n = recvfrom(sockfd, buffer, BUF_SZ, 0, reinterpret_cast<sockaddr *>(&servaddr), &len);
            if (n < 0) {
                continue;
            }

            buffer[n] = '\0';
            std::string message(buffer);
            logger.log("Message from server: ", message);
            message_queue.push(message); // Помещаем сообщение в очередь
        }
    }
    catch (...) {
        exceptions.push_back(std::current_exception());
    }
}

bool is_input_available(int timeout_seconds) {
    fd_set readfds;
    struct timeval tv;

    // Ожидаем ввода с stdin (стандартный ввод)
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    // Устанавливаем тайм-аут
    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;

    // Проверяем доступность ввода
    int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);

    return ret > 0;  // Если вернулся положительный результат, значит, ввод доступен
}

std::variant<int, std::pair<int, int>> input(int flag) {
    logger.log("Input flag: ", flag);
    if (flag == 0) { // Ввод для menu()
        int s;
        std::cin >> s;
        while (!std::cin.good() || s < 1 || s > 3) {
            logger.log("Input: ", s);
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Wrong input. Please try again." << std::endl;
            std::cout << "Enter the number to select an option: ";
            std::cin >> s;
        }
        return s;
    }
    else if (flag == 1) { //Ввод для хода.
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        int x;
        int y;
        bool is_move_done = false;
        std::cout << "You have " << SECONDS_PER_MOVE << " seconds to make a move" << std::endl;
        std::cout << "Cell: " << std::flush;
        if (is_input_available(SECONDS_PER_MOVE)) { // Проверка: сделан ли ход вовремя.
            std::cin >> x >> y;
            while (!std::cin.good() || x < 1 || x > 3 || y < 1 || y > 3 || game.is_cell_taken(x - 1, y - 1)) {
                logger.log("Entered: ", x, " ", y);
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Row and column must be in range [1, 3] and cell must be empty" << std::endl;
                std::cout << "Enter the correct cell: " << std::flush;
                std::cin >> x >> y;
            }
            is_move_done = true;
        }

        if (!is_move_done) {
            x = -1;
            y = -1;
            logger.log("Time for move is up");
        }
        return std::make_pair(x, y);

    }
    else if (flag == 2) { // Обработка ввода для состояния после игры.
        int x;
        std::cin >> x;
        while (!std::cin.good() && x != 1 && x != 2) {
            logger.log("Entered: ", x);
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Wrong input. Enter 1 or 2: ";
            std::cin >> x;
        }
        return x;
    }
}


void after_the_game_handler(int sockfd, sockaddr_in servaddr) { //Обработка состояния после игры
    game.clear();
    client_configuration.player_symbol = '-';
    std::cout << "1. New game\n2. Exit" << std::endl;
    std::cout << "Choose the option: ";
    int event = std::get<int>(input(2));
    if (event == 1) {
        logger.log("Starting new game...");
        send_to_server(sockfd, servaddr, "NEWGAME " + client_configuration.username);
    }
    else {
        logger.log("Exiting after the game");
        message_queue.push("EXIT");
        stop_flag = true;
    }
}

/* Кладём X - X делает ход
 * Кладём O - O делает ход
 * */
void make_move(int sockfd, sockaddr_in servaddr, char flag) {

    std::pair < int, int > move_pair = std::get < std::pair < int, int >> (input(1));
    /* Уменьшение координаты на 1 т.к. для пользователя ввод клетки от 1 до 3, а не от 0 до 2*/
    int x = move_pair.first - 1;
    int y = move_pair.second - 1;

    /* Ход не сделан вовремя */
    if (x == -2 && y == -2) {
        std::cout << "Time is up. You lost by time" << std::endl;
        send_to_server(sockfd, servaddr,
                       std::string(client_configuration.player_symbol == 'X' ? "END 4" : "END 5") + \
                       " " + client_configuration.username + client_configuration.player_symbol
        );
    }
    else {
        game.push_move(x, y, flag);
        game.draw_board();
        //int game_status = game.game_status();
        std::string msg = "BOARD " + std::string(1, flag) + " " + std::to_string(x) + " " + std::to_string(y) + " " + client_configuration.username;
        send_to_server(sockfd, servaddr, msg);
        std::cout << "Waiting for opponent!" << std::endl;
    }
}



/* Функция обработки сообщений */
void process_messages(int sockfd, sockaddr_in servaddr) {
    try {
        while (!stop_flag) {
            std::string message = message_queue.pop(); // Получение сообщения из очереди
            /* Первый ход для 'X' */
            if (message == "FX") {
                client_configuration.player_symbol = 'X';
                std::cout << "You are playing as X! Write coordinates of row and col. Example: 1 1" << std::endl;
                std::cout << "Board:" << std::endl;
                game.draw_board();
                make_move(sockfd, servaddr, 'X');
            }
            /* Первый ход для 'O' */
            else if (message == "FO") {
                client_configuration.player_symbol = 'O';
                std::cout << "You are playing as O! Waiting for opponent!" << std::endl;
            }

            /* Ход для 'X' */
            else if (message.back() == 'X') {
                /* Установить поле (msg[0], msg[2]) = 'O' */
                game.push_move(message[0] - '0', message[2] - '0', 'O');
                int game_status_ = game.game_status();
                if (game_status_ == 0) {
                    client_configuration.player_symbol = 'X';
                    std::cout << "You are playing as X! Write coordinates of row and col. Example: 1 1" << std::endl;
                    std::cout << "Board:" << std::endl;
                    game.draw_board();
                    make_move(sockfd, servaddr, 'X');
                }

                    /* Сообщение серверу об окончании игры */
                else {
                    send_to_server(sockfd, servaddr, "END " + std::to_string(game_status_) + " " + client_configuration.username + client_configuration.player_symbol);
                }
            }

            /* Ход для 'O' */
            else if (message.back() == 'O') {
                /* Установить поле (msg[0], msg[2]) = 'X' */
                game.push_move(message[0] - '0', message[2] - '0', 'X');
                int game_status_ = game.game_status();
                if (game_status_ == 0) {
                    std::cout << "Board:" << std::endl;
                    game.draw_board();
                    std::cout << "Write coordinates of row and col. Example: 1 1" << std::endl;
                    make_move(sockfd, servaddr, 'O');
                }
                else {
                    send_to_server(sockfd, servaddr, "END " + std::to_string(game_status_) + " " + client_configuration.username + client_configuration.player_symbol);
                }
            }
            else if (message == "Waiting user.") {
                logger.log("Waiting for the second user...");
                std::cout << "Waiting for the second user..." << std::endl;
            }

            /* Обработка сценариев конца игры */
            else if (message == "Won") {
                logger.log("Client won");
                std::cout << "Congratulations! You won!" << std::endl;
                after_the_game_handler(sockfd, servaddr);
            }
            else if (message == "Lost") {
                logger.log("Client lost");
                game.draw_board();
                std::cout << "You lost!" << std::endl;
                after_the_game_handler(sockfd, servaddr);
            }
            else if (message == "Draw") {
                logger.log("Draw");
                game.draw_board();
                std::cout << "Draw" << std::endl;
                after_the_game_handler(sockfd, servaddr);
            }
            else if (message == "WonBT") {
                logger.log("Won by time");
                std::cout << "Congratulations! You won by time!" << std::endl;
                game.draw_board();
                after_the_game_handler(sockfd, servaddr);
            }
            else if (message == "LostBT") {
                logger.log("Lost by time");
                std::cout << "You lost by time!" << std::endl;
                game.draw_board();
                after_the_game_handler(sockfd, servaddr);
            }
            else if (message == "USER_ALREADY_EXISTS") {
                logger.log("Repeated username");
                std::cout << "Username already exists" << std::endl;
                std::cout << "Enter another username." << std::endl;
                sign_up(sockfd, servaddr);
            }
            else if (message == "USER_ALREADY_ONLINE") {
                logger.log("User already online");
                std::cout << "This user now in game, you can't join with this username" << std::endl;
                std::cout << "1. Enter another username\n"
                             "2. Back to menu" << std::endl;
                std::cout << "Your choice: ";
                int x = std::get<int>(input(2));
                if (x == 1) {
                    sign_in(sockfd, servaddr);
                }
                else if (x == 2) {
                    disable_menu = false;
                    menu(sockfd, servaddr);
                }

            }
            else if (message == "USER_NOT_FOUND") {
                logger.log("User not found.");
                std::cout << "User not found" << std::endl;
                std::cout << "1. Enter another username\n"
                             "2. Back to menu" << std::endl;
                std::cout << "Your choice: ";
                int x = std::get<int>(input(2));
                if (x == 1) {
                    sign_in(sockfd, servaddr);
                }
                else if (x == 2) {
                    disable_menu = false;
                    menu(sockfd, servaddr);
                }
            }
            /* Выход из игры. Команда получена от menu() */
            else if (message == "EXIT") {
                std::cout << "Exiting..." << std::endl;
                logger.log("Exiting...");
                break;
            }
            else {
                std::cout << message << std::endl;
            }
        }
    }
    catch (...) {
        exceptions.push_back(std::current_exception());
    }
}


/* Вход в учётную запись */
void sign_in(int sockfd, sockaddr_in servaddr) {
    std::cout << "Username: ";
    std::cin >> client_configuration.username;
    logger.log("Username \"", client_configuration.username, "\" was entered");

    std::cout << "Password: ";
    std::string password;
    std::cin >> password;
    logger.log("Password was entered");
    std::string query_for_server = "SIGNIN " + client_configuration.username + " " + std::to_string(encrypt(password));
    send_to_server(sockfd, servaddr, query_for_server);
}

/* Регистрация */
void sign_up(int sockfd, sockaddr_in servaddr) {
    std::cout << "Username: ";
    std::cin >> client_configuration.username;
    logger.log("Username \"", client_configuration.username, "\" was entered");

    std::cout << "Password: ";
    std::string password;
    std::cin >> password;
    logger.log("Password was entered");
    std::string query_for_server = "SIGNUP " + client_configuration.username + " " + std::to_string(encrypt(password));
    send_to_server(sockfd, servaddr, query_for_server);
}

void menu(int sockfd, sockaddr_in servaddr) {
    logger.log("App was started");

    while (!disable_menu) {

        std::cout << "1. Sign in\n2. Sign up\n3. Exit\n";
        std::cout << "Enter the number to select an option: ";

        int x = std::get<int>(input(0));

        if (x == 1) {
            logger.log("Sign in was selected");
            sign_in(sockfd, servaddr);
            disable_menu = true;
        }
        else if (x == 2) {
            logger.log("Sign up was selected");
            sign_up(sockfd, servaddr);
            disable_menu = true;
        }
        else if (x == 3) {
            logger.log("Exit was selected");
            stop_flag = true;
            disable_menu = true;
            message_queue.push("EXIT");
            break;
            //break; // Exit the loop and program
        }
        else {
            logger.log("Wrong input");
            std::cout << "Wrong input, please try again.\n";
        }

    }
}

int main(int argc, char** argv) {
    //std::ios::sync_with_stdio(false);
    std::string config_file;

    if (argc < 2) {
        std::cerr << "Usage: " + std::string(argv[0]) + "<path/to/config_file";
        return 1;
    } else {
        config_file = argv[1];
    }

    try {
        /* Чтение конфигурационного файла */
        if (!std::filesystem::exists("client.ini")) {
            std::cout << "client.ini does not exist" << std::endl;
            return 0;
        }
        mINI::INIFile file("client.ini");
        mINI::INIStructure ini;
        file.read(ini);
        std::string &address = ini["info"]["server"];
        std::string &log_file = ini["info"]["log_file"];
        if (!log_file.empty()) {
            logger.set_log_file(log_file);
        }
        if (address.empty()) {
            std::cout << "client.ini: [info][server] empty/doesn't exist" << std::endl;
            return 0;
        }
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(PORT);
        servaddr.sin_addr.s_addr = inet_addr(address.c_str());
        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            std::cout << "Connect failed" << std::endl;
            logger.log("Connect failed");
            close(sockfd);
            return 1;
        }
        /* Получение информации с конфигурационного файла от сервера */
        send_to_server(sockfd, servaddr, "GET");
        {
            char buffer[BUF_SZ];
            socklen_t len = sizeof(servaddr);

            int n = recvfrom(sockfd, buffer, BUF_SZ, 0, reinterpret_cast<sockaddr *>(&servaddr), &len);
            if (n < 0) {
                std::cout << "Failed to connect to the server" << std::endl;
                logger.log("Connect failed");
                return 1;
            }

            buffer[n] = '\0';
            SECONDS_PER_MOVE = atoi(buffer);
        }
        // Start the thread to handle server responses
        std::thread response_handler(handle_server_responses, sockfd, servaddr);
        std::thread message_processor(process_messages, sockfd, servaddr);

        if (!disable_menu) {
            menu(sockfd, servaddr);
        }
        /* Обработка исключений потоков */
        for (const auto& e : exceptions) {
            try {
                std::rethrow_exception(e);
            }
            catch (const std::exception& err) {
                logger.log(err.what());
            }
            catch(...) {
                logger.log("Unknown error");
            }
        }

        response_handler.join();
        message_processor.join();
        close(sockfd);
    }
    /* Обработка исключений основного потока */
    catch (const std::exception &error) {
        logger.log(error.what());
        std::cout << "Exiting due to the error.";
    }
    catch (...) {
        logger.log("Unknown error");
        std::cout << "Exiting due to the error.";
    }
    return 0;
}
