# =============================================================================
# app.py
# Save to: dashboard/app.py
#
# Voice Call Failure Analytics System — Streamlit Dashboard
#
# Calls the compiled C++ backend (build/vcfas) via subprocess, feeds all user
# inputs through stdin, parses the structured stdout, and renders results.
# No analytics logic lives here — Python only visualises backend output.
#
# Run:
#   streamlit run dashboard/app.py
#
# Requirements (requirements.txt):
#   streamlit, plotly, pandas
# =============================================================================

import os
import re
import subprocess
import tempfile
from pathlib import Path
import platform

import pandas as pd
import plotly.express as px
import streamlit as st

# =============================================================================
# Page configuration
# =============================================================================

st.set_page_config(
    page_title="Off-Net Voice Call Failure Analytics",
    page_icon="📡",
    layout="wide",
    initial_sidebar_state="expanded",
)

# =============================================================================
# Title & description
# =============================================================================
st.title("📡 Off-Net Voice Call Failure Analytics System")

st.caption(
    "Telecom KPI Dashboard"
)

with st.expander("ℹ About this Dashboard", expanded=False):

    st.markdown("""
This dashboard analyses aggregated telecom voice-call records using
ISUP/Q.850 cause codes.

Features:
- KPI Summary
- Cause Code Analysis
- Daily Failure Trend
- Operator Comparison
- Trunk Group Analysis
- Rule-Based Recommendations
""")

st.divider()

# =============================================================================
# Backend binary location
# =============================================================================

PROJECT_ROOT = Path(__file__).resolve().parent.parent

BUILD_DIR = PROJECT_ROOT / "build"
if platform.system()=="Windows":
    BACKEND_BINARY = BUILD_DIR / "vcfas.exe"
else:
    BACKEND_BINARY = BUILD_DIR / "vcfas"

def backend_available() -> bool:
    return BACKEND_BINARY.exists() and os.access(BACKEND_BINARY, os.X_OK)

# =============================================================================
# Sidebar — Dataset
# =============================================================================
st.sidebar.header("📂 Dataset")

uploaded_file = st.sidebar.file_uploader(
    "Upload CSV Dataset",
    type=["csv"],
    help=(
    "Required columns: Date, Operator, Circle, POI_Type, POI_Name, Direction, Trunk_Group, Cause_Code, Cause_Description, Call_Count."
)
)

st.sidebar.divider()

# =============================================================================
# Sidebar — Filters
# =============================================================================

st.sidebar.header("🔍 Filters")
st.sidebar.caption("Leave blank to include all values.")

operators_input = st.sidebar.multiselect(
    "Operator",
    ["Jio", "Airtel", "Vodafone", "BSNL"],
    default=[]
)
circles_input = st.sidebar.multiselect(
    "Circle",
    ["Punjab","Haryana","Rajasthan","Uttar Pradesh"],
    default=[]
)
poi_types_input = st.sidebar.multiselect(
    "POI Type",
    ["Local","NLD","ILD"],
    default=[]
)
directions_input = st.sidebar.multiselect(
    "Direction",
    ["Incoming","Outgoing"],
    default=[]
)
date_from = st.sidebar.date_input(
    "From Date",
    value=None
)
date_to = st.sidebar.date_input(
    "To Date",
    value=None
)

st.sidebar.divider()

run_button = st.sidebar.button(
    "▶ Run Analysis",
    use_container_width=True,
)

with st.sidebar.expander("⚙ Analysis Parameters", expanded=False):

    threshold_pct = st.number_input(
        "Major Cause Threshold (% of Total Calls)",
        min_value=0.0,
        max_value=100.0,
        value=0.05,
        step=0.01,
        format="%.2f",
    )

# =============================================================================
# Helper: build the stdin string that matches main.cpp's readLine() prompts
# =============================================================================
def build_stdin(csv_path: str) -> str:
    lines = [
    csv_path,
    ",".join(operators_input),
    ",".join(circles_input),
    ",".join(poi_types_input),
    ",".join(directions_input),
    date_from.strftime("%d-%m-%Y") if date_from else "",
    date_to.strftime("%d-%m-%Y") if date_to else "",
    str(threshold_pct)
]
    return "\n".join(lines) + "\n"

# =============================================================================
# Helper: run the backend subprocess
# =============================================================================

def run_backend(csv_path: str) -> tuple:
    """Returns (success, stdout, stderr)."""

    if not backend_available():
        message = (
            f"Backend executable not found:\n{BACKEND_BINARY}\n\n"
        )
        if platform.system() == "Windows":
            message += (
                "Build using:\n"
                "mkdir build\n"
                "cd build\n"
                "cmake .. -G \"MinGW Makefiles\"\n"
                "cmake --build ."
            )
        else:
            message += (
                "Build using:\n"
                "mkdir build\n"
                "cd build\n"
                "cmake ..\n"
                "cmake --build ."
            )
        return False, "", message
    
    env = os.environ.copy()
    if platform.system() == "Windows":
        env["PATH"] = r"C:\msys64\ucrt64\bin;" + os.pathsep + env["PATH"]
        env["MSYSTEM"] = "UCRT64"
        env["CHERE_INVOKING"] = "1"

    try:
        result = subprocess.run(
        [str(BACKEND_BINARY)],
        input=build_stdin(csv_path),
        capture_output=True,
        text=True,
        timeout=300,
        env=env,
        cwd=str(PROJECT_ROOT)
    )
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", "Backend timed out (> 300 s)."
    except Exception as exc:
        return False, "", f"Subprocess error: {exc}"

# =============================================================================
# Parsers — extract structured data from backend stdout
# =============================================================================

def _find(pattern: str, text: str, cast=str, default=None):
    m = re.search(pattern, text)
    if m:
        try:
            return cast(m.group(1).strip())
        except Exception:
            return default
    return default

def parse_kpi(output: str) -> dict:
    return {
        "records_loaded":   _find(r"Records loaded\s*:\s*(\d+)",        output, int,   0),
        "records_filtered": _find(r"Records filtered\s*:\s*(\d+)",      output, int,   0),
        "total_calls":      _find(r"Total Calls\s*:\s*(\d+)",           output, int,   0),
        "failed_calls":     _find(r"Failed Calls\s*:\s*(\d+)",          output, int,   0),
        "csr":              _find(r"Call Success Rate\s*:\s*([\d.]+)",   output, float, 0.0),
        "failure_rate":     _find(r"Failure Rate\s*:\s*([\d.]+)",       output, float, 0.0),
        "major_codes":      _find(r"Major Cause Codes\s*:\s*(\d+)",     output, int,   0),
        "recommendations":  _find(r"Recommendations\s*:\s*(\d+)",       output, int,   0),
    }

def parse_cause_codes(output: str) -> pd.DataFrame:
    """
    Matches:  CC  17  |      987654 calls  |   42.31 %  |  User busy
    """
    rows = []
    for m in re.finditer(
        r"CC\s+(\d+)\s+\|\s+([\d]+)\s+calls\s+\|\s+([\d.]+)\s+%\s+\|\s+(.+)",
        output,
    ):
        rows.append({
            "Cause Code":     int(m.group(1)),
            "Failed Calls":   int(m.group(2)),
            "Contribution %": float(m.group(3)),
            "Description":    m.group(4).strip(),
        })
    return pd.DataFrame(rows)

def parse_operator_stats(output: str) -> pd.DataFrame:
    rows = []

    pattern = (
        r"Operator\s*:\s*(.*?)\n"
        r"Total Calls\s*:\s*(\d+)\n"
        r"Failed Calls\s*:\s*(\d+)\n"
        r"Failure Rate\s*:\s*([\d.]+)\s*%\n"
        r"CSR\s*:\s*([\d.]+)"
    )

    for m in re.finditer(pattern, output):
        rows.append({
            "Operator": m.group(1).strip(),
            "Total Calls": int(m.group(2)),
            "Failed Calls": int(m.group(3)),
            "Failure Rate %": float(m.group(4)),
            "CSR %": float(m.group(5)),
        })

    return pd.DataFrame(rows)

def parse_recommendations(output: str):

    rows = []
    pattern = (
        r"CC\s+(\d+)\s+\[(.*?)\]\s*"
        r".*?Contribution\s*:\s*([\d.]+)%\s*"
        r".*?Affected Operators\s*:\s*(.*?)\n"
        r".*?Description\s*:\s*(.*?)\n"
        r".*?Action\s*:\s*(.*?)\n"
        r".*?Impact\s*:\s*(.*?)(?=\nCC|\n\.\.\.|\Z)"
    )

    for m in re.finditer(pattern, output, re.DOTALL):
        rows.append({

            "Cause Code": int(m.group(1)),

            "Priority": m.group(2).strip(),

            "Contribution %": float(m.group(3)),

            "Affected Operators": m.group(4).strip(),

            "Description": m.group(5).strip(),

            "Suggested Action": m.group(6).strip(),

            "Expected Impact": m.group(7).strip()

        })
    return pd.DataFrame(rows)

def parse_trend(output: str):

    rows = []

    pattern = (
        r"Date=(.*?),"
        r"Cause=(\d+),"
        r"Contribution=([\d.]+),"
        r"FailedCalls=(\d+),"
        r"TotalCalls=(\d+),"
        r"Trunks=(.*)"
    )

    for line in output.splitlines():

        line = line.strip()

        m = re.search(pattern, line)
        if not m:
            continue

        rows.append({

            "Date": m.group(1),

            "Cause Code": "CC " + m.group(2),

            "Contribution %": float(m.group(3)),

            "Failed Calls": int(m.group(4)),

            "Total Calls": int(m.group(5)),

            "Trunk Groups": m.group(6)

        })

    return pd.DataFrame(rows)

# =============================================================================
#  Priority icon helpers
# =============================================================================

PRIORITY_ICON = {"High": "🔴", "Medium": "🟡", "Low": "🟢"}

def p_icon(p: str) -> str:
    return PRIORITY_ICON.get(p, "⚪") + " " + p


# =============================================================================
# Gate: nothing to show without a file + button press
# =============================================================================

if not uploaded_file:
    st.info("👈 Upload a CSV file in the sidebar and click ▶ Run Analysis.")
    if not backend_available():
        st.warning(
            f"⚠️ Backend binary not found at `{BACKEND_BINARY}`. "
            "Build the project with CMake before running the dashboard."
        )
    st.stop()

if not run_button:
    st.info("Configure filters in the sidebar, then click **▶ Run Analysis**.")
    st.stop()

# =============================================================================
# Save upload, run backend, clean up
# =============================================================================

with tempfile.NamedTemporaryFile(suffix=".csv", delete=False) as tmp:
    tmp.write(uploaded_file.read())
    tmp_path = tmp.name
try:
    with st.spinner("Running C++ backend…"):
        success, stdout, stderr = run_backend(tmp_path)
finally:
    os.unlink(tmp_path)

if stderr.strip():
    with st.expander("⚠️ Backend messages (row-skip warnings)", expanded=False):
        st.code(stderr, language="text")

if not success or not stdout.strip():
    st.error("❌ Backend run failed. Check messages above.")
    with st.expander("Raw backend output"):
        st.code(stdout or "(no output)", language="text")
    st.stop()

# =============================================================================
# Parse backend output
# =============================================================================
kpi        = parse_kpi(stdout)
df_causes  = parse_cause_codes(stdout)
df_ops     = parse_operator_stats(stdout)
df_trend   = parse_trend(stdout)
df_recs    = parse_recommendations(stdout)
# =============================================================================
# Section 1 — Dataset info
# =============================================================================

st.subheader("📋 Dataset")
d1, d2, d3 = st.columns(3)
d1.metric("Records Loaded",    f"{kpi['records_loaded']:,}")
d2.metric("Records Analysed",  f"{kpi['records_filtered']:,}")
d3.metric("File",              uploaded_file.name)
st.divider()

# =============================================================================
# Section 2 — KPI cards
# =============================================================================
successful_calls = kpi["total_calls"] - kpi["failed_calls"]
if kpi["csr"] >= 99.8:
    status = "🟢 Excellent"
elif kpi["csr"] >= 99.5:
    status = "🟡 Healthy"
else:
    status = "🔴 Needs Attention"
st.subheader("📊 Network KPI Dashboard")

row1 = st.columns(4)

row1[0].metric(
    "📞 Total Calls",
    f"{kpi['total_calls']:,}"
)

row1[1].metric(
    "✅ Successful Calls",
    f"{successful_calls:,}"
)

row1[2].metric(
    "❌ Failed Calls",
    f"{kpi['failed_calls']:,}"
)

row1[3].metric(
    "📈 CSR",
    f"{kpi['csr']:.2f}%"
)

row2 = st.columns(4)

row2[0].metric(
    "⚠ Failure Rate",
    f"{kpi['failure_rate']:.2f}%"
)

row2[1].metric(
    "🚨 Major Causes",
    kpi["major_codes"]
)

row2[2].metric(
    "💡 Recommendations",
    kpi["recommendations"]
)

row2[3].metric(
    "🟢 Network Status" if kpi["csr"] >= 99.5 else "🔴 Network Status",
    status.replace("🟢 ","").replace("🟡 ","").replace("🔴 ","")
)
st.divider()

# =============================================================================
# Section 3 — Cause Code Bar Chart (contribution)
# =============================================================================

st.subheader(
    f"📉 Cause Codes Contributing More Than {threshold_pct:.2f}% of Total Calls")
if df_causes.empty:
    st.info("No cause codes above the contribution threshold.")
else:
    # Show only Top 10 highest contributing cause codes
    df_causes = (
        df_causes
        .sort_values(
            by="Contribution %",
            ascending=False
        )
        .head(10)
    )
    df_causes["Label"] = (
        "CC " + df_causes["Cause Code"].astype(str)
        + " — " + df_causes["Description"].str.slice(0, 28)
    )
    fig_bar = px.bar(
        df_causes,
        x="Contribution %",
        y="Label",
        orientation="h",
        color="Contribution %",
        color_continuous_scale="Reds",
        text="Contribution %",
        hover_data={
            "Failed Calls": True,
            "Cause Code":   True,
            "Description":  True,
            "Label":        False,
        },
        labels={
        "Label": "Cause Code",
        "Contribution %": "Contribution to Total Calls (%)",
    },
    )
    
    fig_bar.update_traces(
    texttemplate="%{x:.2f} %",
    textposition="outside"
)
    fig_bar.update_layout(

    title=None,

    yaxis=dict(

        categoryorder="total ascending",

        automargin=True

    ),

    coloraxis_showscale=False,

    showlegend=False,

    height=430,

    margin=dict(

        l=15,

        r=20,

        t=20,

        b=20

    )

)
    fig_bar.update_layout(
    title=dict(text="")
)
    
    st.plotly_chart(fig_bar, width="stretch")

st.divider()

# =============================================================================
# Section 4 — Cause Code Trend Line Chart
# =============================================================================

st.subheader("📈 Daily Network Performance")

if df_trend.empty:

    st.info("No trend data available.")

else:

    df_trend["Hover Trunks"] = (
        df_trend["Trunk Groups"]
        .str.replace("|", "<br>", regex=False)
    )
    df_trend["Date"] = pd.to_datetime(
    df_trend["Date"],
    format="%d-%m-%Y"
    )

    df_trend = df_trend.sort_values("Date")

    fig_line = px.line(
        
        df_trend,
        
        x="Date",

        y="Contribution %",

        color="Cause Code",

        markers=True,

        line_shape="linear",

        labels={
            "Contribution %":
            "Daily Contribution to Total Calls (%)"
        }

    )

    for trace in fig_line.data:

        trace.name = "" if trace.name is None else str(trace.name)

        trace_df = (
            df_trend[df_trend["Cause Code"] == trace.name]
            .sort_values("Date")
            .reset_index(drop=True)
        )

        trace.customdata = trace_df[
            [
                "Failed Calls",
                "Total Calls",
                "Hover Trunks"
            ]
        ].to_numpy()

        trace.hovertemplate = (
            "<b>%{fullData.name}</b><br>"
            "<b>Date:</b> %{x|%d-%m}<br><br>"
            "<b>Major Trunk Group Connectivity</b><br><br>"
            "%{customdata[2]}"
            "<extra></extra>"
        )

        trace.line.width = 2
        trace.opacity = 0.75
    fig_line.update_layout(
    legend_title_text="Cause Code",
    title=None,

    height=500,

    hovermode="closest",

    xaxis_title="Date",

    yaxis_title="Daily Contribution to Total Calls (%)"

)
    fig_line.update_xaxes(

    type="date",

    tickmode="linear",

    dtick="D1",

    tickformat="%d-%m",

    tickangle=-45

)
    fig_line.update_layout(
    title=dict(text="")
)

    st.plotly_chart(fig_line, width="stretch")
    
st.divider()
# =============================================================================
# Section 5 — Operator Statistics
# =============================================================================

st.subheader("📡 Operator Failure Rate Comparison")
if df_ops.empty:
    st.info("No operator data available.")

else:

    df_ops = df_ops.sort_values(
        by="Failure Rate %",
        ascending=False
    )

    fig_ops = px.bar(
        df_ops,
        x="Operator",
        y="Failure Rate %",
        color="Failure Rate %",
        color_continuous_scale="Reds",
        text="Failure Rate %",
        labels={
            "Failure Rate %": "Failure Rate (%)",
            "Operator": "Operator"
        },
    )

    fig_ops.update_traces(
        texttemplate="%{y:.2f}%",
        textposition="outside",
    )

    fig_ops.update_layout(
        coloraxis_showscale=False,
        height=360,
        yaxis_title="Failure Rate (%)",
        xaxis_title="Operator",
        margin={"l":10,"r":10,"t":10,"b":10},
    )
    fig_ops.update_layout(
    title=dict(text="")
)
    st.plotly_chart(fig_ops, width="stretch")

    st.dataframe(
        df_ops.style
        .format({
            "Total Calls":"{:,}",
            "Failed Calls":"{:,}",
            "Failure Rate %":"{:.2f}",
            "CSR %":"{:.2f}",
        })
        .background_gradient(
            subset=["Failure Rate %"],
            cmap="Reds"
        ),
        use_container_width=True,
        hide_index=True
    )

st.divider()

# =============================================================================
# Section 6 — Recommendations
# =============================================================================

st.subheader("💡 NOC Recommendations")
r1, r2, r3, r4 = st.columns(4)
r1.metric("Total",          kpi["recommendations"])
if not df_recs.empty:
    r2.metric("🔴 High",   len(df_recs[df_recs["Priority"] == "High"]))
    r3.metric("🟡 Medium", len(df_recs[df_recs["Priority"] == "Medium"]))
    r4.metric("🟢 Low",    len(df_recs[df_recs["Priority"] == "Low"]))

if df_recs.empty:
    st.warning(
        "No recommendations were generated because no major cause codes exceeded the selected contribution threshold."
    )

else:

    for priority in ["High", "Medium", "Low"]:

        subset = df_recs[df_recs["Priority"] == priority]

        if subset.empty:
            continue

        with st.expander(
            f"{PRIORITY_ICON[priority]} {priority} Priority ({len(subset)})",
            expanded=False,
        ):

            for _, row in subset.iterrows():

                icon = PRIORITY_ICON.get(row["Priority"], "⚪")

                desc = str(row["Description"])[:55]

                label = (
                    f"{icon} CC {row['Cause Code']} — {desc}"
                    f" ({row['Contribution %']:.2f}%)"
                )

                st.markdown(f"### {label}")

                c1, c2, c3 = st.columns(3)

                c1.markdown(f"**Priority:** {p_icon(row['Priority'])}")

                c2.markdown(
                    f"**Affected Operators:** {row['Affected Operators']}"
                )

                c3.markdown(
                    f"**Contribution to Total Calls:** {row['Contribution %']:.2f}%"
                )

                st.markdown("**Cause Description**")

                st.write(row["Description"])

                st.markdown("**Suggested Action**")

                st.info(row["Suggested Action"])

                st.markdown("**Expected Impact**")

                st.success(row["Expected Impact"])

                st.divider()
st.divider()

st.caption(
    "Off-Net Voice Call Failure Analytics System | "
    "Reliance Jio Practice School Project | "
    "Backend: C++ | Dashboard: Streamlit"
)
# =============================================================================
# Raw output (debug)
# =============================================================================

with st.expander("🖥️ Raw Backend Output", expanded=False):
    st.code(stdout, language="text")
