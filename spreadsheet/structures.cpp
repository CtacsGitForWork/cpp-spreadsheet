#include "common.h"

#include <cctype>
#include <sstream>
#include <algorithm>

const int LETTERS = 26;
const int MAX_POSITION_LENGTH = 17;
const int MAX_POS_LETTER_COUNT = 3;

const Position Position::NONE = {-1, -1};

bool Position::operator==(const Position rhs) const {
    return row == rhs.row && col == rhs.col;
}

bool Position::operator<(const Position rhs) const {
    return std::tie(row, col) < std::tie(rhs.row, rhs.col);
}

bool Position::IsValid() const {
    return row >= 0 && col >= 0 && row < MAX_ROWS && col < MAX_COLS;
}

std::string Position::ToString() const {
    if (!IsValid()) {
        return "";
    }

    std::string result;
    result.reserve(MAX_POSITION_LENGTH);
    int col_copy = col;
    while (col_copy >= 0) {
        result.insert(result.begin(), 'A' + (col_copy % LETTERS));
        col_copy = col_copy / LETTERS - 1;
    }

    result += std::to_string(row + 1);

    return result;
}

Position Position::FromString(std::string_view str) {
    // Сразу проверяем базовые невалидные случаи
    if (str.empty() || str.size() > MAX_POSITION_LENGTH) {
        return NONE;
    }

    // Ищем границу между буквами и цифрами
    size_t letter_count = 0;
    while (letter_count < str.size() && isalpha(str[letter_count])) {
        // Проверяем верхний регистр сразу при обходе
        if (!isupper(str[letter_count++])) {
            return NONE;
        }
    }

    // Проверяем наличие буквенной части и её длину
    if (letter_count == 0 || letter_count > MAX_POS_LETTER_COUNT) {
        return NONE;
    }

    // Проверяем что оставшаяся часть - только цифры
    const size_t number_pos = letter_count;
    if (number_pos == str.size()) {  // Нет числовой части
        return NONE;
    }

    // Проверка цифровой части
    for (size_t i = number_pos; i < str.size(); ++i) {
        if (!isdigit(str[i])) {
            return NONE;
        }
    }

    // Парсим буквенную часть в столбец (оптимизированный расчёт)
    int col = 0;
    for (size_t i = 0; i < letter_count; ++i) {
        col = col * LETTERS + (str[i] - 'A' + 1);
    }
    col--;

    // Парсим числовую часть (без исключений (без stoi) и без from_chars)
    int row = 0;
    for (size_t i = number_pos; i < str.size(); ++i) {
        row = row * 10 + (str[i] - '0'); //Умножение на 10 "сдвигает" текущее число влево, освобождая место для новой цифры
    }
    row--;

    // Финальная проверка диапазонов
    Position result{row, col};
    return result.IsValid() ? result : NONE;
}

bool Size::operator==(Size rhs) const {
    return cols == rhs.cols && rows == rhs.rows;
}
