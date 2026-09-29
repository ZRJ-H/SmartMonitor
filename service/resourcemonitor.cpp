#include "resourcemonitor.h"
#include <QElapsedTimer>
#include <QMap>
#include <QRegularExpression>
#include <cmath>
#include <vector>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#endif

namespace ResourceMetrics {
double cpuPercent(quint64 ticks,qint64 elapsedNs,unsigned processors)
{
    if(elapsedNs<=0 || processors==0)return -1;
    // Process times are in 100ns units; normalize across all logical CPUs.
    return qBound(0.0,double(ticks)*10000.0/double(elapsedNs)/processors,100.0);
}
double gpuPercent(const QList<QPair<QString,double>> &engines,quint64 pid)
{
    static const QRegularExpression name("^pid_(\\d+)_(luid_.*_phys_\\d+_eng_\\d+)_engtype_");
    QMap<QString,double> totals;
    for(const auto &engine:engines){
        const auto match=name.match(engine.first);
        if(!match.hasMatch() || match.captured(1).toULongLong()!=pid)continue;
        if(!std::isfinite(engine.second) || engine.second<0)return -1;
        totals[match.captured(2)]+=engine.second;
    }
    double busiest=0;
    for(double value:totals)busiest=qMax(busiest,value);
    // Engines operate concurrently; summing unrelated engines overcounts usage.
    return qBound(0.0,busiest,100.0);
}
QString percentText(double value)
{
    return !std::isfinite(value)||value<0 ? QStringLiteral("—")
        : QString::number(qBound(0.0,value,100.0),'f',1)+"%";
}
}

#ifdef Q_OS_WIN
namespace {
quint64 ticks(const FILETIME &time){return (quint64(time.dwHighDateTime)<<32)|time.dwLowDateTime;}
class ProcessSampler {
public:
    ProcessSampler(){
        if(PdhOpenQueryW(nullptr,0,&query_)==ERROR_SUCCESS){
            if(PdhAddEnglishCounterW(query_,L"\\GPU Engine(*)\\Utilization Percentage",0,&gpu_)!=ERROR_SUCCESS){
                PdhCloseQuery(query_);query_=nullptr;gpu_=nullptr;
            }
        }
        clock_.start();
    }
    ~ProcessSampler(){if(query_)PdhCloseQuery(query_);}
    void sample(double &cpu,double &gpu,double &memory){
        cpu=gpu=memory=-1;
        FILETIME created{},exited{},kernel{},user{};
        if(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)){
            const quint64 current=ticks(kernel)+ticks(user);
            const qint64 now=clock_.nsecsElapsed();
            if(previousNs_>=0 && current>=previousTicks_)
                cpu=ResourceMetrics::cpuPercent(current-previousTicks_,now-previousNs_,GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
            previousTicks_=current;previousNs_=now;
        }else previousNs_=-1;
        PROCESS_MEMORY_COUNTERS info{};info.cb=sizeof(info);
        MEMORYSTATUSEX physical{};physical.dwLength=sizeof(physical);
        if(GetProcessMemoryInfo(GetCurrentProcess(),&info,sizeof(info)) && GlobalMemoryStatusEx(&physical) && physical.ullTotalPhys)
            memory=qBound(0.0,100.0*double(info.WorkingSetSize)/double(physical.ullTotalPhys),100.0);
        if(!query_)return;
        if(PdhCollectQueryData(query_)!=ERROR_SUCCESS){gpuPrimed_=false;return;}
        if(!gpuPrimed_){gpuPrimed_=true;return;}
        for(int attempt=0;attempt<3;++attempt){
            DWORD bytes=0,count=0;
            const auto status=PdhGetFormattedCounterArrayW(gpu_,PDH_FMT_DOUBLE,&bytes,&count,nullptr);
            if(status!=PDH_MORE_DATA || bytes==0)return;
            // Use aligned storage; instance lists can grow between the size and data calls.
            std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> buffer((bytes+sizeof(PDH_FMT_COUNTERVALUE_ITEM_W)-1)/sizeof(PDH_FMT_COUNTERVALUE_ITEM_W));
            const auto result=PdhGetFormattedCounterArrayW(gpu_,PDH_FMT_DOUBLE,&bytes,&count,buffer.data());
            if(result==PDH_MORE_DATA)continue;
            if(result!=ERROR_SUCCESS)return;
            QList<QPair<QString,double>> engines;
            bool valid=false;
            for(DWORD i=0;i<count;++i){
                const auto &entry=buffer[i];
                const bool ok=entry.FmtValue.CStatus==PDH_CSTATUS_VALID_DATA || entry.FmtValue.CStatus==PDH_CSTATUS_NEW_DATA;
                valid|=ok;
                engines.append({QString::fromWCharArray(entry.szName),ok?entry.FmtValue.doubleValue:-1.0});
            }
            if(valid)gpu=ResourceMetrics::gpuPercent(engines,GetCurrentProcessId());
            return;
        }
    }
private:
    PDH_HQUERY query_=nullptr;
    PDH_HCOUNTER gpu_=nullptr;
    bool gpuPrimed_=false;
    QElapsedTimer clock_;
    quint64 previousTicks_=0;
    qint64 previousNs_=-1;
};
}
#endif
ResourceMonitor::~ResourceMonitor(){stop();}
void ResourceMonitor::stop(){
    requestInterruption();
    {QMutexLocker lock(&waitMutex_);wake_.wakeAll();}
    wait();
}
void ResourceMonitor::run(){
#ifdef Q_OS_WIN
    ProcessSampler sampler;
#endif
    while(!isInterruptionRequested()){
        double cpu=-1,gpu=-1,memory=-1;
#ifdef Q_OS_WIN
        sampler.sample(cpu,gpu,memory);
#endif
        if(isInterruptionRequested())break;
        emit usageReady(cpu,gpu,memory);
        QMutexLocker lock(&waitMutex_);
        if(!isInterruptionRequested())wake_.wait(&waitMutex_,1000);
    }
}