// 【阅读导引】
// 采集节拍计算：根据源 FPS 和输出上限计算定时器间隔、文件每次前进的帧数。
// 此类不读视频，便于用确定的数值测试取代真实设备和等待。

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
