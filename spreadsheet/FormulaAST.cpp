#include "FormulaAST.h"

#include "FormulaBaseListener.h"
#include "FormulaLexer.h"
#include "FormulaParser.h"

#include <cassert>
#include <cmath>
#include <memory>
#include <optional>
#include <sstream>

namespace ASTImpl
{

    enum ExprPrecedence
    {
        EP_ADD,
        EP_SUB,
        EP_MUL,
        EP_DIV,
        EP_UNARY,
        EP_ATOM,
        EP_END,
    };

    // a bit is set when the parentheses are needed
    enum PrecedenceRule
    {
        PR_NONE = 0b00,                // never needed
        PR_LEFT = 0b01,                // needed for a left child
        PR_RIGHT = 0b10,               // needed for a right child
        PR_BOTH = PR_LEFT | PR_RIGHT,  // needed for both children
    };

    // PRECEDENCE_RULES[parent][child] determines if parentheses need
    // to be inserted between a parent and a child of specific precedences;
    // for some nodes rules are different for left and right children:
    // (X c Y) p Z  vs  X p (Y c Z)
    //
    // The interesting cases are the ones where removing the parens would change the AST.
    // It may happen when our precedence rules for parentheses are different from
    // the grammatic precedence of operations.
    //
    // Case analysis:
    // A + (B + C) - always okay (nothing of lower grammatic precedence could have been written to the
    // right)
    //    (e.g. if we had A + (B + C) / D, it wouldn't parse in a way
    //    that woudld have given us A + (B + C) as a subexpression to deal with)
    // A + (B - C) - always okay (nothing of lower grammatic precedence could have been written to the
    // right) A - (B + C) - never okay A - (B - C) - never okay A * (B * C) - always okay (the parent
    // has the highest grammatic precedence) A * (B / C) - always okay (the parent has the highest
    // grammatic precedence) A / (B * C) - never okay A / (B / C) - never okay
    // -(A + B) - never okay
    // -(A - B) - never okay
    // -(A * B) - always okay (the resulting binary op has the highest grammatic precedence)
    // -(A / B) - always okay (the resulting binary op has the highest grammatic precedence)
    // +(A + B) - **sometimes okay** (e.g. parens in +(A + B) / C are **not** optional)
    //     (currently in the table we're always putting in the parentheses)
    // +(A - B) - **sometimes okay** (same)
    //     (currently in the table we're always putting in the parentheses)
    // +(A * B) - always okay (the resulting binary op has the highest grammatic precedence)
    // +(A / B) - always okay (the resulting binary op has the highest grammatic precedence)
    constexpr PrecedenceRule PRECEDENCE_RULES[EP_END][EP_END] =
    {
        /* EP_ADD */ {PR_NONE, PR_NONE, PR_NONE, PR_NONE, PR_NONE, PR_NONE},
        /* EP_SUB */ {PR_RIGHT, PR_RIGHT, PR_NONE, PR_NONE, PR_NONE, PR_NONE},
        /* EP_MUL */ {PR_BOTH, PR_BOTH, PR_NONE, PR_NONE, PR_NONE, PR_NONE},
        /* EP_DIV */ {PR_BOTH, PR_BOTH, PR_RIGHT, PR_RIGHT, PR_NONE, PR_NONE},
        /* EP_UNARY */ {PR_BOTH, PR_BOTH, PR_NONE, PR_NONE, PR_NONE, PR_NONE},
        /* EP_ATOM */ {PR_NONE, PR_NONE, PR_NONE, PR_NONE, PR_NONE, PR_NONE},
    };

    class Expr
    {
    public:
        virtual ~Expr() = default;
        virtual void Print(std::ostream& out) const = 0;
        virtual void DoPrintFormula(std::ostream& out, ExprPrecedence precedence) const = 0;
        virtual double Evaluate(const SheetInterface& sheet) const = 0;

        // higher is tighter
        virtual ExprPrecedence GetPrecedence() const = 0;

        void PrintFormula(std::ostream& out, ExprPrecedence parent_precedence, bool right_child = false) const
        {
            auto precedence = GetPrecedence();
            auto mask = right_child ? PR_RIGHT : PR_LEFT;
            bool parens_needed = PRECEDENCE_RULES[parent_precedence][precedence] & mask;
            if (parens_needed)
                out << '(';

            DoPrintFormula(out, precedence);

            if (parens_needed)
                out << ')';
        }
    };

    namespace
    {
        class BinaryOpExpr final : public Expr
        {
        public:
            enum Type : char
            {
                Add = '+',
                Subtract = '-',
                Multiply = '*',
                Divide = '/',
            };

        public:
            explicit BinaryOpExpr(Type type, std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) : type(type), lhs(std::move(lhs)), rhs(std::move(rhs)) { }

            void Print(std::ostream& out) const override
            {
                out << '(' << static_cast<char>(type) << ' ';
                lhs->Print(out);
                out << ' ';
                rhs->Print(out);
                out << ')';
            }
            void DoPrintFormula(std::ostream& out, ExprPrecedence precedence) const override
            {
                lhs->PrintFormula(out, precedence);
                out << static_cast<char>(type);
                rhs->PrintFormula(out, precedence, /* right_child = */ true);
            }

            ExprPrecedence GetPrecedence() const override
            {
                switch (type)
                {
                    case Add:
                        return EP_ADD;
                    case Subtract:
                        return EP_SUB;
                    case Multiply:
                        return EP_MUL;
                    case Divide:
                        return EP_DIV;
                    default:
                        // have to do this because VC++ has a buggy warning
                        assert(false);
                        return static_cast<ExprPrecedence>(INT_MAX);
                }
            }

            double Evaluate(const SheetInterface& sheet) const override
            {
                double result = 0;
                double lhs_value = lhs->Evaluate(sheet);
                double rhs_value = rhs->Evaluate(sheet);

                switch (type)
                {
                case Type::Add:
                    result = lhs_value + rhs_value;
                    break;
                case Type::Subtract:
                    result = lhs_value - rhs_value;
                    break;
                case Type::Multiply:
                    result = lhs_value * rhs_value;
                    break;
                case Type::Divide:
                    if (rhs_value < std::numeric_limits<double>::epsilon() && rhs_value > -std::numeric_limits<double>::epsilon())
                        throw FormulaError(FormulaError::Category::Div0);
                    else
                        return lhs_value / rhs_value;
                }

                if (result == std::numeric_limits<double>::infinity() || result == -std::numeric_limits<double>::infinity())
                    throw FormulaError(FormulaError::Category::Div0);
                else
                    return result;
            }

        private:
            Type type;
            std::unique_ptr<Expr> lhs;
            std::unique_ptr<Expr> rhs;
        };
        class UnaryOpExpr final : public Expr
        {
        public:
            enum Type : char
            {
                UnaryPlus = '+',
                UnaryMinus = '-',
            };

        public:
            explicit UnaryOpExpr(Type type, std::unique_ptr<Expr> operand)  : type(type), operand(std::move(operand)) { }

            void Print(std::ostream& out) const override
            {
                out << '(' << static_cast<char>(type) << ' ';
                operand->Print(out);
                out << ')';
            }
            void DoPrintFormula(std::ostream& out, ExprPrecedence precedence) const override
            {
                out << static_cast<char>(type);
                operand->PrintFormula(out, precedence);
            }

            ExprPrecedence GetPrecedence() const override
            {
                return EP_UNARY;
            }

            double Evaluate(const SheetInterface& sheet) const override
            {
                double value = operand->Evaluate(sheet);

                switch (type)
                {
                case Type::UnaryMinus:
                    return -value;
                default:
                    return value;
                }
            }

        private:
            Type type;
            std::unique_ptr<Expr> operand;
        };
        class CellExpr final : public Expr
        {
        public:
            explicit CellExpr(const Position* cell) : cell(cell) { }

            void Print(std::ostream& out) const override
            {
                if (!cell->IsValid())
                    out << FormulaError::Category::Ref;
                else
                    out << cell->ToString();
            }
            void DoPrintFormula(std::ostream& out, ExprPrecedence /* precedence */) const override
            {
                Print(out);
            }
            ExprPrecedence GetPrecedence() const override
            {
                return EP_ATOM;
            }

            double Evaluate(const SheetInterface& sheet) const override
            {
                const CellInterface* c = sheet.GetCell(*cell);

                if (!c)
                    return 0.0;

                CellInterface::Value result = c->GetValue();

                if (std::holds_alternative<std::string>(result))
                {
                    if (std::get<std::string>(result) == "")
                        return 0.0;
                    else
                        throw FormulaError(FormulaError::Category::Value);
                }
                else if (std::holds_alternative<FormulaError>(result))
                    throw std::get<FormulaError>(result);
                else
                    return std::get<double>(result);
            }

        private:
            const Position* cell;
        };
        class NumberExpr final : public Expr
        {
        public:
            explicit NumberExpr(double value) : value(value) { }

            void Print(std::ostream& out) const override
            {
                out << value;
            }
            void DoPrintFormula(std::ostream& out, ExprPrecedence /* precedence */) const override
            {
                out << value;
            }

            ExprPrecedence GetPrecedence() const override
            {
                return EP_ATOM;
            }

            double Evaluate(const SheetInterface&) const override
            {
                return value;
            }

        private:
            double value;
        };

        class ParseASTListener final : public FormulaBaseListener
        {
        public:
            std::unique_ptr<Expr> MoveRoot()
            {
                assert(args.size() == 1);
                auto root = std::move(args.front());
                args.clear();

                return root;
            }

            std::forward_list<Position> MoveCells()
            {
                return std::move(cells);
            }

        public:
            void exitUnaryOp(FormulaParser::UnaryOpContext* ctx) override
            {
                assert(args.size() >= 1);

                auto operand = std::move(args.back());

                UnaryOpExpr::Type type;
                if (ctx->SUB())
                {
                    type = UnaryOpExpr::UnaryMinus;
                }
                else
                {
                    assert(ctx->ADD() != nullptr);
                    type = UnaryOpExpr::UnaryPlus;
                }

                auto node = std::make_unique<UnaryOpExpr>(type, std::move(operand));
                args.back() = std::move(node);
            }
            void exitLiteral(FormulaParser::LiteralContext* ctx) override {
                double value = 0;
                auto valueStr = ctx->NUMBER()->getSymbol()->getText();
                std::istringstream in(valueStr);
                in >> value;
                if (!in)
                    throw ParsingError("Invalid number: " + valueStr);

                auto node = std::make_unique<NumberExpr>(value);
                args.push_back(std::move(node));
            }
            void exitCell(FormulaParser::CellContext* ctx) override
            {
                auto value_str = ctx->CELL()->getSymbol()->getText();
                auto value = Position::FromString(value_str);
                if (!value.IsValid())
                    throw FormulaException("Invalid position: " + value_str);

                cells.push_front(value);
                auto node = std::make_unique<CellExpr>(&cells.front());
                args.push_back(std::move(node));
            }
            void exitBinaryOp(FormulaParser::BinaryOpContext* ctx) override
            {
                assert(args.size() >= 2);

                auto rhs = std::move(args.back());
                args.pop_back();

                auto lhs = std::move(args.back());

                BinaryOpExpr::Type type;
                if (ctx->ADD())
                    type = BinaryOpExpr::Add;
                else if (ctx->SUB())
                    type = BinaryOpExpr::Subtract;
                else if (ctx->MUL())
                    type = BinaryOpExpr::Multiply;
                else
                {
                    assert(ctx->DIV() != nullptr);
                    type = BinaryOpExpr::Divide;
                }

                auto node = std::make_unique<BinaryOpExpr>(type, std::move(lhs), std::move(rhs));
                args.back() = std::move(node);
            }

            void visitErrorNode(antlr4::tree::ErrorNode* node) override
            {
                throw ParsingError("Error when parsing: " + node->getSymbol()->getText());
            }

        private:
            std::vector<std::unique_ptr<Expr>> args;
            std::forward_list<Position> cells;
        };

        class BailErrorListener : public antlr4::BaseErrorListener
        {
        public:
            void syntaxError(antlr4::Recognizer* /* recognizer */, antlr4::Token* /* offendingSymbol */,
                             size_t /* line */, size_t /* charPositionInLine */, const std::string& msg,
                             std::exception_ptr /* e */) override
            {
                throw ParsingError("Error when lexing: " + msg);
            }
        };
    }
}

FormulaAST ParseFormulaAST(std::istream& in)
{
    using namespace antlr4;

    ANTLRInputStream input(in);

    FormulaLexer lexer(&input);
    ASTImpl::BailErrorListener error_listener;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&error_listener);

    CommonTokenStream tokens(&lexer);

    FormulaParser parser(&tokens);
    auto error_handler = std::make_shared<BailErrorStrategy>();
    parser.setErrorHandler(error_handler);
    parser.removeErrorListeners();

    tree::ParseTree* tree = parser.main();
    ASTImpl::ParseASTListener listener;
    tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

    return FormulaAST(listener.MoveRoot(), listener.MoveCells());
}

FormulaAST ParseFormulaAST(const std::string& in_str)
{
    std::istringstream in(in_str);
    return ParseFormulaAST(in);
}

void FormulaAST::PrintCells(std::ostream& out) const
{
    for (auto cell : cells)
    {
        out << cell.ToString() << ' ';
    }
}

void FormulaAST::Print(std::ostream& out) const
{
    root_expr->Print(out);
}

void FormulaAST::PrintFormula(std::ostream& out) const
{
    root_expr->PrintFormula(out, ASTImpl::EP_ATOM);
}

double FormulaAST::Execute(const SheetInterface& sheet) const
{
    return root_expr->Evaluate(sheet);
}

FormulaAST::FormulaAST(std::unique_ptr<ASTImpl::Expr> root_expr, std::forward_list<Position> cells) : root_expr(std::move(root_expr)) , cells(std::move(cells))
{
    cells.sort();  // to avoid sorting in GetReferencedCells
}

FormulaAST::~FormulaAST() = default;
