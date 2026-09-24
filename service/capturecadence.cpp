// 【阅读导引】
// 采集节拍计算：根据源 FPS 和输出上限计算定时器间隔、文件每次前进的帧数。
// 此类不读视频，便于用确定的数值测试取代真实设备和等待。

#include "capturecadence.h"

#include <QtGlobal>
#include <cmath>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
CaptureCadence::CaptureCadence(double sourceFps, double maximumFps)
{
    configure(sourceFps, maximumFps);
}

// 对不可信源 FPS 采用默认值，计算实际输出速率并清空小数帧累积。
void CaptureCadence::configure(double sourceFps, double maximumFps)
{
    source_fps_ = (sourceFps >= 1.0 && sourceFps <= 240.0) ? sourceFps : 25.0;
    output_fps_ = qMax(1.0, qMin(source_fps_, maximumFps));
    frame_accumulator_ = 0.0;
}

// 返回限制后的输出 FPS，不代表设备一定能达到的实测吞吐。
double CaptureCadence::outputFps() const
{
    return output_fps_;
}

// 把输出 FPS 转为定时器毫秒间隔，并确保至少为 1 ms。
int CaptureCadence::timerIntervalMs() const
{
    return qMax(1, qRound(1000.0 / output_fps_));
}

// 累积源/输出速率比，取整数步长并保留小数余量，避免长期播放速度漂移。
int CaptureCadence::framesToAdvance()
{
    frame_accumulator_ += source_fps_ / output_fps_;
    const int frames = qMax(1, static_cast<int>(std::floor(frame_accumulator_)));
    frame_accumulator_ -= frames;
    return frames;
}
