#include "sheet.h"
#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>

using namespace std::literals;

Sheet::Sheet() {
    // Инициализируем пустую таблицу
    cells_.resize(1);
    cells_[0].resize(1);
}

Sheet::~Sheet() = default;

void Sheet::MaybeIncreaseSizeToIncludePosition(Position pos) {
    ResizeIfNeeded(cells_, pos.row);
    
    if (pos.col >= 0) {
        for (auto& row : cells_) {
            ResizeIfNeeded(row, pos.col);
        }
    }
}

void Sheet::SetCell(Position pos, std::string text) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }

    MaybeIncreaseSizeToIncludePosition(pos);

    Cell* cell = GetConcreteCell(pos);
    if (!cell) {
        cells_[pos.row][pos.col] = std::make_unique<Cell>(*this);
        cell = cells_[pos.row][pos.col].get();
    }

    try {
        cell->Set(std::move(text));
    } catch (const CircularDependencyException&) {
        throw;
    } catch (const FormulaException&) {
        throw;
    } catch (const std::exception& e) {
        throw FormulaException(std::string("Unknown formula error: ") + e.what());
    }
}

const CellInterface* Sheet::GetCell(Position pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }

    if (pos.row >= static_cast<int>(cells_.size()) || 
        pos.col >= static_cast<int>(cells_[pos.row].size())) {
        return nullptr;
    }

    return cells_[pos.row][pos.col].get();
}

CellInterface* Sheet::GetCell(Position pos) {
    return const_cast<CellInterface*>(
        static_cast<const Sheet&>(*this).GetCell(pos)
    );
}

const Cell* Sheet::GetConcreteCell(Position pos) const {
    return dynamic_cast<const Cell*>(GetCell(pos));
}

Cell* Sheet::GetConcreteCell(Position pos) {
    return dynamic_cast<Cell*>(GetCell(pos));
}

void Sheet::ClearCell(Position pos) {
    Cell* cell = GetConcreteCell(pos);
    if (!cell) return;

    if (cell->IsReferenced()) {
        // Кто-то ссылается → превращаем в пустую клетку
        cell->Clear();
    } else {
        // Никто не ссылается → удаляем объект полностью
        cells_[pos.row][pos.col].reset();
    }
}

Size Sheet::GetActualSize() const {
    int max_row = -1;
    int max_col = -1;

    for (int row = 0; row < static_cast<int>(cells_.size()); ++row) {
        for (int col = 0; col < static_cast<int>(cells_[row].size()); ++col) {
            if (cells_[row][col] && !cells_[row][col]->GetText().empty()) {
                max_row = std::max(max_row, row);
                max_col = std::max(max_col, col);
            }
        }
    }

    return {
        max_row >= 0 ? max_row + 1 : 0,
        max_col >= 0 ? max_col + 1 : 0
    };
}

Size Sheet::GetPrintableSize() const {
    return GetActualSize();
}

void Sheet::PrintCells(std::ostream& output,
                       const std::function<void(const CellInterface&)>& printCell) const {
    Size size = GetPrintableSize();
    
    for (int row = 0; row < size.rows; ++row) {
        for (int col = 0; col < size.cols; ++col) {
            if (col > 0) {
                output << '\t';
            }
            const CellInterface* cell = nullptr;
            if (row < static_cast<int>(cells_.size()) && 
                col < static_cast<int>(cells_[row].size())) {
                cell = cells_[row][col].get();
            }
            
            // Выводим только ячейки с непустым текстом
            if (cell && !cell->GetText().empty()) {
                printCell(*cell);
            }
            // Пустые ячейки (nullptr или с пустым текстом) не выводятся
        }
        output << '\n';
    }
}

void Sheet::PrintValues(std::ostream& output) const {
    PrintCells(output, [&output](const CellInterface& cell) {
        auto val = cell.GetValue();
        if (std::holds_alternative<double>(val)) {
            output << std::get<double>(val);
        } else if (std::holds_alternative<FormulaError>(val)) {
            output << std::get<FormulaError>(val);
        } else {
            output << std::get<std::string>(val);
        }
    });
}

void Sheet::PrintTexts(std::ostream& output) const {
    PrintCells(output, [&output](const CellInterface& cell) {
        output << cell.GetText();
    });
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}

