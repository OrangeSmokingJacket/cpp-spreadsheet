#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <vector>
#include <set>

class DependeciesGraph
{
public:
    bool TryAddEdges(Position to, std::vector<Position> from); // does not change graph if there is an error
    bool TryAddEdge(std::pair<Position, Position> from_to); // does not change graph if there is reversed edge
    std::vector<Position> GetDependecies(Position from) const; // returns vector of Positions that depended on this Position

private:
    std::vector<std::pair<Position, Position>> edges;
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

private:
    bool CheckPosition(Position pos) const;
    bool HasCircularDependencies(Position pos) const;

    std::set<Position> positions;
    std::vector<std::vector<std::unique_ptr<CellInterface>>> cells;
    std::unordered_map<Position, CellInterface::Value> cash;
};