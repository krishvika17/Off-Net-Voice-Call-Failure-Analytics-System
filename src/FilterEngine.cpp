// =============================================================================
// FilterEngine.cpp
// Save to: src/FilterEngine.cpp
// =============================================================================

#include "FilterEngine.h"

#include <algorithm>
#include <string>
#include <vector>

// =============================================================================
// Constructor
// =============================================================================
FilterEngine::FilterEngine(const std::vector<CallRecord>& records)
    : records_(records)
{}

// =============================================================================
// toComparableDate
//
// Converts "DD-MM-YYYY" to "YYYY-MM-DD" so that std::string's lexicographic
// ordering correctly represents chronological ordering.
// Returns the input unchanged if it is not exactly 10 characters.
// =============================================================================
std::string FilterEngine::toComparableDate(const std::string& ddmmyyyy) {
    // Expected format: DD-MM-YYYY  (length == 10)
    if (ddmmyyyy.size() != 10) return ddmmyyyy;
    //  01234567890
    //  DD-MM-YYYY
    const std::string dd   = ddmmyyyy.substr(0, 2);
    const std::string mm   = ddmmyyyy.substr(3, 2);
    const std::string yyyy = ddmmyyyy.substr(6, 4);
    return yyyy + "-" + mm + "-" + dd;
}

// =============================================================================
// compareDates
// =============================================================================
int FilterEngine::compareDates(const std::string& a, const std::string& b) {
    const std::string ca = toComparableDate(a);
    const std::string cb = toComparableDate(b);
    if (ca < cb) return -1;
    if (ca > cb) return  1;
    return 0;
}

// =============================================================================
// matchesAny
// =============================================================================
bool FilterEngine::matchesAny(const std::string&              value,
                              const std::vector<std::string>& list)
{
    if (list.empty()) return true;  // empty list = accept all
    return std::find(list.begin(), list.end(), value) != list.end();
}

// =============================================================================
// apply
//
// Iterates over the full dataset once and copies every record that satisfies
// all active criteria into the result vector.
// =============================================================================
std::vector<CallRecord> FilterEngine::apply(const FilterCriteria& fc) const {
    std::vector<CallRecord> result;
    result.reserve(records_.size());  // reserve worst-case to avoid reallocs

    // Pre-convert date bounds once (not inside the loop)
    const std::string fromComp = fc.date_from.empty()
                                 ? ""
                                 : toComparableDate(fc.date_from);
    const std::string toComp   = fc.date_to.empty()
                                 ? ""
                                 : toComparableDate(fc.date_to);

    // Iterate through every Off-Net call record.
    for (const auto& rec : records_) {

        // --- Destination Operator filter ---------------------------------------------------
        if (!matchesAny(rec.operator_name, fc.operators))
            continue;

        // --- Circle filter -----------------------------------------------------
        if (!matchesAny(rec.circle, fc.circles))
            continue;

        // --- POI Type filter ---------------------------------------------------
        if (!matchesAny(rec.poi_type, fc.poi_types))
            continue;

        // --- Direction filter --------------------------------------------------
        if (!matchesAny(rec.direction, fc.directions))
            continue;

        // --- Date range filter -------------------------------------------------
        if (!fromComp.empty() || !toComp.empty()) {
            const std::string recComp = toComparableDate(rec.date);

            if (!fromComp.empty() && recComp < fromComp)
                continue;

            if (!toComp.empty() && recComp > toComp)
                continue;
        }

        result.push_back(rec);
    }

    result.shrink_to_fit();  // release unused reserved capacity
    return result;
}

// =============================================================================
// uniqueValues (private helper)
//
// Collects the value of a given string member across all records,
// deduplicates, and returns them sorted ascending.
// Uses a pointer-to-member to stay generic without templates.
// =============================================================================
std::vector<std::string> FilterEngine::uniqueValues(
        std::string CallRecord::* member) const
{
    std::vector<std::string> vals;
    vals.reserve(records_.size());

    for (const auto& rec : records_)
        vals.push_back(rec.*member);

    std::sort(vals.begin(), vals.end());
    vals.erase(std::unique(vals.begin(), vals.end()), vals.end());

    vals.erase(std::remove(vals.begin(), vals.end(), ""), vals.end());

    return vals;
}

// =============================================================================
// Public unique-value accessors
// =============================================================================

std::vector<std::string> FilterEngine::uniqueOperators() const {
    return uniqueValues(&CallRecord::operator_name);
}

std::vector<std::string> FilterEngine::uniqueCircles() const {
    return uniqueValues(&CallRecord::circle);
}

std::vector<std::string> FilterEngine::uniquePOITypes() const {
    return uniqueValues(&CallRecord::poi_type);
}

std::vector<std::string> FilterEngine::uniqueDirections() const {
    return uniqueValues(&CallRecord::direction);
}

std::vector<std::string> FilterEngine::uniqueDates() const {
    // Dates need chronological sort, not pure lexicographic sort on DD-MM-YYYY.
    std::vector<std::string> dates;
    dates.reserve(records_.size());

    for (const auto& rec : records_)
        if (!rec.date.empty())
            dates.push_back(rec.date);

    // Sort by converted form (YYYY-MM-DD) to get chronological order
    std::sort(dates.begin(), dates.end(),
        [](const std::string& a, const std::string& b) {
            return FilterEngine::toComparableDate(a) <
                   FilterEngine::toComparableDate(b);
        });

    dates.erase(std::unique(dates.begin(), dates.end()), dates.end());
    return dates;
}
