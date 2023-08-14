#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>

Cell::Value Cell::CellContent::GetValue(Sheet&) const
{
	return "";
}
std::string Cell::CellContent::GetText() const
{
	return "";
}
std::vector<Position> Cell::CellContent::GetReferencedCells() const
{
	return std::vector<Position>();
}

Cell::Value Cell::TextCell::GetValue(Sheet&) const
{
	if (text.empty())
		return text;

	if (text[0] == '\'')
		return text.substr(1);
	else
		return text;
}
std::string Cell::TextCell::GetText() const
{
	return text;
}
std::vector<Position> Cell::TextCell::GetReferencedCells() const
{
	return std::vector<Position>();
}

Cell::Value Cell::NumberCell::GetValue(Sheet& sheet) const
{
	return value;
}
std::string Cell::NumberCell::GetText() const
{
	std::string str = std::to_string(value);
	str.erase(str.find_last_not_of('0') + 1, std::string::npos);
	str.erase(str.find_last_not_of('.') + 1, std::string::npos);
	return str;
}
std::vector<Position> Cell::NumberCell::GetReferencedCells() const
{
	return std::vector<Position>();
}

Cell::Value Cell::FormulaCell::GetValue(Sheet& sheet) const
{
	auto value = formula->Evaluate(sheet);
	if (std::holds_alternative<double>(value))
		return std::get<double>(value);
	else
		return std::get<FormulaError>(value);
}
std::string Cell::FormulaCell::GetText() const
{
	return "=" + formula->GetExpression();
}
std::vector<Position> Cell::FormulaCell::GetReferencedCells() const
{
	return formula->GetReferencedCells();
}

void Cell::Set(std::string text)
{
	std::unique_ptr<CellContent> prev_content(content.release());
	if (text.empty())
	{
		content = std::unique_ptr<CellContent>(new CellContent());
	}
	else if (text[0] == '=' && text.length() != 1)
	{
		try
		{
			content = std::unique_ptr<CellContent>(new FormulaCell(ParseFormula(text.substr(1))));
		}
		catch (const std::exception& exc)
		{
			content = move(prev_content);
			std::throw_with_nested(FormulaException(exc.what()));
		}
	}
	else
	{
			char* end;
			double value = std::strtod(text.data(), &end);
			if (*end == '\0')
				content = std::unique_ptr<CellContent>(new NumberCell(value));
			else
				content = std::unique_ptr<CellContent>(new TextCell(std::move(text)));
	}
	prev_content.release();
}
void Cell::Clear()
{
	content.release();
	content = std::unique_ptr<CellContent>(new CellContent());
}

Cell::Value Cell::GetValue() const
{
	if (cash.has_value())
		return cash.value();

	CellInterface::Value value = content->GetValue(sheet_ref);
	cash = value;

	return value;
}
std::string Cell::GetText() const
{
	return content->GetText();
}
std::vector<Position> Cell::GetReferencedCells() const
{
	return content->GetReferencedCells();
}

void Cell::ClearCash()
{
	cash = {};
}
