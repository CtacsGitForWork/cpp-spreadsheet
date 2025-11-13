#pragma once

#include "common.h"
#include "formula.h"

#include <functional>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

class Sheet;

class Cell : public CellInterface {
public:
    explicit Cell(Sheet& sheet);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    // Есть ли на эту ячейку ссылки у других ячеек
    bool IsReferenced() const;

private:
    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    Sheet& sheet_;
    std::unique_ptr<Impl> impl_;

    // Граф зависимостей: источники и зависимые
    std::unordered_set<Cell*> source_cells_;        // Ячейки, значения которых нужны для вычисления этой ячейки
    std::unordered_set<Cell*> dependent_cells_;     // Ячейки, которые используют значение этой ячейки для своих вычислений
   
    bool CheckCircularDependency(const std::vector<Position>& new_refs) const;
    void UnsubscribeFromSources();
    void UpdateDependencies(const std::vector<Position>& new_refs);
    void InvalidateCacheDownstream(); 
};


