#ifndef __TIC_TAC_TOE__GAME
#define __TIC_TAC_TOE__GAME


#include <iostream>
#include <vector>
#include <array>
#include <string>


class TicTacToe {
    static inline const int N_ = 3;
    static inline const char EMPTY_ = '-';
    static inline const char X_CHAR_ = 'X';
    static inline const char O_CHAR_ = 'O';

    int moves_count_;

public:
    typedef std::array<std::array<char, N_>, N_> Board;
    TicTacToe(int moves_count = 0);
    /*
     * 0 - game is ongoing
     * 1 - player x win
     * 2 - player 0 - win
     * */
    int game_status() const;

    void draw_board() const;

    void push_move(const int &x, const int &y, const char& order);
    void clear();
    bool is_cell_taken(const int&, const int&) const;
    int size() const;
    std::string to_string() const;
    ~TicTacToe();
private:
    Board board_;
};



#endif