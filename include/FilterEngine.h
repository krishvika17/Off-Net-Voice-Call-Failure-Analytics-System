#pragma once

#include <string>
#include <vector>
#include "CallRecord.h"

// =============================================================================
// FilterEngine.h
// Save to: include/FilterEngine.h
//
//Filters an Off-Net telecom dataset (std::vector<CallRecord>) by one or more optional criteria.
// All criteria are stored in a FilterCriteria struct.  Any field left at its
// default value is treated as "no filter" for that dimension.
//
// A contribution threshold (%) is carried here for later use by
// AnalyticsEngine; FilterEngine itself does not calculate contributions.
//
// Project conventions:
//   - Cause codes 16–31 are normal outcomes; 1–15 and 32–127 are failures.
//   - All headers use the .h extension.
//   - STL only — no third-party libraries.
// =============================================================================

// -----------------------------------------------------------------------------
// FilterCriteria
//
// All multi-value fields use std::vector<std::string>.
// An empty vector means "accept all values" for that dimension.
// Date strings must match the format found in CallRecord::date ("DD-MM-YYYY").
// -----------------------------------------------------------------------------
struct FilterCriteria {

    // Off-Net filtering dimensions — empty means no filtering
    std::vector<std::string> operators;   // e.g. {"Airtel", "Jio", "BSNL", "Vodafone"}
    std::vector<std::string> circles;     // e.g. {"Punjab","Haryana","Rajasthan","Uttar Pradesh"}
    std::vector<std::string> poi_types;   // e.g. {"Local", "NLD", "ILD"}
    std::vector<std::string> directions;  // e.g. {"Incoming"}, {"Outgoing"}

    // Inclusive date range — empty string means no bound on that end
    std::string date_from;  // "DD-MM-YYYY"  e.g. "01-06-2026"
    std::string date_to;    // "DD-MM-YYYY"  e.g. "30-06-2026"

    // Major Failure Contribution Threshold (%) forwarded to AnalyticsEngine.
    // A cause code whose share of total failure call_count is below this
    // value may be suppressed in analysis output.
    // Default: 0.05  (i.e. 0.05 %)
    double contribution_threshold_pct = 0.05;
};

// -----------------------------------------------------------------------------
// FilterEngine
// -----------------------------------------------------------------------------
class FilterEngine {
public:
    // -------------------------------------------------------------------------
    // Constructor
    // records : the full parsed dataset produced by DatasetParser
    // -------------------------------------------------------------------------
    explicit FilterEngine(const std::vector<CallRecord>& records);

    // -------------------------------------------------------------------------
    // apply
    // Returns a new vector containing only the records that satisfy every
    // non-empty criterion in fc.  The original dataset is not modified.
    // -------------------------------------------------------------------------
    std::vector<CallRecord> apply(const FilterCriteria& fc) const;

    // -------------------------------------------------------------------------
    // Convenience accessors — unique sorted values present in the dataset.
    // Useful for populating filter menus in the CLI or dashboard.
    // -------------------------------------------------------------------------
    std::vector<std::string> uniqueOperators()  const;
    std::vector<std::string> uniqueCircles()    const;
    std::vector<std::string> uniquePOITypes()   const;
    std::vector<std::string> uniqueDirections() const;
    std::vector<std::string> uniqueDates()      const;  // sorted ascending

    // Total records held by this engine (before any filtering)
    std::size_t totalRecords() const { return records_.size(); }

private:
    const std::vector<CallRecord>& records_;  // non-owning reference

    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    // Return true if value is found in the list (case-sensitive).
    // Always returns true when list is empty (= no filter).
    static bool matchesAny(const std::string&              value,
                           const std::vector<std::string>& list);

    // Compare two "DD-MM-YYYY" date strings lexicographically after
    // converting to the comparable form "YYYY-MM-DD".
    // Returns  <0  if a < b
    //          0   if a == b
    //          >0  if a > b
    static int  compareDates(const std::string& a, const std::string& b);

    // Convert "DD-MM-YYYY" → "YYYY-MM-DD" for lexicographic comparison.
    static std::string toComparableDate(const std::string& ddmmyyyy);

    // Collect unique values of a string member from records_, sorted.
    std::vector<std::string> uniqueValues(
        std::string CallRecord::* member) const;
};
