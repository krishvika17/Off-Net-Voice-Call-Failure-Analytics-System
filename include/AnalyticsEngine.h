#pragma once

#include <string>
#include <vector>
#include "CallRecord.h"
#include "FilterEngine.h"   // for FilterCriteria::contribution_threshold_pct

// =============================================================================
// AnalyticsEngine.h
// Save to: include/AnalyticsEngine.h
//
// Computes call failure analytics over a filtered vector<CallRecord>.
// Failure cause codes are defined as codes OUTSIDE the range 16–31 inclusive.
//
// All results are returned through plain structs — no file I/O here.
// The dashboard (Streamlit) and exporters consume these structs downstream.
//
// Project conventions:
//   - Cause codes 16–31  → normal outcomes  (excluded from all failure KPIs)
//   - Cause codes 1–15 and 32–127 → failures
//   - contribution_threshold_pct comes from FilterCriteria (default 0.05 %)
//   - STL only, C++17
// =============================================================================


// -----------------------------------------------------------------------------
// KPISummary
// Top-level headline numbers for the dashboard KPI cards.
// -----------------------------------------------------------------------------
struct KPISummary {
    long long total_calls        = 0;   // sum of call_count across all records
    long long failed_calls       = 0;   // sum of call_count for failure codes
    long long normal_calls       = 0;   // total_calls - failed_calls
    double    failure_rate_pct   = 0.0; // (failed_calls / total_calls) * 100
    double    csr_pct            = 0.0; // Call Success Rate = 100 - failure_rate_pct
    int       total_cause_codes  = 0;   // distinct failure cause codes seen
    int       major_cause_codes  = 0;   // distinct codes >= contribution_threshold_pct
};


// -----------------------------------------------------------------------------
// CauseCodeStat
// Per-cause-code failure breakdown — drives the Streamlit bar chart.
// Sorted descending by failed_call_count before being stored in AnalyticsResult.
// Only codes whose contribution >= contribution_threshold_pct are included.
// -----------------------------------------------------------------------------
struct CauseCodeStat
{
    int cause_code = 0;

    std::string cause_description;

    long long failed_call_count = 0;

    double contribution_pct = 0.0;

    std::vector<std::string> affected_operators;
};

// -----------------------------------------------------------------------------
// TrunkGroupStat
// Stores failure contribution for each Off-Net trunk group.
// -----------------------------------------------------------------------------
struct TrunkGroupStat
{
    std::string trunk_group;

    long long total_calls = 0;

    long long failed_calls = 0;

    double failure_rate_pct = 0.0;

    double contribution_pct = 0.0;
};

// -----------------------------------------------------------------------------
// OperatorStat
// Per-operator failure summary — drives the operator comparison section.
// -----------------------------------------------------------------------------
struct OperatorStat {
    std::string operator_name;
    long long   total_calls      = 0;
    long long   failed_calls     = 0;
    double      failure_rate_pct = 0.0;
    double      csr_pct          = 0.0;
};


// -----------------------------------------------------------------------------
// TrendPoint
// One point on the date-vs-failed-calls line graph.
// Dates are returned in ascending chronological order.
// -----------------------------------------------------------------------------
struct DailyCauseTrend
{
    std::string date;

    int cause_code = 0;

    std::string cause_description;

    long long failed_calls = 0;

    long long total_calls = 0;

    double contribution_pct = 0.0;

    std::vector<TrunkGroupStat> trunk_groups;
};

struct TrendPoint
{
    std::vector<DailyCauseTrend> daily_causes;
};

// -----------------------------------------------------------------------------
// AnalyticsResult
// Single aggregate returned by AnalyticsEngine::compute().
// Every downstream consumer (exporter, dashboard bridge) reads from here.
// -----------------------------------------------------------------------------
struct AnalyticsResult
{
    KPISummary kpi;

    std::vector<CauseCodeStat> cause_code_stats;

    std::vector<OperatorStat> operator_stats;

    std::vector<TrendPoint> trend;

    double contribution_threshold_pct = 0.05;
};


// -----------------------------------------------------------------------------
// AnalyticsEngine
// -----------------------------------------------------------------------------
class AnalyticsEngine {
public:
    // -------------------------------------------------------------------------
    // Constructor
    // records : filtered vector produced by FilterEngine::apply()
    // threshold_pct : from FilterCriteria::contribution_threshold_pct
    //                 cause codes below this % share are excluded from
    //                 cause_code_stats (but still counted in KPIs).
    // -------------------------------------------------------------------------
    explicit AnalyticsEngine(const std::vector<CallRecord>& records,
                             double threshold_pct = 0.05);

    // -------------------------------------------------------------------------
    // compute
    // Runs all analytics in a single pass where possible and returns the
    // populated AnalyticsResult.  Can be called multiple times safely
    // (stateless between calls — all state lives in the returned struct).
    // -------------------------------------------------------------------------
    AnalyticsResult compute() const;

private:
    const std::vector<CallRecord>& records_;
    double threshold_pct_;

    // -------------------------------------------------------------------------
    // Private calculation helpers
    // -------------------------------------------------------------------------

    // Populate kpi and raw per-cause totals in one pass over records
    void   computeAnalytics(
               KPISummary&                          kpi,
               std::vector<CauseCodeStat>&          causeStats,
               std::vector<OperatorStat>&           opStats,
               std::vector<TrendPoint>&             trend) const;

    // Finalise percentages, sort, apply threshold filter
    // -------------------------------------------------------------------------
    // Computes Off-Net trunk group statistics.
    // Populates AnalyticsResult::trunk_group_stats.
    // -------------------------------------------------------------------------
  
    void   finaliseResults(AnalyticsResult& result) const;

    // Return true for failure cause codes (outside 16–31 inclusive)
    static bool isFailureCode(int code);
};
