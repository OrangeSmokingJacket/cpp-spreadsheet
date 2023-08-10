#pragma once

#include "common.h"
#include "formula.h"

#include <forward_list>

class Cell : public CellInterface
{
public:
    Cell() : content(std::unique_ptr<CellContent>(new CellContent())) { }
    ~Cell()
    {
        content.release();
    }

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

private:
    class CellContent
    {
    public:
        virtual Value GetValue() const;
        virtual std::string GetText() const;
    };
    class TextCell : public CellContent
    {
    public:
        explicit TextCell(std::string&& text) : text(text) {}

        Value GetValue() const override;
        std::string GetText() const override;
    private:
        std::string text;
    };
    class FormulaCell : public CellContent
    {
    public:
        explicit FormulaCell(std::unique_ptr<FormulaInterface> f) : formula(std::move(f)) {}

        Value GetValue() const override;
        std::string GetText() const override;
    private:
        std::unique_ptr<FormulaInterface> formula;
    };

    std::unique_ptr<CellContent> content;
    std::forward_list<Position> cells;
};