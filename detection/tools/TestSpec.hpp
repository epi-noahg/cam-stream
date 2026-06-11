#pragma once
// Ground-truth annotation file for a recorded 3-cam session.
//
// Produced by camdetect_annotate, consumed by camdetect_runtest.  All event
// frames are MASTER frame indices (the shared clock of the debug/annotate
// viewers); per-cam frame offsets ("décalage") are stored alongside so a
// replay reproduces exactly what the annotator saw.

#include "camdetect/Types.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <opencv2/core/persistence.hpp>
#include <string>
#include <vector>

namespace camdetect {

struct TestEvent {
    enum class Type { Dart, HumanBegin, HumanEnd, Clear };

    Type        type {Type::Dart};
    int         frame{0};
    std::string zone;       // Dart only: "T20", "D16", "20", "25", "Bull", "MISS"
    int         score{0};   // Dart only
};

inline const char* toString(TestEvent::Type t)
{
    switch (t) {
        case TestEvent::Type::Dart:       return "dart";
        case TestEvent::Type::HumanBegin: return "human_begin";
        case TestEvent::Type::HumanEnd:   return "human_end";
        case TestEvent::Type::Clear:      return "clear";
    }
    return "dart";
}

inline bool typeFromString(const std::string& s, TestEvent::Type& out)
{
    if (s == "dart")        { out = TestEvent::Type::Dart;       return true; }
    if (s == "human_begin") { out = TestEvent::Type::HumanBegin; return true; }
    if (s == "human_end")   { out = TestEvent::Type::HumanEnd;   return true; }
    if (s == "clear")       { out = TestEvent::Type::Clear;      return true; }
    return false;
}

/// Normalise a user-typed zone label to the pipeline's convention
/// ("T20", "D16", "20", "25", "Bull", "MISS").  Returns "" if invalid.
inline std::string canonicalZone(std::string z)
{
    for (auto& c : z) c = static_cast<char>(std::toupper(c));
    if (z == "MISS" || z == "OUT")  return "MISS";
    if (z == "BULL" || z == "50")   return "Bull";
    if (z == "25")                   return "25";

    int  mult  = 1;
    auto digits = z;
    if (!z.empty() && (z[0] == 'T' || z[0] == 'D')) {
        mult   = (z[0] == 'T') ? 3 : 2;
        digits = z.substr(1);
    }
    if (digits.empty() ||
        !std::all_of(digits.begin(), digits.end(),
                     [](char c) { return std::isdigit(c); }))
        return "";
    const int sector = std::stoi(digits);
    if (sector < 1 || sector > 20) return "";
    if (mult == 3) return "T" + std::to_string(sector);
    if (mult == 2) return "D" + std::to_string(sector);
    return std::to_string(sector);
}

/// Score for a canonical zone label, -1 if the label is invalid.
inline int zoneScore(const std::string& zone)
{
    const std::string z = canonicalZone(zone);
    if (z.empty())     return -1;
    if (z == "MISS")   return 0;
    if (z == "Bull")   return 50;
    if (z == "25")     return 25;
    if (z[0] == 'T')   return 3 * std::stoi(z.substr(1));
    if (z[0] == 'D')   return 2 * std::stoi(z.substr(1));
    return std::stoi(z);
}

struct TestSpec {
    int                                 fps{30};
    std::array<int, NUM_CAMS>           offsets{};   // per-cam frame décalage
    std::array<std::string, NUM_CAMS>   videos;
    std::array<std::string, NUM_CAMS>   calibs;
    std::vector<TestEvent>              events;      // insertion order in RAM,
                                                     // sorted by frame on disk

    void sortByFrame()
    {
        std::stable_sort(events.begin(), events.end(),
                         [](const TestEvent& a, const TestEvent& b) {
                             return a.frame < b.frame;
                         });
    }

    bool save(const std::string& path) const
    {
        cv::FileStorage fs(path, cv::FileStorage::WRITE);
        if (!fs.isOpened()) return false;

        TestSpec sorted = *this;
        sorted.sortByFrame();

        fs << "fps" << fps;
        fs << "offsets" << "[";
        for (int o : offsets) fs << o;
        fs << "]";
        fs << "videos" << "[";
        for (const auto& v : videos) fs << v;
        fs << "]";
        fs << "calibs" << "[";
        for (const auto& c : calibs) fs << c;
        fs << "]";
        fs << "events" << "[";
        for (const auto& e : sorted.events) {
            fs << "{" << "frame" << e.frame
               << "type" << toString(e.type);
            if (e.type == TestEvent::Type::Dart)
                fs << "zone" << e.zone << "score" << e.score;
            fs << "}";
        }
        fs << "]";
        return true;
    }

    bool load(const std::string& path)
    {
        cv::FileStorage fs(path, cv::FileStorage::READ);
        if (!fs.isOpened()) return false;

        fs["fps"] >> fps;
        if (fps <= 0) fps = 30;

        const cv::FileNode offs = fs["offsets"];
        for (int i = 0; i < NUM_CAMS && i < static_cast<int>(offs.size()); ++i)
            offs[i] >> offsets[i];

        const cv::FileNode vids = fs["videos"];
        for (int i = 0; i < NUM_CAMS && i < static_cast<int>(vids.size()); ++i)
            vids[i] >> videos[i];

        const cv::FileNode cals = fs["calibs"];
        for (int i = 0; i < NUM_CAMS && i < static_cast<int>(cals.size()); ++i)
            cals[i] >> calibs[i];

        events.clear();
        for (const auto& n : fs["events"]) {
            TestEvent   e;
            std::string type_str;
            n["frame"] >> e.frame;
            n["type"]  >> type_str;
            if (!typeFromString(type_str, e.type)) continue;
            if (e.type == TestEvent::Type::Dart) {
                n["zone"]  >> e.zone;
                n["score"] >> e.score;
            }
            events.push_back(std::move(e));
        }
        sortByFrame();
        return true;
    }
};

} // namespace camdetect
