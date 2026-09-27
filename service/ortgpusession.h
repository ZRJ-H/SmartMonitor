#ifndef SMART_ORT_SESSION_H
#define SMART_ORT_SESSION_H
// ORT C ABI keeps the existing MinGW application independent of MSVC C++ ABI.
#include <windows.h>
#include <dxgi1_2.h>
#if defined(__MINGW32__) && !defined(_stdcall)
#define _stdcall __stdcall
#endif
#undef _Check_return_
#undef _In_reads_
#undef _Inout_updates_
#undef _Out_writes_
#undef _Inout_updates_all_
#undef _Out_writes_bytes_all_
#undef _Out_writes_all_
#undef _Success_
#undef _Outptr_result_buffer_maybenull_
#include "../third_party/onnxruntime/onnxruntime_c_api.h"
#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>

class OrtGpuSession {
public:
    OrtGpuSession(const std::wstring &directory, const std::wstring &model,
                  const std::wstring &profile = {}) {
        try {
            module = LoadLibraryExW((directory + L"/onnxruntime.dll").c_str(), nullptr,
                                    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (!module) throw std::runtime_error("Cannot load onnxruntime.dll or DirectML dependency: " + std::to_string(GetLastError()));
            auto getBase = symbol<const OrtApiBase* (ORT_API_CALL*)()>(module,"OrtGetApiBase");
            if (!getBase || !(api = getBase()->GetApi(ORT_API_VERSION))) throw std::runtime_error("ORT API 22 unavailable");
            check(api->CreateEnv(ORT_LOGGING_LEVEL_WARNING,"SmartMonitor",&env));
            check(api->DisableTelemetryEvents(env));
            OrtSessionOptions *raw = nullptr;
            check(api->CreateSessionOptions(&raw));
            auto options = std::unique_ptr<OrtSessionOptions, void(*)(OrtSessionOptions*)>(raw, api->ReleaseSessionOptions);
            check(api->DisableMemPattern(raw));
            check(api->SetSessionExecutionMode(raw,ORT_SEQUENTIAL));
            check(api->SetIntraOpNumThreads(raw,2));
            check(api->SetSessionGraphOptimizationLevel(raw,ORT_ENABLE_ALL));
            auto append = symbol<OrtStatus* (ORT_API_CALL*)(OrtSessionOptions*,int)>(module,"OrtSessionOptionsAppendExecutionProvider_DML");
            if (!append) throw std::runtime_error("DirectML provider unavailable");
            int adapter = chooseAdapter();
            check(append(raw,adapter));
            if (!profile.empty()) check(api->EnableProfiling(raw,profile.c_str()));
            check(api->CreateSession(env,model.c_str(),raw,&session));
            OrtAllocator *alloc=nullptr; check(api->GetAllocatorWithDefaultOptions(&alloc));
            char *name=nullptr;
            check(api->SessionGetInputName(session,0,alloc,&name)); inputName=name; alloc->Free(alloc,name);
            check(api->SessionGetOutputName(session,0,alloc,&name)); outputName=name; alloc->Free(alloc,name);
            check(api->CreateCpuMemoryInfo(OrtArenaAllocator,OrtMemTypeDefault,&memory));
        } catch (...) { release(); throw; }
    }
    ~OrtGpuSession(){release();}
    OrtGpuSession(const OrtGpuSession&)=delete;
    OrtGpuSession& operator=(const OrtGpuSession&)=delete;
    const std::wstring &device() const { return deviceName; }
    cv::Mat run(const cv::Mat &blob) {
        if (blob.type()!=CV_32F || !blob.isContinuous() || blob.dims!=4)
            throw std::runtime_error("Expected contiguous FP32 NCHW blob");
        int64_t dims[4]={blob.size[0],blob.size[1],blob.size[2],blob.size[3]};
        OrtValue *input=nullptr,*output=nullptr;
        check(api->CreateTensorWithDataAsOrtValue(memory,const_cast<float*>(blob.ptr<float>()),blob.total()*sizeof(float),
                  dims,4,ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,&input));
        auto inputGuard=std::unique_ptr<OrtValue,void(*)(OrtValue*)>(input,api->ReleaseValue);
        const char *in=inputName.c_str(), *out=outputName.c_str();
        const OrtValue *inputs[]={input};
        check(api->Run(session,nullptr,&in,inputs,1,&out,1,&output));
        auto outputGuard=std::unique_ptr<OrtValue,void(*)(OrtValue*)>(output,api->ReleaseValue);
        OrtTensorTypeAndShapeInfo *shape=nullptr;check(api->GetTensorTypeAndShape(output,&shape));
        auto shapeGuard=std::unique_ptr<OrtTensorTypeAndShapeInfo,void(*)(OrtTensorTypeAndShapeInfo*)>(shape,api->ReleaseTensorTypeAndShapeInfo);
        size_t count=0;check(api->GetDimensionsCount(shape,&count));
        std::vector<int64_t> sizes(count);check(api->GetDimensions(shape,sizes.data(),count));
        ONNXTensorElementDataType type;check(api->GetTensorElementType(shape,&type));
        if(type!=ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || count!=3 || sizes[0]!=1)
            throw std::runtime_error("Unsupported YOLO output tensor");
        std::vector<int> cvSizes;for(auto size:sizes){if(size<=0||size>1000000)throw std::runtime_error("Invalid shape");cvSizes.push_back(int(size));}
        void *data=nullptr;check(api->GetTensorMutableData(output,&data));
        return cv::Mat(int(count),cvSizes.data(),CV_32F,data).clone();
    }
    std::string finishProfile(){
        OrtAllocator *alloc=nullptr;check(api->GetAllocatorWithDefaultOptions(&alloc));
        char *path=nullptr;check(api->SessionEndProfiling(session,alloc,&path));
        std::string result=path;alloc->Free(alloc,path);return result;
    }
private:
    template<typename T> static T symbol(HMODULE library,const char *name){
        auto address=GetProcAddress(library,name);T result=nullptr;
        static_assert(sizeof(result)==sizeof(address),"Windows function pointer size");
        std::memcpy(&result,&address,sizeof(result));return result;
    }
    HMODULE module=nullptr;
    const OrtApi *api=nullptr;OrtEnv *env=nullptr;OrtSession *session=nullptr;OrtMemoryInfo *memory=nullptr;
    std::string inputName,outputName;std::wstring deviceName;
    void check(OrtStatus *status){if(status){std::string message=api->GetErrorMessage(status);api->ReleaseStatus(status);throw std::runtime_error(message);}}
    void release(){
        if(api){if(memory)api->ReleaseMemoryInfo(memory);if(session)api->ReleaseSession(session);if(env)api->ReleaseEnv(env);}
        memory=nullptr;session=nullptr;env=nullptr;api=nullptr;
        if(module)FreeLibrary(module);
        module=nullptr;
    }
    int chooseAdapter(){
        HMODULE dxgi=LoadLibraryExW(L"dxgi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!dxgi)throw std::runtime_error("DXGI unavailable");
        auto create=symbol<HRESULT(WINAPI*)(REFIID,void**)>(dxgi,"CreateDXGIFactory1");
        const GUID iid={0x770aae78,0xf26f,0x4dba,{0xa8,0x29,0x25,0x3c,0x83,0xd1,0xb3,0x87}};
        IDXGIFactory1 *factory=nullptr;
        if(!create||FAILED(create(iid,reinterpret_cast<void**>(&factory)))){FreeLibrary(dxgi);throw std::runtime_error("DXGI factory unavailable");}
        int selected=-1;SIZE_T best=0;
        for(UINT i=0;;++i){IDXGIAdapter1 *adapter=nullptr;if(factory->EnumAdapters1(i,&adapter)==DXGI_ERROR_NOT_FOUND)break;
            if(!adapter)break;
            DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);adapter->Release();
            if(!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)&&(selected<0||desc.DedicatedVideoMemory>best)){
                selected=int(i);best=desc.DedicatedVideoMemory;deviceName=desc.Description;}}
        factory->Release();FreeLibrary(dxgi);
        if(selected<0)throw std::runtime_error("No hardware GPU adapter");
        return selected;
    }
};
#endif
