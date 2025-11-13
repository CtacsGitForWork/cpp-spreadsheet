#pragma once

#include "cell.h"
#include "common.h"

#include <functional>


class Sheet : public SheetInterface {
public:
    Sheet();
    ~Sheet();

    void SetCell(Position pos, std::string text) override;  // Проверка циклических зависимостей происходит в методе Cell::Set

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    const Cell* GetConcreteCell(Position pos) const;
    Cell* GetConcreteCell(Position pos);

private:
    template<typename T>
    void ResizeIfNeeded(std::vector<T>& vec, int idx) {
        if (idx >= static_cast<int>(vec.size())) {
            vec.resize(idx + 1);
        }
    }

    void MaybeIncreaseSizeToIncludePosition(Position pos);
    void PrintCells(std::ostream& output,
                    const std::function<void(const CellInterface&)>& printCell) const;
    Size GetActualSize() const;

    std::vector<std::vector<std::unique_ptr<Cell>>> cells_;
};

