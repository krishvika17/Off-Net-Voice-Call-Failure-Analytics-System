Voice-Call-Failure-Analytics-System/

│

├──CMakeLists.txt # CMake build configuration

├── README.md # Project overview, build & run guide

├── requirements.txt # Python deps: streamlit, plotly, pandas, openpyxl

├── .gitignore

│

├── include/ # All C++ headers

│ ├── CallRecord.h

│ ├── DatasetParser.h

│ ├── FilterEngine.h

│ ├── AnalyticsEngine.h

│ └── RecommendationEngine.h

│

├── src/ # All C++ implementation files

│ ├── main.cpp

│ ├── DatasetParser.cpp

│ ├── FilterEngine.cpp

│ ├── AnalyticsEngine.cpp

│ └── RecommendationEngine.cpp

│

├── dashboard/

│ └── app.py # Streamlit+Plotly interactive dashboard

│

├── data/

│ └── data/ (Optional sample datasets)

│

├── output/ # Future analytics exports

├── docs/

│ ├── architecture.md

│ ├── screenshots

│

└── tests/ # Reserved for future unit tests