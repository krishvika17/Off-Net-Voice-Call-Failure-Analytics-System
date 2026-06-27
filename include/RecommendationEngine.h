#pragma once

#include <string>
#include <vector>
#include "AnalyticsEngine.h"   // AnalyticsResult, CauseCodeStat, OperatorStat, KPISummary

// =============================================================================
// RecommendationEngine.h
// Save to: include/RecommendationEngine.h
//
// Generates rule-based telecom recommendations
// using AnalyticsEngine results.
//
// Rules follow ISUP Q.850 telecom best practices:
//   routing, congestion, trunk/POI capacity, signalling, configuration,
//   maintenance, and hardware investigation.
//
// Only failure cause codes (outside 16–31) generate recommendations.
// Results are returned as plain structs — no file I/O, no JSON.
//
// Project conventions:
//   - Cause codes 16–31  → normal outcomes (excluded)
//   - Cause codes 1–15 and 32–127 → failures (acted upon)
//   - STL only, C++17
// =============================================================================


// -----------------------------------------------------------------------------
// Priority — how urgently the NOC should act on this recommendation.
// Derived from the contribution percentage of each failure cause code.
// -----------------------------------------------------------------------------
enum class Priority { LOW, MEDIUM, HIGH };

inline std::string priorityToString(Priority p) {
    switch (p) {
        case Priority::LOW:    return "Low";
        case Priority::MEDIUM: return "Medium";
        case Priority::HIGH:   return "High";
    }
    return "Unknown";
}


// -----------------------------------------------------------------------------
// Recommendation
// One actionable item returned to the caller / dashboard.
// -----------------------------------------------------------------------------
struct Recommendation {
    int         cause_code          = 0;
    std::string cause_description;

    Priority    priority            = Priority::LOW;
    std::string suggested_action;   // Telecom best-practice action
    std::string expected_impact;    // What improvement is expected
    double      contribution_pct    = 0.0;  // share of total failed calls

    // Operators that show elevated failure rates for this cause code
    std::vector<std::string> affected_operators;
};


// -----------------------------------------------------------------------------
// RecommendationReport
// Top-level result returned by RecommendationEngine::generate().
// -----------------------------------------------------------------------------
struct RecommendationReport {
    std::vector<Recommendation> recommendations;  // sorted by priority desc
    int  total_recommendations = 0;
    int  high_priority_count   = 0;
    int  medium_priority_count = 0;
    int  low_priority_count    = 0;
};


// -----------------------------------------------------------------------------
// RecommendationEngine
// -----------------------------------------------------------------------------
class RecommendationEngine {
public:
    // -------------------------------------------------------------------------
    // Constructor
    // analytics : result from AnalyticsEngine::compute()
    // anomalies : report from AnomalyDetector::analyse() or analyseTopN()
    //
    // Only cause codes present in analytics.cause_code_stats (i.e. those that
    // passed the contribution threshold) receive recommendations.
    // -------------------------------------------------------------------------
    RecommendationEngine(
    const AnalyticsResult& analytics
    );
    // -------------------------------------------------------------------------
    // generate
    // Applies all rules and returns a RecommendationReport.
    // Safe to call multiple times (stateless between calls).
    // -------------------------------------------------------------------------
    RecommendationReport generate() const;

private:
    const AnalyticsResult& analytics_;

    // -------------------------------------------------------------------------
    // Rule helpers
    // -------------------------------------------------------------------------
    // Returns Severity::LOW and sets found=false if no anomaly exists.
    // Derive priority from contribution percentage.
    static Priority derivePriority(double contribution_pct);
    // Build the telecom suggested_action string from the cause code.
    // Based on ITU-T Q.850 / Dialogic ISUP cause code classifications.
    static std::string suggestedAction(int cause_code,
                                       const std::string& cause_description);

    // Build the expected_impact string that pairs with the suggested action.
    static std::string expectedImpact(int cause_code);

    // Collect operator names whose failure_rate_pct exceeds the overall rate
    // — these are flagged as "affected" for this recommendation.
    std::vector<std::string> affectedOperators(
        double overall_failure_rate_pct) const;

    // True for failure cause codes (outside 16–31 inclusive)
    static bool isFailureCode(int code);
};
