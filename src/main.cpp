// =============================================================================
// main.cpp
// Save to: src/main.cpp
//
// Voice-Call-Failure-Analytics-System — backend entry point.
//
// Collects all inputs from std::cin so Streamlit (or any caller) can later
// pipe values in without touching this file.
//
// Pipeline:
//   DatasetParser → FilterEngine → AnalyticsEngine→ RecommendationEngine
// =============================================================================

#include "CallRecord.h"
#include "DatasetParser.h"
#include "FilterEngine.h"
#include "AnalyticsEngine.h"
#include "RecommendationEngine.h"

// #include <chrono>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// readLine — read one trimmed line from stdin
// -----------------------------------------------------------------------------
static std::string readLine(const std::string &prompt)
{
    std::string line;

    std::cout << prompt << '\n';

    if (!std::getline(std::cin, line))
    {
        std::cin.clear();
        return "";
    }

    const auto b = line.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return "";

    const auto e = line.find_last_not_of(" \t\r\n");
    return line.substr(b, e - b + 1);
}
// readList — comma-separated values → vector<string>
// Empty input returns an empty vector (= no filter for that dimension).
static std::vector<std::string> readList(const std::string &prompt)
{
    const std::string raw = readLine(prompt);
    std::vector<std::string> out;
    if (raw.empty())
        return out;
    std::istringstream ss(raw);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        const auto b = token.find_first_not_of(" \t");
        const auto e = token.find_last_not_of(" \t");
        if (b != std::string::npos)
            out.push_back(token.substr(b, e - b + 1));
    }
    return out;
}

// readDouble — returns defaultVal on empty input or parse failure
static double readDouble(const std::string &prompt, double defaultVal)
{
    const std::string raw = readLine(prompt);
    if (raw.empty())
        return defaultVal;
    try
    {
        return std::stod(raw);
    }
    catch (...)
    {
        return defaultVal;
    }
}

// =============================================================================
// main
// =============================================================================
int main()
{

    std::cout << "================================================\n"
              << " Off-Net Voice Call Failure Analytics System\n"
              << "================================================\n\n"
              << "Press Enter to accept the default value shown in [brackets].\n\n";

    // -------------------------------------------------------------------------
    // 1. Collect inputs
    // -------------------------------------------------------------------------
    std::cin.clear();
    std::string csvPath;

    std::cout << "Enter CSV dataset path:\n";
    std::getline(std::cin, csvPath);

    if (csvPath.empty())
    {
        std::cerr << "Error: CSV file path is required.\n";
        return 1;
    }

    std::cout << "\n-- Off-Net Call Filters (comma-separated values; press Enter to skip) --\n";
    const auto operators = readList("Operators (Jio, Airtel, Vodafone, BSNL): ");
    const auto circles = readList("Circles     (e.g. Punjab, Haryana, Rajasthan, Uttar Pradesh): ");
    const auto poi_types = readList("POI Types   (e.g. Local, NLD, ILD): ");
    const auto directions = readList("Directions  (Incoming, Outgoing): ");
    const auto dateFrom = readLine("Date from   DD-MM-YYYY [unbounded]: ");
    const auto dateTo = readLine("Date to     DD-MM-YYYY [unbounded]: ");

    const double threshold =
        readDouble("Major Failure Contribution Threshold (%) [0.05]: ", 0.05);
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // 2. DatasetParser
    // -------------------------------------------------------------------------
    std::vector<CallRecord> allRecords;
    try
    {

        std::cout << "Loading CSV dataset...\n";
        DatasetParser parser(csvPath);
        allRecords = parser.parse();

        std::cout << "Dataset : "
                  << parser.parsedRows()
                  << " rows parsed, "
                  << parser.skippedRows()
                  << " skipped\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading dataset: " << e.what() << "\n";
        return 1;
    }

    if (allRecords.empty())
    {
        std::cerr << "Error: no valid records found.\n";
        return 1;
    }

    // -------------------------------------------------------------------------
    // 3. FilterEngine
    // -------------------------------------------------------------------------
    FilterCriteria fc;
    fc.operators = operators;
    fc.circles = circles;
    fc.poi_types = poi_types;
    fc.directions = directions;
    fc.date_from = dateFrom;
    fc.date_to = dateTo;
    fc.contribution_threshold_pct = threshold;

    FilterEngine filterEngine(allRecords);
    const std::vector<CallRecord> filtered = filterEngine.apply(fc);

    std::cout << "Records loaded   : " << allRecords.size() << "\n";
    std::cout << "Records filtered : " << filtered.size() << "\n";

    if (filtered.empty())
    {
        std::cerr << "Warning: no records match the applied filters.\n";
        return 0;
    }

    // -------------------------------------------------------------------------
    // 4. AnalyticsEngine
    // -------------------------------------------------------------------------
    std::cout << "\n===== DEBUG : FILTERED CC57 (12-06-2026) =====\n";

    long long total = 0;

    for (const auto &r : filtered)
    {
        if (r.date == "12-06-2026" &&
            r.cause_code == 57)
        {
            std::cout
                << r.trunk_group
                << " | Calls = "
                << r.call_count
                << "\n";

            total += r.call_count;
        }
    }

    std::cout << "TOTAL CC57 = " << total << "\n";
    AnalyticsEngine analyticsEngine(filtered, threshold);
    const AnalyticsResult analytics = analyticsEngine.compute();
    const KPISummary &kpi = analytics.kpi;

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "\n========== KPI Summary ==========\n"
              << "Total Calls          : " << kpi.total_calls << "\n"
              << "Failed Calls         : " << kpi.failed_calls << "\n"
              << "Call Success Rate    : " << kpi.csr_pct << " %\n"
              << "Failure Rate         : " << kpi.failure_rate_pct << " %\n"
              << "Major Cause Codes    : " << kpi.major_cause_codes << "\n";
              if (kpi.csr_pct >= 99.5)
            {
                std::cout << "Network Status      : Healthy\n";
            }
            else
            {
                std::cout << "Network Status      : Needs Attention\n";
            }

    std::cout << "\n-- Major Failure Causes Above Selected Threshold --\n";
    for (const auto &cs : analytics.cause_code_stats)
    {
        std::cout << "  CC " << std::setw(3) << cs.cause_code
                  << "  |  " << std::setw(12) << cs.failed_call_count << " calls"
                  << "  |  " << std::setw(6) << cs.contribution_pct << " %"
                  << "  |  " << cs.cause_description << "\n";
    }
    
    std::cout << "\n-- Operator Statistics --\n";

for (const auto &op : analytics.operator_stats)
{
    std::cout
        << "Operator      : " << op.operator_name << "\n"
        << "Total Calls   : " << op.total_calls << "\n"
        << "Failed Calls  : " << op.failed_calls << "\n"
        << "Failure Rate  : " << op.failure_rate_pct << " %\n"
        << "CSR           : " << op.csr_pct << " %\n\n";
}

std::cout << "\n========== Daily Cause Code Trend ==========\n";

for (const auto &day : analytics.trend)
{
    for (const auto &cause : day.daily_causes)
    {
        std::cout
        << "Date=" << cause.date
        << ",Cause=" << cause.cause_code
        << ",Contribution=" << std::fixed
        << std::setprecision(2)
        << cause.contribution_pct
        << ",FailedCalls=" << cause.failed_calls
        << ",TotalCalls=" << cause.total_calls
        << ",Trunks=";
        if (cause.trunk_groups.empty())
        {
            std::cout << "None";
        }
        else
        {
            for (size_t i = 0; i < cause.trunk_groups.size(); ++i)
            {
                if (i)
                    std::cout << " | ";

                std::cout
                << cause.trunk_groups[i].trunk_group
                << "["
                << cause.trunk_groups[i].failed_calls
                << "/"
                << cause.trunk_groups[i].total_calls
                << " = "
                << std::fixed
                << std::setprecision(2)
                << cause.trunk_groups[i].contribution_pct
                << "%]";
            }
        }

        std::cout << "\n";
    }
}

    // -------------------------------------------------------------------------
    // 5. RecommendationEngine
    // -------------------------------------------------------------------------

    RecommendationEngine recEngine(analytics);
    const RecommendationReport recReport = recEngine.generate();

    std::cout << "\n========== Recommendations ==========\n"
              << "Total Recommendations: " << recReport.total_recommendations << "\n"
              << "  High   : " << recReport.high_priority_count << "\n"
              << "  Medium : " << recReport.medium_priority_count << "\n"
              << "  Low    : " << recReport.low_priority_count << "\n";

    if (!recReport.recommendations.empty())
    {
        std::cout << "\n-- Recommendation List --\n";
        const int recommendationLimit =
            std::min(10,
                     static_cast<int>(recReport.recommendations.size()));

        for (int i = 0; i < recommendationLimit; i++)
        {
            const auto &r = recReport.recommendations[i];

            std::cout << "\nCC "
                      << r.cause_code
                      << " ["
                      << priorityToString(r.priority)
                      << "]"
                      << "\n";

            std::cout << "Contribution : "
                      << r.contribution_pct
                      << "%\n";
            std::cout << "Affected Operators : ";

            if (r.affected_operators.empty())
            {
                std::cout << "None";
            }
            else
            {
                for (size_t j = 0; j < r.affected_operators.size(); ++j)
                {
                    if (j != 0)
                        std::cout << ", ";

                    std::cout << r.affected_operators[j];
                }
            }

            std::cout << "\n";          

            std::cout << "Description  : "
                      << r.cause_description
                      << "\n";

            std::cout << "Action       : "
                      << r.suggested_action
                      << "\n";
            std::cout << "Impact       : "
                      << r.expected_impact
                      << "\n";
        }

        if (recReport.recommendations.size() > recommendationLimit)
        {
            std::cout
                << "\n..."
                << recReport.recommendations.size() - recommendationLimit
                << " more recommendations hidden.\n";
        }
    }

    return 0;
}
