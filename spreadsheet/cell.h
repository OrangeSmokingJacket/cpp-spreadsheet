#pragma once

#include "common.h"
#include "formula.h"
#include "sheet.h"

#include <forward_list>
#include <optional>

class Sheet;

class Cell : public CellInterface
{
public:
    Cell(Sheet& sheet) : sheet_ref(sheet), content(std::unique_ptr<CellContent>(new CellContent())) { }
    ~Cell()
    {
        content.release();
    }

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    void ClearCash();

private:
    class CellContent
    {
    public:
        virtual Value GetValue(Sheet& sheet) const;
        virtual std::string GetText() const;
        virtual std::vector<Position> GetReferencedCells() const;
    };
    class TextCell : public CellContent
    {
    public:
        explicit TextCell(std::string&& text) : text(text) {}

        Value GetValue(Sheet& sheet) const override;
        std::string GetText() const override;
        std::vector<Position> GetReferencedCells() const override;

    private:
        std::string text;
    };
    class NumberCell : public CellContent
    {
    public:
        explicit NumberCell(double v) : value(v) {}

        Value GetValue(Sheet& sheet) const override;
        std::string GetText() const override;
        std::vector<Position> GetReferencedCells() const override;

    private:
        double value;
    };
    class FormulaCell : public CellContent
    {
    public:
        explicit FormulaCell(std::unique_ptr<FormulaInterface> f) : formula(std::move(f)) {}

        Value GetValue(Sheet& sheet) const override;
        std::string GetText() const override;
        std::vector<Position> GetReferencedCells() const override;

    private:
        std::unique_ptr<FormulaInterface> formula;
    };

    Sheet& sheet_ref;
    std::unique_ptr<CellContent> content;
    mutable std::optional<CellInterface::Value> cash;
};