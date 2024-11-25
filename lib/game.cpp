#include <fstream>

#include "game.hpp"
#include "logger.hpp"

static Logger logger("game", "game.log");


TicTacToe::TicTacToe(int moves_count) : moves_count_(moves_count), board_(
        {{{'-', '-', '-'}, {'-', '-', '-'}, {'-', '-', '-'}}}
) {};


TicTacToe::~TicTacToe() {
    moves_count_ = 0;
    clear();
}

void TicTacToe::clear() {
    board_ = {{{'-', '-', '-'}, {'-', '-', '-'}, {'-', '-', '-'}}};
    moves_count_ = 0;
    logger.log("Board cleared");
}

int TicTacToe::size() const {
    return N_;
}

/*
 * 0 - game is ongoing
 * 1 - player x won
 * 2 - player 0 - won
 * 3 - draw
 * */
int TicTacToe::game_status() const {
    if (moves_count_ < 5) {
        return 0;
    }
    int game_status = 0;
    for (int i = 0; i < N_; ++i) {
        int horizontal_matches = 0;
        int vertical_matches = 0;
        for (int j = 0; j < N_ - 1; ++j) {
            if (board_[i][j] == board_[i][j + 1] && board_[i][j] != EMPTY_) {
                ++horizontal_matches;
            }
            if (board_[j][i] == board_[j + 1][i] && board_[j][i] != EMPTY_) {
                ++vertical_matches;
            }
        }
        if (horizontal_matches == N_ - 1) {
            game_status = (board_[i][0] == X_CHAR_ ? 1 : 2);
            break;
        }
        else if (vertical_matches == N_ - 1) {
            game_status = (board_[0][i] == X_CHAR_ ? 1 : 2);
            break;
        }
    }
    if (game_status == 0) {
        //calculate diagonal;
        int main_diagonal_matches = 0;
        int side_diagonal_matches = 0;
        for (int i = 0; i < N_ - 1; ++i) {
            if (board_[i][i] == board_[i + 1][i + 1] && board_[i][i] != EMPTY_) {
                ++main_diagonal_matches;
            }
            if (board_[i][N_ - i - 1] == board_[i + 1][N_ - i - 2] && board_[i][N_ - i - 1] != EMPTY_) {
                ++side_diagonal_matches;
            }
        }
        if (main_diagonal_matches == N_ - 1) {
            game_status = (board_[0][0] == X_CHAR_ ? 1 : 2);
        }
        else if (side_diagonal_matches == N_ - 1) {
            game_status = (board_[0][N_ - 1] == X_CHAR_ ? 1 : 2);
        }
    }
    if (game_status == 0 && moves_count_ == N_ * N_) {
        game_status = 3;
    }

    logger.log("Game status: ", game_status);
    return game_status;
}

void TicTacToe::draw_board() const {
    for (int i = 0; i < N_; ++i) {
        for (int j = 0; j < N_; ++j) {
            std::cout << board_[i][j] << " | ";
        }
        std::cout << "\n";
    }
    logger.log("The board was drawn");
}

bool TicTacToe::is_cell_taken(const int &x, const int &y) const {
    return board_[x][y] != EMPTY_;
}

void TicTacToe::push_move(const int &x, const int &y, const char& order) {
    if (order == X_CHAR_) {
        board_[x][y] = X_CHAR_;
        logger.log(x, " ", y, ": X");
    }
    else {
        board_[x][y] = O_CHAR_;
        logger.log(x, " ", y, ": O");
    }
    //board_[x][y] = (order == 0 ? X_CHAR_ : O_CHAR_);
    ++moves_count_;

}

std::string TicTacToe::to_string() const {
    std::string res;
    for (int i = 0; i < N_; ++i) {
        for (int j = 0; j < N_; ++j) {
            res += board_[i][j];
        }
        if (i + 1 < N_) {
            res += '\n';
        }
    }
    return res;
}