#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

FormulaError::FormulaError(Category category)
    : category_(category) {}

FormulaError::Category FormulaError::GetCategory() const {
    return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
    return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
    switch (category_) {
    case FormulaError::Category::Ref:
        return "#REF!"sv;
        
    case FormulaError::Category::Value:
        return "#VALUE!"sv;
        
    case FormulaError::Category::Arithmetic:
        return "#ARITHM!"sv;
       
    default:
        return "#ARITHM!"sv;
    }    
}

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}


namespace {
class Formula : public FormulaInterface {
public:
    explicit Formula(std::string expression) try : ast_(ParseFormulaAST(expression)) {
    } catch (const FormulaException&) {
        throw;
    } catch (const std::exception& e) {
        throw FormulaException("Formula parsing error: "s + e.what());
    } 

    Value Evaluate(const SheetInterface& sheet) const override {
        auto get_cell_value = [&sheet](Position pos) -> double {
            const CellInterface* cell = sheet.GetCell(pos);
            if (!cell) {
                // ячейка отсутствует => трактуется как 0
                return 0.0;
            }
            auto val = cell->GetValue();

            // Варианты: std::string, double, FormulaError
            if (std::holds_alternative<double>(val)) {
                return std::get<double>(val);
            }

            if (std::holds_alternative<FormulaError>(val)) {
                throw std::get<FormulaError>(val);
            }

            if (std::holds_alternative<std::string>(val)) {
                const std::string& s = std::get<std::string>(val);
                if (s.empty()) {
                    // пустая строка трактуется как 0
                    return 0.0;
                }
                // пытаемся конвертировать строку в число
                try {
                    size_t idx = 0;
                    double d = std::stod(s, &idx);
                    if (idx == s.size()) {
                        return d;
                    } else {
                        // строка содержит нечисловые символы
                        throw FormulaError(FormulaError::Category::Value);
                    }
                } catch (const std::invalid_argument&) {
                    throw FormulaError(FormulaError::Category::Value);
                } catch (const std::out_of_range&) {
                    throw FormulaError(FormulaError::Category::Arithmetic);
                }
            }

            // По умолчанию (на всякий случай)
            return 0.0;
        };
        
        try {
            return ast_.Execute(get_cell_value);
        } catch (const FormulaError& e) {
            return e;
        }
    }

    std::string GetExpression() const override {
        std::ostringstream out;
        ast_.PrintFormula(out);
        return out.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        std::vector<Position> result(ast_.GetReferencedCells().begin(), ast_.GetReferencedCells().end());
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {   
    try {
        return std::make_unique<Formula>(std::move(expression));
    } catch (FormulaException &fe) {
        throw fe;
    }
}
