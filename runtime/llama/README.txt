Put llama.cpp's runtime here.

After building llama.cpp with CUDA on your internet-connected machine, copy the
ENTIRE contents of its build output (build/bin/Release/) into runtime/llama/ (which the package script copies next to the exe as llama/):

  llama-server.exe
  llama.dll
  ggml.dll
  ggml-base.dll
  ggml-cpu.dll
  ggml-cuda.dll
  ... (and any other .dll produced next to llama-server.exe)

If the airgapped machine does not already have the matching NVIDIA CUDA runtime
installed, also drop the CUDA runtime DLLs next to llama-server.exe, e.g.:

  cudart64_*.dll
  cublas64_*.dll
  cublasLt64_*.dll

(grab these from your CUDA Toolkit bin folder on the connected machine).

llama-server is launched automatically by the app using the path in config.json.
