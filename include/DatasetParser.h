#pragma once

#include <string>
#include <vector>

#include "CallRecord.h"

class DatasetParser
{
public:
    // Constructor
    explicit DatasetParser(const std::string& filePath);
    // Parse an Off-Net (Inter-Operator) CSV dataset
    std::vector<CallRecord> parse();

    // Statistics
    int totalRows() const { return totalRows_; }
    int parsedRows() const { return parsedRows_; }
    int skippedRows() const { return skippedRows_; }

    const std::string& filePath() const { return filePath_; }

private:

    // ==========================================================
    // CSV Helpers
    // ==========================================================

    static void splitCSVLine(
        const std::string& line,
        std::vector<std::string>& fields);

    static bool parseIntField(
        const std::string& text,
        int& value);

    static bool parseLLField(
        const std::string& text,
        long long& value);

    // ==========================================================
    // Utility Functions
    // ==========================================================

    static std::string trim(
        const std::string& s);

    static std::string normalise(
        const std::string& s);

    void resolveColumns(
        const std::vector<std::string>& headers,
        const std::vector<std::string>& columnList,
        std::vector<int>& outPositions,
        std::vector<std::string>& missingColumns,
        bool required) const;

    // ==========================================================
    // Member Variables
    // ==========================================================

    std::string filePath_;

    int totalRows_ = 0;
    int parsedRows_ = 0;
    int skippedRows_ = 0;

    // ==========================================================
    // Required CSV Columns
    // ==========================================================

    static const std::vector<std::string> REQUIRED_COLUMNS;

    // ==========================================================
    // Optional CSV Columns(Currently None)
    // ==========================================================

    static const std::vector<std::string> OPTIONAL_COLUMNS;
};