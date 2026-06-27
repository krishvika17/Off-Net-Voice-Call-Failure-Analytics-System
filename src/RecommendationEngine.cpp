// =============================================================================
// RecommendationEngine.cpp
// Save to: src/RecommendationEngine.cpp
// =============================================================================

#include "RecommendationEngine.h"
#include <algorithm>
#include <string>
#include <vector>

// =============================================================================
// Constructor
// =============================================================================
RecommendationEngine::RecommendationEngine(
    const AnalyticsResult& analytics)
    : analytics_(analytics)
{
}
// =============================================================================
// isFailureCode
// Codes 16–31 inclusive are normal outcomes; everything else is a failure.
// =============================================================================
bool RecommendationEngine::isFailureCode(int code) {
    return code < 16 || code > 31;
}

// =============================================================================
// derivePriority
//
// Assigns recommendation priority based on the contribution percentage of
// each failure cause code.
//
// Higher contribution indicates a greater impact on the overall failure rate,
// therefore higher priority is assigned for corrective action.
//
// Priority Rules:
//   Contribution >= 15%  -> High
//   Contribution >= 10%  -> Medium
//   Contribution < 10%   -> Low
// =============================================================================

Priority RecommendationEngine::
derivePriority(double contribution)
{
    if (contribution >= 0.20)
        return Priority::HIGH;

    if (contribution >= 0.10)
        return Priority::MEDIUM;

    return Priority::LOW;
}

// =============================================================================
// suggestedAction
//
// Rule table built from ITU-T Q.850 cause code semantics and standard
// telecom NOC remediation practices (routing, congestion, trunk/POI capacity,
// signalling, configuration, maintenance, hardware investigation).
//
// Cause code groups:
//   1–15   : Routing / addressing failures
//   32–47  : Resource unavailability / congestion
//   48–63  : Service or option not available
//   64–79  : Service or option not implemented
//   80–95  : Invalid message / protocol error at user side
//   96–111 : Protocol error at network side
//   112–127: Interworking / unknown failures
// =============================================================================
std::string RecommendationEngine::suggestedAction(
        int cause_code, const std::string& cause_description)
{
    // ----- Group: Routing and Addressing Failures (1–15) ---------------------
    
    if (cause_code >= 1 && cause_code <= 15)
        return "Investigate routing and addressing configuration for cause '"
               + cause_description + "'. Audit translation tables, trunk group "
               "routing entries, and interconnect signalling parameters.";

    // ----- Group: Resource Unavailability / Congestion (32–47) --------------
  
    if (cause_code >= 32 && cause_code <= 47)
        return "Analyse capacity and congestion for cause '"
               + cause_description + "'. Review trunk group occupancy, add "
               "circuits on high-traffic routes, and enable overflow routing.";

    // ----- Group: Service or Option Not Available (48–63) --------------------

    if (cause_code >= 48 && cause_code <= 63)
        return "Investigate service provisioning for cause '"
               + cause_description + "'. Audit subscriber service profiles, "
               "class of service settings, and feature activation on all "
               "affected network nodes.";

    // ----- Group: Service or Option Not Implemented (64–79) -----------------

    if (cause_code >= 64 && cause_code <= 79)
        return "Investigate capability mismatch for cause '"
               + cause_description + "'. Align feature and bearer service "
               "configurations across all network elements on the affected route.";

    // ----- Group: Invalid Message / User Error (80–95) -----------------------

    if (cause_code >= 80 && cause_code <= 95)
        return "Capture ISUP signalling traces for cause '"
               + cause_description + "'. Identify the node generating invalid "
               "messages and apply configuration or software corrections.";

    // ----- Group: Protocol Error at Network Side (96–111) -------------------

    if (cause_code >= 96 && cause_code <= 111)
        return "Perform ISUP protocol trace for cause '"
               + cause_description + "'. Co-ordinate with the peer operator "
               "to identify and correct the protocol implementation defect.";

    // ----- Group: Interworking (112–127) ------------------------------------

    if (cause_code >= 112 && cause_code <= 127)
        return "Investigate interworking failure for cause '"
               + cause_description + "'. Trace calls across gateway nodes, "
               "verify signalling interoperability, and co-ordinate with the "
               "interconnect partner to isolate the root cause.";

    // ----- Default fallback --------------------------------------------------
    return "Review ISUP signalling configuration for cause code "
           + std::to_string(cause_code) + " ('" + cause_description + "'). "
           "Collect call traces, analyse trunk group statistics, and engage "
           "the network vendor for further diagnosis.";
}

// =============================================================================
// expectedImpact
// Pairs with suggestedAction — describes the measurable improvement expected
// if the action is taken. Grouped by the same cause-code ranges.
// =============================================================================
std::string RecommendationEngine::expectedImpact(int cause_code)
{
    if (cause_code >= 1  && cause_code <= 15)
        return "Reduction in routing and addressing failures; improvement in "
               "call completion rate (CCR) for affected destinations.";

    if (cause_code >= 32 && cause_code <= 47)
        return "Reduction in congestion and resource-blocking failures; "
               "improvement in network-level CSR during peak traffic hours.";

    if (cause_code >= 48 && cause_code <= 63)
        return "Elimination of service-unavailability failures; improved "
               "subscriber experience and reduction in repeat-attempt traffic.";

    if (cause_code >= 64 && cause_code <= 79)
        return "Resolution of bearer or feature incompatibilities; improved "
               "interoperability between network nodes and reduced re-attempts.";

    if (cause_code >= 80 && cause_code <= 95)
        return "Elimination of invalid-message protocol errors; stabilisation "
               "of the affected trunk group and reduction in unexplained drops.";

    if (cause_code >= 96 && cause_code <= 111)
        return "Correction of network-side protocol errors; improved ISUP "
               "signalling compliance and reduction in protocol-failure CDRs.";

    if (cause_code >= 112 && cause_code <= 127)
        return "Resolution of interworking failures; improved end-to-end call "
               "success rate on interconnect routes with the identified partner.";

    return "Improved overall Call Success Rate (CSR) and reduction in "
           "failure call count for the affected cause code.";
}

// =============================================================================
// affectedOperators
// Returns the names of operators whose failure_rate_pct exceeds the overall
// network failure rate — indicating they are disproportionately affected.
// =============================================================================
std::vector<std::string> RecommendationEngine::affectedOperators(
        double overall_failure_rate_pct) const
{
    std::vector<std::string> affected;
    for (const auto& op : analytics_.operator_stats) {
        if (op.failure_rate_pct >= 1.0)
            affected.push_back(op.operator_name);
    }
    return affected;
}

// =============================================================================
// generate
//
// For each major cause code in analytics_.cause_code_stats:
//   1. Look up anomaly severity from the AnomalyReport.
//   2. Derive Priority from contribution % and anomaly severity.
//   3. Build suggested_action and expected_impact via rule tables.
//   4. Tag affected operators.
//   5. Push into the report and sort by priority desc.
// =============================================================================
RecommendationReport RecommendationEngine::generate() const
{
    RecommendationReport report;


    for (const auto& cs : analytics_.cause_code_stats) {
        // Safety guard: only act on failure codes
        if (!isFailureCode(cs.cause_code)) continue;

        Recommendation rec;

        rec.cause_code = cs.cause_code;
        rec.cause_description = cs.cause_description;
        rec.contribution_pct = cs.contribution_pct;

        rec.priority = derivePriority(cs.contribution_pct);

        rec.suggested_action =
            suggestedAction(cs.cause_code,
                            cs.cause_description);

        rec.expected_impact =
            expectedImpact(cs.cause_code);

        rec.affected_operators =
        cs.affected_operators;
        
        report.recommendations.push_back(std::move(rec));
    }

    // Sort descending: HIGH → MEDIUM → LOW, then by contribution_pct desc
    std::sort(report.recommendations.begin(), report.recommendations.end(),
        [](const Recommendation& a, const Recommendation& b) {
            if (a.priority != b.priority)
                return static_cast<int>(a.priority) >
                       static_cast<int>(b.priority);
            return a.contribution_pct > b.contribution_pct;
        });

    // Build summary counts
    report.total_recommendations = static_cast<int>(
        report.recommendations.size());

    for (const auto& r : report.recommendations) {
        switch (r.priority) {
            case Priority::HIGH:   ++report.high_priority_count;   break;
            case Priority::MEDIUM: ++report.medium_priority_count; break;
            case Priority::LOW:    ++report.low_priority_count;    break;
        }
    }

    return report;
}
