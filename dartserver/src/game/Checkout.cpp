#include "Checkout.hpp"

#include <algorithm>

namespace dart::game {
namespace {

/// All scoring throws (singles/doubles/triples 1-20 + both bulls), ordered by
/// descending point value so backtracking finds high-value finishes first.
const std::vector<Throw>& allThrows() {
    static const std::vector<Throw> kThrows = [] {
        std::vector<Throw> v;
        for (int val = 1; val <= 20; ++val) {
            v.push_back({val, 1, false});
            v.push_back({val, 2, false});
            v.push_back({val, 3, false});
        }
        v.push_back({25, 1, false});  // outer bull
        v.push_back({50, 1, false});  // inner bull
        std::sort(v.begin(), v.end(), [](const Throw& a, const Throw& b) {
            return a.hitValue() > b.hitValue();
        });
        return v;
    }();
    return kThrows;
}

bool isValidFinish(const Throw& thr, InOutType outType) {
    switch (outType) {
        case InOutType::Any:    return true;
        case InOutType::Double: return thr.multiplier == 2 || thr.value == 50;
        case InOutType::Triple: return thr.multiplier == 3;
    }
    return false;
}

/// Backtrack for a finish using exactly @p exactDarts darts.
std::optional<std::vector<Throw>> backtrack(int score, std::vector<Throw> path,
                                            int exactDarts, InOutType outType) {
    if (static_cast<int>(path.size()) == exactDarts) {
        if (score == 0 && !path.empty() && isValidFinish(path.back(), outType))
            return path;
        return std::nullopt;
    }
    if (score <= 0) return std::nullopt;

    for (const Throw& thr : allThrows()) {
        const int hv = thr.hitValue();
        if (hv > score) continue;

        if (static_cast<int>(path.size()) == exactDarts - 1) {
            if (hv == score && isValidFinish(thr, outType)) {
                path.push_back(thr);
                return path;
            }
            continue;
        }
        std::vector<Throw> next = path;
        next.push_back(thr);
        if (auto res = backtrack(score - hv, std::move(next), exactDarts, outType))
            return res;
    }
    return std::nullopt;
}

std::optional<std::vector<Throw>> findOptimalCheckout(int score, int maxDarts,
                                                      InOutType outType) {
    for (int n = 1; n <= maxDarts; ++n)
        if (auto sol = backtrack(score, {}, n, outType))
            return sol;
    return std::nullopt;
}

} // namespace

std::optional<std::vector<Throw>> getCheckoutSuggestion(int score, int dartsLeft,
                                                        InOutType outType) {
    if (score < 2 || score > 170) return std::nullopt;
    return findOptimalCheckout(score, std::min(dartsLeft, 3), outType);
}

bool canCheckout(int score, InOutType outType) {
    if (score == 1 && outType != InOutType::Any) return false;
    if (outType == InOutType::Any) return score > 0;
    return findOptimalCheckout(score, 3, outType).has_value();
}

} // namespace dart::game
