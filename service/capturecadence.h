#ifndef CAPTURECADENCE_H
#define CAPTURECADENCE_H

class CaptureCadence
{
public:
    explicit CaptureCadence(double sourceFps = 25.0, double maximumFps = 12.0);
    void configure(double sourceFps, double maximumFps = 12.0);
    double outputFps() const;
    int timerIntervalMs() const;
    int framesToAdvance();

private:
    double source_fps_;
    double output_fps_;
    double frame_accumulator_;
};

#endif // CAPTURECADENCE_H
