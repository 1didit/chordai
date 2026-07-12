#pragma once

// Owned by plan 03-03. Skeleton created in 03-01 for build wiring only.

#include <vector>

struct OnsetEnvelopeResult
{
    std::vector<double> envelope;
    double rateHz = 250.0;
};

OnsetEnvelopeResult computeOnsetEnvelope (const std::vector<float>& samples8k);
