
### Запуск проекта

Для запуска игры необходимо выполнить следующие шаги:

1. Склонируйте репозиторий:
   ```bash
   git clone https://github.com/cezarDi/TicTacToe
   cd TicTacToe

2. Компиляция (требуется cmake):
    ```bash
   cmake . && make

3. Настройте поля

   в файле ```server.ini``` для сервера:
    ```bash
    move_time=
    database_file=
    ```

   в файле ```client.ini``` для клиента:
    ```bash
    server=
   ```
   

4. Запуск:<br/>
    ```./server``` - для сервера<br/>
   ```./client``` - для клиента

5. Авторизация:

	При запуске программа попросит вас зарегистрироваться. При последующих запусках ваши логин и пароль будут сохранены и регистрация не потребуется.

## Правила игры в крестики-нолики

Один игрок использует символ "X", а другой — "O". Игра проходит на квадратном поле размером 3x3.

### Цель игры

Цель игры — первым собрать три своих символа в ряд: по горизонтали, вертикали или диагонали.

### Правила игры

1. **Начало игры**:
    - Первый ход всегда делает игрок, играющий за "X".

2. **Ход игры**:
    - Игроки поочередно ставят свои символы ("X" или "O") на свободные клетки поля.
    - Игрок не может ставить свой символ в уже занятую клетку.

3. **Финиш игры**:
    - Игра заканчивается, когда один из игроков соберет три своих символа в ряд (горизонтально, вертикально или диагонально).
    - Если все клетки заполнены, а ни один из игроков не собрал три символа в ряд, игра заканчивается ничьей.
