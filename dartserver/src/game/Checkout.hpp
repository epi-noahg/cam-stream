#pragma once

// Pure checkout solver — C++ port of GoDartss/src/lib/checkout.ts.  Finds the
// fewest-darts finish for a score, and whether a score is checkout-able.

#include "GameTypes.hpp"

#include <optional>
#include <vector>

namespace dart::game {

/// Optimal (fewest-darts) checkout for @p score given darts remaining, or
/// nullopt if none exists in classic range.  outType defaults to Double.
std::optional<std::vector<Throw>> getCheckoutSuggestion(
    int score, int dartsLeft, InOutType outType = InOutType::Double);

/// Whether @p score can be finished at all under the given out rule.
bool canCheckout(int score, InOutType outType);

} // namespace dart::game
