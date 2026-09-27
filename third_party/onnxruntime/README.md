# ONNX Runtime C API header

Unmodified `onnxruntime_c_api.h` from Microsoft.ML.OnnxRuntime.DirectML 1.22.0 (MIT).
The complete license is in LICENSE. MinGW calling-convention compatibility is handled in service/ortgpusession.h, not by modifying this vendor header.

Binary runtime and DirectML dependency are downloaded by scripts/setup-gpu-runtime.ps1 into ignored artifacts, with pinned versions and SHA-256 verification. Deployment copies the GPU runtime and its licenses beside the executable in gpu/.
