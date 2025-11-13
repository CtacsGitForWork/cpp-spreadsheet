#include "cell.h"
#include "sheet.h"

#include <cassert>
#include <cctype>
#include <optional>
#include <queue>
#include <stack>
#include <stdexcept>
#include <utility>
#include <variant>

using std::string;

// ================= Impl =================

class Cell::Impl {
public:
    virtual ~Impl() = default;
    virtual CellInterface::Value GetValue() const = 0;
    virtual std::string GetText() const = 0;
    virtual std::vector<Position> GetReferencedCells() const = 0;        
    virtual void InvalidateCache() {}    // Для формульных ячеек — сброс кэша; по умолчанию ничего
};


// ================= Impl: Empty =================

class Cell::EmptyImpl : public Impl {
public:
    CellInterface::Value GetValue() const override {           
        return 0.0;
    }

    std::string GetText() const override { 
        return std::string{}; 
    }

    std::vector<Position> GetReferencedCells() const override { 
        return {}; 
    }
};


// ================= Impl: Text =================

class Cell::TextImpl : public Impl {
public:
    explicit TextImpl(std::string text) : text_(std::move(text)) {}

    CellInterface::Value GetValue() const override {
        if (!text_.empty() && text_[0] == ESCAPE_SIGN) {
            return text_.substr(1); // снимаем экранирование
        }
        return text_;
    }

    std::string GetText() const override { 
        return text_; 
    }

    std::vector<Position> GetReferencedCells() const override { 
        return {}; 
    }

private:
    std::string text_;
};


// ================= Impl: Formula =================

class Cell::FormulaImpl : public Impl {
public:        
    FormulaImpl(std::string expression, SheetInterface& sheet)
        : formula_ptr_(ParseFormula(std::move(expression)))
        , sheet_(sheet) {}

    CellInterface::Value GetValue() const override {    
        if (!cache_.has_value()) {        	
            auto eval_result = formula_ptr_->Evaluate(sheet_);

            if (std::holds_alternative<double>(eval_result)) {           
                cache_ = std::get<double>(eval_result);
            } else {            
                cache_ = std::get<FormulaError>(eval_result);
            }
        }
        return *cache_;
    }

    std::string GetText() const override {
        return FORMULA_SIGN + formula_ptr_->GetExpression();  // Очищенная версия	
    }

    std::vector<Position> GetReferencedCells() const override {
        return formula_ptr_->GetReferencedCells();	
    }

    void InvalidateCache() override { // только reset собственного кэша
        cache_.reset();
    }

private:
    std::unique_ptr<FormulaInterface> formula_ptr_;
    SheetInterface& sheet_;
    mutable std::optional<CellInterface::Value> cache_; 
};


// ================= Cell =================

Cell::Cell(Sheet& sheet)
    : sheet_(sheet)
    , impl_(std::make_unique<EmptyImpl>()) {}

Cell::~Cell() = default;

void Cell::Set(std::string text) {
    // если текст(значение в ячейке) не изменился - сразу выходим
    if (text == Cell::GetText()) {
        return;
    }
    
    // Сохраняем предыдущее состояние
    auto old_impl = std::move(impl_);
    std::vector<Position> new_refs;

    try {
        if (!text.empty() && text[0] == FORMULA_SIGN && text.size() > 1) {
            // формула: парсим без '='
            auto new_impl = std::make_unique<FormulaImpl>(text.substr(1), sheet_);
            new_refs = new_impl->GetReferencedCells();

            // проверка на цикличность (BFS через очередь)
            if (CheckCircularDependency(new_refs)) {
                throw CircularDependencyException("Circular dependency detected");
            }

            impl_ = std::move(new_impl);
        } else if (!text.empty() && text[0] == ESCAPE_SIGN) {
            // экранированный текст
            impl_ = std::make_unique<TextImpl>(text);
        } else if (!text.empty()) {
            // обычный текст
            impl_ = std::make_unique<TextImpl>(std::move(text));
        } else {
            // пусто
            impl_ = std::make_unique<EmptyImpl>();
        }

        // обновляем граф зависимостей (detach от старых, attach к новым)
        UpdateDependencies(new_refs);

        // инвалидируем кэш вниз по зависимостям (stack)
        InvalidateCacheDownstream();

    } catch (...) {
        // откат
        impl_ = std::move(old_impl);
        throw;
    }
}

void Cell::Clear() {
    Set("");

    // отписываемся от всех источников
    UnsubscribeFromSources();

    // инвалидируем зависимых (значение изменилось)
    InvalidateCacheDownstream();
}

CellInterface::Value Cell::GetValue() const {
    return impl_->GetValue();
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetReferencedCells();
}

bool Cell::IsReferenced() const {
    return !dependent_cells_.empty();
}


// ---------- граф/помощники ----------

bool Cell::CheckCircularDependency(const std::vector<Position>& new_refs) const {
    // стартуем BFS от всех новых источников: если дойдём до this — цикл
    std::queue<const Cell*> queue;
    std::unordered_set<const Cell*> visited;

    for (const Position& pos : new_refs) {        
        const Cell* c = sheet_.GetConcreteCell(pos);
        if (!c) {
            continue;
        }
        if (c == this) {
            return true;
        }
        queue.push(c);
    }

    while (!queue.empty()) {
        const Cell* cur = queue.front();
        queue.pop();
        if (!visited.insert(cur).second) {
            continue;
        }
        if (cur == this) {
            return true;
        }
        // идём по ссылкам этой ячейки
        for (const Cell* src : cur->source_cells_) {
            queue.push(src);
        }
    }
    return false;
}

void Cell::UnsubscribeFromSources() {
    for (Cell* src : source_cells_) {
        if (src) {
            src->dependent_cells_.erase(this);
        }
    }
    source_cells_.clear();
}

void Cell::UpdateDependencies(const std::vector<Position>& new_refs) {
    // отписка от старых источников
    UnsubscribeFromSources();

    // подписка на новые источники 
    if (!new_refs.empty()) {
        for (const Position& pos : new_refs) {
            // получить/создать ячейку-источник
            CellInterface* ci = sheet_.GetCell(pos);
            if (!ci) {                
                sheet_.SetCell(pos, "");
                ci = sheet_.GetCell(pos);
            }
            if (!ci) continue;

            Cell* src = dynamic_cast<Cell*>(ci);
            if (!src || src == this) continue;

            source_cells_.insert(src);
            src->dependent_cells_.insert(this);
        }
    }
}

void Cell::InvalidateCacheDownstream() {
    // Проходим вниз по зависимостям и сбрасываем кэш у всех формульных
    std::stack<Cell*> st;
    std::unordered_set<Cell*> visited;

    st.push(this);
    while (!st.empty()) {
        Cell* cur = st.top();
        st.pop();

        if (!visited.insert(cur).second) continue;

        // Сброс кэша, если формула
        if (cur->impl_) {
            cur->impl_->InvalidateCache();
        }

        // дальше вниз
        for (Cell* d : cur->dependent_cells_) {
            st.push(d);
        }
    }
}


