#pragma once

#include <string>

// =============================================================================
// CallRecord.h
// Save to: include/CallRecord.h
//
//Represents a single row from an Off-Net (Inter-Operator) aggregated telecom call dataset.

// Required columns:
// Date
// Operator
// Circle
// POI_Type
// POI_Name
// Direction
// Trunk_Group
// Cause_Code
// Cause_Description
// Call_Count

// Project convention:
//   All headers use the .h extension.
//   Cause codes 16–31 (inclusive) are normal/positive outcomes.
//   Only cause codes OUTSIDE this range (i.e. 1–15 and 32–128) are treated
//   as failures and contribute to KPIs, anomaly detection, and recommendations.
// =============================================================================

struct CallRecord {

    // -------------------------------------------------------------------------
    // Data members — one per Off-Net dataset column
    // -------------------------------------------------------------------------

    std::string date;               // e.g. "16-06-2026"
    std::string operator_name;      //// Destination operator (e.g. Airtel, BSNL, Vi)
    std::string circle;             //location 
    std::string poi_type;           // e.g. "Local", "NLD", "ILD"
    std::string poi_name;           // Interconnection point identifier
    std::string direction;          // "Incoming" or "Outgoing"
    // Used to identify the inter-operator connectivity responsible for failures.
    std::string trunk_group;        // Off-Net inter-operator trunk group
    int         cause_code   = 0;   // ISUP Q.850 cause value (1–128)
    std::string cause_description;  // e.g. "User busy"
    long long   call_count   = 0LL; // aggregate call count for this row

    // -------------------------------------------------------------------------
    // Helper: is this cause code a failure?
    //
    // Cause codes 16–31 (inclusive) are defined as normal/positive outcomes
    // per the project convention (aligned with ITU-T Q.850 "Normal" class):
    //
    //   16 – Normal call clearing       22 – Number changed
    //   17 – User busy                  23 – Redirection to new destination
    //   18 – No user responding         24–30 – Spare/Reserved (normal class)
    //   19 – No answer from user        31 – Normal, unspecified
    //   20 – Subscriber absent
    //   21 – Call rejected
    //
    // All codes in 1–15 and 32–128 are failures.
    // -------------------------------------------------------------------------
    bool isFailure() const {
        return cause_code < 16 || cause_code > 31;
    }

    // -------------------------------------------------------------------------
    // Helper: map cause_code to its ITU-T Q.850 class name.
    //
    // Based on Dialogic CSP1010 appendix:
    //   https://www.dialogic.com/webhelp/csp1010/8.4.1_ipn3/
    //          ccs_appendix_appe_-_cause_codes.htm
    //
    //   1–15   Normal class (early/pre-connection failures)
    //   16–31  Normal class (call clearing — treated as positive outcomes)
    //   32–47  Resource Unavailable
    //   48–63  Service or Option Not Available
    //   64–79  Service or Option Not Implemented
    //   80–95  Invalid Message
    //   96–111 Protocol Error
    //   112–127 Interworking
    //   128     Spare / Vendor-Specific
    // -------------------------------------------------------------------------
    std::string causeCategory() const {
        if (cause_code >= 1   && cause_code <= 31)  return "Normal";
        if (cause_code >= 32  && cause_code <= 47)  return "Resource Unavailable";
        if (cause_code >= 48  && cause_code <= 63)  return "Service/Option Not Available";
        if (cause_code >= 64  && cause_code <= 79)  return "Service/Option Not Implemented";
        if (cause_code >= 80  && cause_code <= 95)  return "Invalid Message";
        if (cause_code >= 96  && cause_code <= 111) return "Protocol Error";
        if (cause_code >= 112 && cause_code <= 127) return "Interworking";
        return "Spare/Vendor-Specific";
    }
};
