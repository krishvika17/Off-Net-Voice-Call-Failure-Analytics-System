// =============================================================================
// AnalyticsEngine.cpp
// Save to: src/AnalyticsEngine.cpp
// =============================================================================

#include "AnalyticsEngine.h"
#include <algorithm>
#include <unordered_map>
#include <string>
#include <unordered_set>

// =============================================================================
// Constructor
// =============================================================================
AnalyticsEngine::AnalyticsEngine(const std::vector<CallRecord> &records,
                                 double threshold_pct)
    : records_(records), threshold_pct_(threshold_pct)
{
}

// =============================================================================
// isFailureCode
// Cause codes 16–31 inclusive are normal outcomes.
// Everything outside that range (1–15, 32–127) is a failure.
// =============================================================================
bool AnalyticsEngine::isFailureCode(int code)
{
    return code < 16 || code > 31;
}

// =============================================================================
// compute
// Entry point — orchestrates all sub-calculations and returns AnalyticsResult.
// =============================================================================
AnalyticsResult AnalyticsEngine::compute() const
{
    AnalyticsResult result;
    result.contribution_threshold_pct = threshold_pct_;

    computeAnalytics(result.kpi,
                     result.cause_code_stats,
                     result.operator_stats,
                     result.trend);

    finaliseResults(result);
    return result;
}

// =============================================================================
// computeAnalytics
//
// Single pass over records_ that simultaneously builds:
//   - KPI totals (total_calls, failed_calls)
//   - per-cause-code aggregates  (keyed by cause_code)
//   - per-operator aggregates    (keyed by operator_name)
//   - per-date aggregates        (keyed by date string)
//
// unordered_map is used for keyed accumulation.
// Results are converted to vectors and sorted afterwards.
// =============================================================================
void AnalyticsEngine::computeAnalytics(
    KPISummary &kpi,
    std::vector<CauseCodeStat> &causeStats,
    std::vector<OperatorStat> &opStats,
    std::vector<TrendPoint> &trend) const
{
    // Keyed accumulators — built during the single pass
    // cause_code → CauseCodeStat (partial: no pct yet)
    std::unordered_map<int, CauseCodeStat> causeMap;
    std::unordered_map<
    int,
    std::unordered_map<std::string,long long>
> causeOperatorMap;

    // operator_name → OperatorStat (partial: no pct yet)
    std::unordered_map<std::string, OperatorStat> opMap;

    // date → TrendPoint (partial)

    struct TrunkAccumulator
    {
        long long failed_calls = 0;
    };

    struct CauseAccumulator
    {
        long long failed_calls = 0;

        std::unordered_map<
            std::string,
            TrunkAccumulator
        > trunks;
    };

    struct DailyAccumulator
    {
        long long totalCalls = 0;

        long long failedCalls = 0;

        // Total attempts on every connectivity
        std::unordered_map<
            std::string,
            long long
        > trunkAttempts;

        std::unordered_map<
            int,
            CauseAccumulator
        > causes;
    };
    std::unordered_map<std::string, DailyAccumulator> trendMap;

    // --- Single pass -----------------------------------------------------------
    for (const auto &rec : records_)
    {
        const bool failure = isFailureCode(rec.cause_code);

        // KPI totals
        kpi.total_calls += rec.call_count;
        if (failure)
            kpi.failed_calls += rec.call_count;

        // Per-cause accumulation (failure codes only)
       if (failure)
        {
            auto &cs = causeMap[rec.cause_code];

            if (cs.cause_code == 0)
            {
                cs.cause_code = rec.cause_code;
                cs.cause_description = rec.cause_description;
            }

            cs.failed_call_count += rec.call_count;

            // NEW
            causeOperatorMap
                [rec.cause_code]
                [rec.operator_name] += rec.call_count;
        }
        // Per-operator accumulation (all codes)

        {
            auto &op = opMap[rec.operator_name];
            if (op.operator_name.empty())
                op.operator_name = rec.operator_name;
            op.total_calls += rec.call_count;
            if (failure)
                op.failed_calls += rec.call_count;
        }

        // Per-date accumulation (all codes for total; failure only for failed)
        {
            auto &day = trendMap[rec.date];

            day.totalCalls += rec.call_count;

            // Every call contributes to total attempts on that trunk
            day.trunkAttempts[rec.trunk_group] += rec.call_count;

            if (failure)
            {
                day.failedCalls += rec.call_count;

                auto &cause = day.causes[rec.cause_code];

                cause.failed_calls += rec.call_count;

                cause.trunks[rec.trunk_group].failed_calls += rec.call_count;
            }
        }
    }

    // --- Derived KPI fields ----------------------------------------------------
    kpi.normal_calls = kpi.total_calls - kpi.failed_calls;
    kpi.total_cause_codes = static_cast<int>(causeMap.size());

    if (kpi.total_calls > 0)
    {
        kpi.failure_rate_pct = (static_cast<double>(kpi.failed_calls) /
                                static_cast<double>(kpi.total_calls)) *
                               100.0;
        kpi.csr_pct = 100.0 - kpi.failure_rate_pct;
    }

    // --- Flatten maps into vectors for output ----------------------------------
    causeStats.clear();
    causeStats.reserve(causeMap.size());

    for (auto &[code, cs] : causeMap)//stores the top 3 contributors to that cause code
{
    auto &ops = causeOperatorMap[code];

    std::vector<
        std::pair<std::string,long long>
    > sortedOps(
        ops.begin(),
        ops.end()
    );

    std::sort(
        sortedOps.begin(),
        sortedOps.end(),
        [](auto &a, auto &b)
        {
            return a.second > b.second;
        }
    );

    for (size_t i=0;
         i<sortedOps.size() && i<3;
         i++)
    {
        cs.affected_operators.push_back(
            sortedOps[i].first
        );
    }

    causeStats.push_back(std::move(cs));
}
    opStats.clear();
    opStats.reserve(opMap.size());
    for (auto &[name, op] : opMap)
        opStats.push_back(std::move(op));
    std::unordered_set<int> majorCauseCodes;

    for (const auto& cs : causeStats)
    {
        double contribution =
            (100.0 * cs.failed_call_count) /
            kpi.total_calls;

        if (contribution >= threshold_pct_)
        {
            majorCauseCodes.insert(cs.cause_code);
        }
    }
    trend.clear();
    trend.reserve(trendMap.size());

    for (auto &[date, day] : trendMap)
    {
        TrendPoint tp;

        for (auto &[code, cause] : day.causes)
        {
            DailyCauseTrend item;

            item.date = date;

            item.cause_code = code;

            item.failed_calls = cause.failed_calls;

            item.total_calls = day.totalCalls;

            if (day.totalCalls > 0)
            {
                item.contribution_pct =
                    (100.0 * cause.failed_calls) /
                    day.totalCalls;
            }
            else
            {
                item.contribution_pct = 0.0;
            }
           if (majorCauseCodes.find(code) == majorCauseCodes.end())
                continue;
            auto it = causeMap.find(code);

            if (it != causeMap.end())
                item.cause_description =
                    it->second.cause_description;

            for (auto &[tg, stats] : cause.trunks)
            {
                TrunkGroupStat trunk;

                trunk.trunk_group = tg;

                trunk.failed_calls = stats.failed_calls;

                trunk.total_calls = day.trunkAttempts[tg];

                if (trunk.total_calls > 0)
                {
                    trunk.contribution_pct =
                        (100.0 * trunk.failed_calls) /
                        trunk.total_calls;

                    trunk.failure_rate_pct =
                        trunk.contribution_pct;
                }
                else
                {
                    trunk.contribution_pct = 0.0;
                    trunk.failure_rate_pct = 0.0;
                }

                item.trunk_groups.push_back(trunk);
            }

            std::sort(
                item.trunk_groups.begin(),
                item.trunk_groups.end(),
                [](const TrunkGroupStat &a,
                   const TrunkGroupStat &b)
                {
                    return a.failed_calls >
                           b.failed_calls;
                });
            if (item.trunk_groups.size() > 3)
            {
                item.trunk_groups.resize(3);
            }

            tp.daily_causes.push_back(item);
        }
        std::sort(
            tp.daily_causes.begin(),
            tp.daily_causes.end(),
            [](const DailyCauseTrend &a,
               const DailyCauseTrend &b)
            {
                return a.contribution_pct >
                       b.contribution_pct;
            });
        trend.push_back(tp);
    }
}

    // =============================================================================
    // finaliseResults
    //
    // After the accumulation pass:
    //   1. Fill percentages in CauseCodeStat and OperatorStat.
    //   2. Count major cause codes (>= threshold).
    //   3. Sort cause_code_stats descending by failed_call_count.
    //   4. Remove cause codes below contribution_threshold_pct.
    //   5. Sort trend chronologically (dates stored as "DD-MM-YYYY" need
    //      conversion to "YYYY-MM-DD" for correct lexicographic ordering).
    //   6. Sort operator_stats descending by failed_calls.
    // =============================================================================
    void AnalyticsEngine::finaliseResults(AnalyticsResult & result) const
    {

        const double totalFailed =
            static_cast<double>(result.kpi.failed_calls);

        // -------------------------------------------------------------------------
        // 1. Cause-code percentages + major code count
        // -------------------------------------------------------------------------
        int majorCount = 0;

        const double totalCalls =
        static_cast<double>(result.kpi.total_calls);

    for (auto &cs : result.cause_code_stats)
    {
        if (totalCalls > 0.0)
        {
            cs.contribution_pct =
                (100.0 * cs.failed_call_count) /
                totalCalls;
        }
        else
        {
            cs.contribution_pct = 0.0;
        }

        if (cs.contribution_pct >= threshold_pct_)
            ++majorCount;
    }

        result.kpi.major_cause_codes = majorCount;

        // -------------------------------------------------------------------------
        // 2. Sort cause codes descending by failed_call_count
        // -------------------------------------------------------------------------
        std::sort(result.cause_code_stats.begin(),
                  result.cause_code_stats.end(),
                  [](const CauseCodeStat &a, const CauseCodeStat &b)
                  {
                      return a.failed_call_count > b.failed_call_count;
                  });
        // -------------------------------------------------------------------------
        // 3. Remove cause codes below the contribution threshold
        // -------------------------------------------------------------------------
        auto belowThreshold = [&](const CauseCodeStat &cs)
        {
            return cs.contribution_pct < threshold_pct_;
        };
        result.cause_code_stats.erase(
            std::remove_if(result.cause_code_stats.begin(),
                           result.cause_code_stats.end(),
                           belowThreshold),
            result.cause_code_stats.end());

        // -------------------------------------------------------------------------
        // 4. Operator percentages, sorted descending by failed_calls
        // -------------------------------------------------------------------------

        for (auto &op : result.operator_stats)
        {
            if (op.total_calls > 0)
            {
                op.failure_rate_pct =
                    (static_cast<double>(op.failed_calls) /
                     static_cast<double>(op.total_calls)) *
                    100.0;
                op.csr_pct = 100.0 - op.failure_rate_pct;
            }
        }

        std::sort(result.operator_stats.begin(), result.operator_stats.end(),
                  [](const OperatorStat &a, const OperatorStat &b)
                  {
                      if (a.failed_calls != b.failed_calls)
                          return a.failed_calls > b.failed_calls;
                      return a.operator_name < b.operator_name;
                  });

        // -------------------------------------------------------------------------
        // 5. Sort trend chronologically
        //    Dates are "DD-MM-YYYY" — convert to "YYYY-MM-DD" for correct sort
        // -------------------------------------------------------------------------
        auto toISO = [](const std::string &ddmmyyyy) -> std::string
        {
            if (ddmmyyyy.size() != 10)
                return ddmmyyyy;
            return ddmmyyyy.substr(6, 4) + "-" + ddmmyyyy.substr(3, 2) + "-" + ddmmyyyy.substr(0, 2);
        };

        std::sort(
            result.trend.begin(),
            result.trend.end(),
            [&toISO](const TrendPoint &a,
                     const TrendPoint &b)
            {
                if (a.daily_causes.empty())
                    return false;

                if (b.daily_causes.empty())
                    return true;

                return toISO(a.daily_causes.front().date) < toISO(b.daily_causes.front().date);
            });
    }
