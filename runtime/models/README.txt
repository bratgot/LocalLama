Put your GGUF model file here and point config.json -> "model_path" at it.

Download a quantized instruct model (.gguf) on your connected machine, e.g. a
Q4_K_M quant of a 3B-8B instruction-tuned model, then copy it here and rename it
to match config.json (default: model.gguf), or edit "model_path" in config.json.

With an NVIDIA GPU and "n_gpu_layers": 99 the whole model is offloaded to VRAM.
Rough VRAM guide for Q4_K_M: ~3B needs ~3 GB, ~7-8B needs ~6 GB, plus the KV
cache for your context size. If you run out of VRAM, lower n_gpu_layers in
config.json (the rest runs on CPU) or pick a smaller model / quant.

Check the license of the specific model AND size you choose before relying on it
("open source" varies: some are Apache-2.0 or MIT, others ship custom licenses
with usage restrictions).
