// =============================================================================
// DatasetParser.cpp
// CSV Implementation
// =============================================================================

#include "DatasetParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
// Required Columns
// =============================================================================

const std::vector<std::string> DatasetParser::REQUIRED_COLUMNS =
{
    "Date",
    "Operator",
    "Circle",
    "POI_Type",
    "POI_Name",
    "Direction",
    "Trunk_Group",
    "Cause_Code",
    "Cause_Description",
    "Call_Count"
};
const std::vector<std::string> DatasetParser::OPTIONAL_COLUMNS =
{
};

namespace ReqIdx
{
    constexpr int DATE = 0;
    constexpr int OPERATOR = 1;
    constexpr int CIRCLE = 2;
    constexpr int POI_TYPE = 3;
    constexpr int POI_NAME = 4;
    constexpr int DIRECTION = 5;
    constexpr int TRUNK_GROUP = 6;
    constexpr int CAUSE_CODE = 7;
    constexpr int CAUSE_DESCRIPTION = 8;
    constexpr int CALL_COUNT = 9;
}

// =============================================================================
// Constructor
// =============================================================================

DatasetParser::DatasetParser(const std::string& filePath)
    : filePath_(filePath)
{
}

// =============================================================================
// trim
// =============================================================================

std::string DatasetParser::trim(const std::string& s)
{
    if (s.empty())
        return {};

    auto begin = std::find_if_not(
        s.begin(),
        s.end(),
        [](unsigned char c)
        {
            return std::isspace(c);
        });

    auto end = std::find_if_not(
        s.rbegin(),
        s.rend(),
        [](unsigned char c)
        {
            return std::isspace(c);
        }).base();

    if (begin >= end)
        return {};

    return std::string(begin, end);
}

// =============================================================================
// normalise
// =============================================================================

std::string DatasetParser::normalise(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    for (unsigned char c : s)
    {
        if (c == '_' || c == ' ')
            continue;

        out.push_back(
            static_cast<char>(std::tolower(c)));
    }

    return out;
}

// =============================================================================
// resolveColumns
// =============================================================================

void DatasetParser::resolveColumns(
    const std::vector<std::string>& headers,
    const std::vector<std::string>& columnList,
    std::vector<int>& outPositions,
    std::vector<std::string>& missingColumns,
    bool required) const
{
    outPositions.assign(columnList.size(), -1);

    std::vector<std::string> normHeaders;
    normHeaders.reserve(headers.size());

    for (const auto& h : headers)
        normHeaders.push_back(normalise(h));

    for (size_t i = 0; i < columnList.size(); ++i)
    {
        std::string target = normalise(columnList[i]);

        for (size_t j = 0; j < normHeaders.size(); ++j)
        {
            if (target == normHeaders[j])
            {
                outPositions[i] = static_cast<int>(j);
                break;
            }
        }

        if (required && outPositions[i] == -1)
            missingColumns.push_back(columnList[i]);
    }
}

// =============================================================================
// splitCSVLine
// Supports quoted CSV fields containing commas.
// Reuses the supplied vector to minimize allocations.
// =============================================================================

void DatasetParser::splitCSVLine(
    const std::string& line,
    std::vector<std::string>& fields)
{
    fields.clear();

    std::string current;
    current.reserve(64);

    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i)
    {
        char c = line[i];

        if (c == '"')
        {
            // Escaped quote ("")
            if (inQuotes &&
                i + 1 < line.size() &&
                line[i + 1] == '"')
            {
                current.push_back('"');
                ++i;
            }
            else
            {
                inQuotes = !inQuotes;
            }
        }
        else if (c == ',' && !inQuotes)
        {
            fields.emplace_back(trim(current));
            current.clear();
        }
        else
        {
            current.push_back(c);
        }
    }

    fields.emplace_back(trim(current));
}

// =============================================================================
// parseIntField
// =============================================================================

bool DatasetParser::parseIntField(
    const std::string& text,
    int& value)
{
    if (text.empty())
        return false;

    try
    {
        size_t pos = 0;
        value = std::stoi(text, &pos);

        return pos == text.size();
    }
    catch (...)
    {
        return false;
    }
}

// =============================================================================
// parseLLField
// =============================================================================
bool DatasetParser::parseLLField(
    const std::string& text,
    long long& value)
{
    if (text.empty())
        return false;

    try
    {
        size_t pos = 0;
        value = std::stoll(text, &pos);

        return pos == text.size();
    }
    catch (...)
    {
        return false;
    }
}

// =============================================================================
// parse
// =============================================================================

std::vector<CallRecord> DatasetParser::parse()
{
    totalRows_ = 0;
    parsedRows_ = 0;
    skippedRows_ = 0;

    std::ifstream file(filePath_);
    file.rdbuf()->pubsetbuf(nullptr, 1 << 20);
    if (!file.is_open())
    {
        throw std::runtime_error(
            "DatasetParser: Cannot open CSV file '" +
            filePath_ + "'");
    }

    std::vector<CallRecord> records;
    records.reserve(200000);

    std::string line;
    std::vector<std::string> fields;
    fields.reserve(16);

    // ---------------------------------------------------------
    // Read Header
    // ---------------------------------------------------------

    if (!std::getline(file, line))
    {
        throw std::runtime_error(
            "DatasetParser: CSV file is empty.");
    }

    splitCSVLine(line, fields);

    std::vector<std::string> headers = fields;

    std::vector<int> reqPos;

    std::vector<std::string> missingColumns;

    resolveColumns(
        headers,
        REQUIRED_COLUMNS,
        reqPos,
        missingColumns,
        true);

    if (!missingColumns.empty())
    {
        std::string msg =
            "Missing required columns: ";

        for (size_t i = 0;
             i < missingColumns.size();
             ++i)
        {
            if (i)
                msg += ", ";

            msg += missingColumns[i];
        }

        throw std::runtime_error(msg);
    }

    // ---------------------------------------------------------
    // Begin parsing data rows
    // ---------------------------------------------------------
    auto fieldAt = [&](int index) -> const std::string&
        {
            static const std::string empty{};

            if (index < 0)
                return empty;

            if (index >= static_cast<int>(fields.size()))
                return empty;

            return fields[index];
        };
    while (std::getline(file, line))
    {
        ++totalRows_;

        fields.clear();
        splitCSVLine(line, fields);

        // Ignore completely empty rows

        if (fields.empty())
            continue;

        CallRecord rec;

        bool rowValid = true;

        // ---------------------------------------------------------
        // Ensure row has enough columns
        // ---------------------------------------------------------

        // ---------------------------------------------------------
        // Read required string fields
        // ---------------------------------------------------------

        rec.date              = fieldAt(reqPos[ReqIdx::DATE]);
        rec.operator_name     = fieldAt(reqPos[ReqIdx::OPERATOR]);
        rec.circle            = fieldAt(reqPos[ReqIdx::CIRCLE]);
        rec.poi_type          = fieldAt(reqPos[ReqIdx::POI_TYPE]);
        rec.poi_name          = fieldAt(reqPos[ReqIdx::POI_NAME]);
        rec.direction         = fieldAt(reqPos[ReqIdx::DIRECTION]);
        rec.trunk_group       = fieldAt(reqPos[ReqIdx::TRUNK_GROUP]);
        rec.cause_description = fieldAt(reqPos[ReqIdx::CAUSE_DESCRIPTION]);

        // ---------------------------------------------------------
        // Validate mandatory strings
        // ---------------------------------------------------------

        if (rec.date.empty())
        {
            rowValid = false;
        }
        else if (rec.operator_name.empty())
        {
            rowValid = false;
        }
        else if (rec.circle.empty())
        {
            rowValid = false;
        }
        else if (rec.poi_type.empty())
        {
            rowValid = false;
        }
        else if (rec.poi_name.empty())
        {
            rowValid = false;
        }
        else if (rec.direction.empty())
        {
            rowValid = false;
        }
        else if (rec.trunk_group.empty())
        {
            rowValid = false;
        }

        // ---------------------------------------------------------
        // Cause Code
        // ---------------------------------------------------------

        if (rowValid)
        {
            if (!parseIntField(
                    fieldAt(reqPos[ReqIdx::CAUSE_CODE]),
                    rec.cause_code))
            {
                rowValid = false;
            }
            else if (rec.cause_code < 1 ||
                     rec.cause_code > 128)
            {
                rowValid = false;
            }
        }

        // ---------------------------------------------------------
        // Call Count
        // ---------------------------------------------------------

        if (rowValid)
        {
            if (!parseLLField(
                    fieldAt(reqPos[ReqIdx::CALL_COUNT]),
                    rec.call_count))
            {
                rowValid = false;
            }
            else if (rec.call_count < 0)
            {
                rowValid = false;
            }
        }

        // ---------------------------------------------------------
        // Accept / Reject
        // ---------------------------------------------------------

        if (rowValid)
        {
            records.emplace_back(std::move(rec));
            ++parsedRows_;
        }
        else
        {
            ++skippedRows_;
        }

    }   // End while()

    return records;
}
