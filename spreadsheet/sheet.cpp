#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>

using namespace std::literals;

std::set<Position> DependeciesGraph::GetAllDependenciesFrom(Position from) const
{
    std::set<Position> visited;
    return GetAllDependenciesFrom(from, visited);
}
std::set<Position> DependeciesGraph::GetAllDependenciesFrom(Position from, std::set<Position>& visited) const
{
    auto it = reversed_edges.find(from);

    if (it == reversed_edges.end())
        return {};
    else
    {
        std::set<Position> result;

        for (const Position& pos : it->second)
        {
            if (visited.find(pos) != visited.end())
                continue;

            result.insert(pos);
            visited.insert(pos);
            for (const Position& pos_ : GetAllDependenciesFrom(pos, visited))
            {
                result.insert(pos_);
            }
        }

        return result;
    }
}
void DependeciesGraph::AddEdges(Position to, const std::vector<Position>& from)
{
    edges.erase(to);

    for (const Position& pos : from)
    {
        edges[to].insert(pos);
        reversed_edges[pos].insert(to);
    }
}

void DependeciesGraph::RemoveCell(Position pos)
{
    edges.erase(pos);
}

Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text)
{
    if (!pos.IsValid())
        throw InvalidPositionException("");
    
    if ((int)cells.size() <= pos.row)
        cells.resize(pos.row + 1);
    if ((int)cells[pos.row].size() <= pos.col)
        cells[pos.row].resize(pos.col + 1);
    
    if (cells[pos.row][pos.col].get())
    {
        if (cells[pos.row][pos.col].get()->GetText() == text)
            return;
    }

    std::unique_ptr<Cell> cell = std::unique_ptr<Cell>(new Cell(*this));

    try
    {
        cell->Set(text);
    }
    catch (const std::exception& exc)
    {
        std::throw_with_nested(FormulaException(exc.what()));
    }

    std::vector<Position> dependencies_to = cell->GetReferencedCells();
    std::set<Position> dependencies_from = graph.GetAllDependenciesFrom(pos);

    if (std::any_of(dependencies_to.begin(), dependencies_to.end(), [&dependencies_from](const Position& p) { return dependencies_from.find(p) != dependencies_from.end(); }) || std::find(dependencies_to.begin(), dependencies_to.end(), pos) != dependencies_to.end())
        throw CircularDependencyException("");

    graph.AddEdges(pos, dependencies_to);

    for (const Position p : dependencies_to)
    {
        if (GetCell(p) == nullptr)
        {
            SetCell(p, "");
            positions.insert(p);
        }
    }

    cells[pos.row][pos.col] = std::move(cell);

    positions.insert(pos);

    for (const Position p : dependencies_from)
    {
        ClearCash(p);
    }
}

const CellInterface* Sheet::GetCell(Position pos) const
{
    if (CheckPosition(pos))
        return cells[pos.row][pos.col].get();
    else
        return nullptr;
}
CellInterface* Sheet::GetCell(Position pos)
{
    if (CheckPosition(pos))
        return cells[pos.row][pos.col].get();
    else
        return nullptr;
}

void Sheet::ClearCell(Position pos)
{
    if (CheckPosition(pos))
    {
        if (cells[pos.row][pos.col] != nullptr)
            cells[pos.row][pos.col].release();

        std::set<Position> cleaning_queue = graph.GetAllDependenciesFrom(pos);

        for (const Position p : cleaning_queue)
        {
            ClearCash(p);
        }

        graph.RemoveCell(pos);

        positions.erase(pos);
    }
}

Size Sheet::GetPrintableSize() const
{
    if (positions.size() == 0)
        return {0, 0};

    Size size;
    Position max = *positions.begin();
    for (const Position& p : positions)
    {
        if (p.row > max.row)
            max.row = p.row;
        if (p.col > max.col)
            max.col = p.col;
    }
    size.rows = max.row + 1;
    size.cols = max.col + 1;

    return size;
}

std::ostream& operator<<(std::ostream& output, CellInterface::Value v)
{
    if (std::holds_alternative<std::string>(v))
        output << std::get<std::string>(v);
    else if (std::holds_alternative<double>(v))
        output << std::get<double>(v);
    else if (std::holds_alternative<FormulaError>(v))
        output << std::get<FormulaError>(v);

    return output;
}
void Sheet::PrintValues(std::ostream& output) const
{
    if (cells.size() == 0)
        return;

    Size size = GetPrintableSize();

    for (int i = 0; i < size.rows; i++)
    {
        for (int j = 0; j < size.cols; j++)
        {
            if (j < (int)cells[i].size())
            {
                if (cells[i][j])
                    output << cells[i][j]->GetValue();
            }
            if (j != size.cols - 1)
                output << '\t';
        }
        output << '\n';
    }
}
void Sheet::PrintTexts(std::ostream& output) const
{
    if (cells.size() == 0)
        return;

    Size size = GetPrintableSize();

    for (int i = 0; i < size.rows; i++)
    {
        for (int j = 0; j < size.cols; j++)
        {
            if (j < (int)cells[i].size())
            {
                if (cells[i][j])
                    output << cells[i][j]->GetText();
            }
            if (j != size.cols - 1)
                output << '\t';
        }
        output << '\n';
    }
}

void Sheet::ClearCash(Position pos)
{
    if (!pos.IsValid())
        return;

    if ((int)cells.size() <= pos.row)
        return;
    if ((int)cells[pos.row].size() <= pos.col)
        return;

    if (cells[pos.row][pos.col].get())
        cells[pos.row][pos.col].get()->ClearCash();
}

bool Sheet::CheckPosition(Position pos) const
{
    if (!pos.IsValid())
        throw InvalidPositionException("");

    if ((int)cells.size() <= pos.row)
        return false;
    else if ((int)cells[pos.row].size() <= pos.col)
        return false;
    else
        return true;
}

std::unique_ptr<SheetInterface> CreateSheet()
{
    return std::make_unique<Sheet>();
}