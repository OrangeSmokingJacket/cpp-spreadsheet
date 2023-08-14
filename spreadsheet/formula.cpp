#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

namespace
{
    class Formula : public FormulaInterface
    {
    public:
        explicit Formula(std::string expression) : ast(ParseFormulaAST(expression)) { }
        Value Evaluate(const SheetInterface& sheet) const override
        {
            try
            {
                return ast.Execute(sheet);
            }
            catch (const FormulaError& exc)
            {
                return exc;
            }
        }
        std::string GetExpression() const override
        {
            std::stringstream result;
            ast.PrintFormula(result);
            return result.str();
        }
        std::vector<Position> GetReferencedCells() const override
        {
            std::set<Position> result;
            for (const Position& pos : ast.GetCells())
            {
                result.insert(pos);
            }

            return { result.begin(), result.end() };
        }

    private:
        FormulaAST ast;
    };
}

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression)
{
    try
    {
        return std::make_unique<Formula>(std::move(expression));
    }
    catch (const std::exception& e)
    {
        throw FormulaException(e.what());
    }
}