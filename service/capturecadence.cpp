#include "capturecadence.h"

#include <QtGlobal>
#include <cmath>

CaptureCadence::CaptureCadence(double sourceFps, double maximumFps)
{
    configure(sourceFps, maximumFps);
}

void CaptureCadence::configure(double sourceFps, double maximumFps)
{
    source_fps_ = (sourceFps >= 1.0 && sourceFps <= 240.0) ? sourceFps : 25.0;
    output_fps_ = qMax(1.0, qMin(source_fps_, maximumFps));
    frame_accumulator_ = 0.0;
}

double CaptureCadence::outputFps() const
{
    return output_fps_;
}

int CaptureCadence::timerIntervalMs() const
{
    return qMax(1, qRound(1000.0 / output_fps_));
}

int CaptureCadence::framesToAdvance()
{
    frame_accumulator_ += source_fps_ / output_fps_;
    const int frames = qMax(1, static_cast<int>(std::floor(frame_accumulator_)));
    frame_accumulator_ -= frames;
    return frames;
}
