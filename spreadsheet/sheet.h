#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <vector>
#include <set>
#include <map>
#include <optional>

class Cell;

class DependeciesGraph
{
public:
    std::set<Position> GetAllDependenciesFrom(Position from) const;
    void AddEdges(Position to, const std::vector<Position>& from);

    void RemoveCell(Position pos);

private:
    std::set<Position> GetAllDependenciesFrom(Position from, std::set<Position>& visited) const;

    std::map<Position, std::set<Position>> edges;
    std::map<Position, std::set<Position>> reversed_edges;
};
class Sheet : public SheetInterface
{
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    void ClearCash(Position pos);

private:
    bool CheckPosition(Position pos) const;

    DependeciesGraph graph;
    std::set<Position> positions;
    std::vector<std::vector<std::unique_ptr<Cell>>> cells;
};