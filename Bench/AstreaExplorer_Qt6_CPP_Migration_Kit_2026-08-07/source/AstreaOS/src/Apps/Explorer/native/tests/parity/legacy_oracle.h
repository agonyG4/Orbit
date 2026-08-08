#pragma once

#include "parity/parity_snapshot.h"

namespace Astrea::Explorer::Native::Parity {

class FixtureTree;

class LegacyOracle final
{
public:
    ParitySnapshot capture(const FixtureTree &fixture) const;
};

} // namespace Astrea::Explorer::Native::Parity
