# Llama.cpp Parameters Configuration Guide

This guide explains all the available parameters for loading models in Kolosal Server, which are based on the comprehensive llama.cpp common_params structure.

## Basic Parameters

### Model Configuration
- **Model ID**: Unique identifier for your model
- **Model Path**: Path to the GGUF model file
- **Model Type**: Type of model (LLM or Embedding)
- **Load Immediately**: Whether to load the model immediately after adding
- **Main GPU ID**: Primary GPU to use (-1 for auto-select)
- **Inference Engine**: Specific inference engine to use (optional)

### Context and Memory
- **Context Size (n_ctx)**: Maximum context size in tokens (default: 4096)
- **Keep Tokens (n_keep)**: Number of tokens to keep in memory (default: 2048)
- **Use Memory Lock (mlock)**: Lock memory pages to prevent swapping
- **Use Memory Mapping (mmap)**: Use memory mapping for faster loading

### Processing Settings
- **Continuous Batching**: Enable continuous batching for better throughput
- **Warmup**: Perform warmup before inference
- **Parallel Sequences**: Number of parallel sequences to process

### Batch Processing
- **Batch Size (n_batch)**: Prompt processing batch size (default: 2048)
- **Micro-batch Size (n_ubatch)**: Physical batch size for prompt processing (default: 512)

### Hardware Acceleration
- **GPU Layers**: Number of layers to offload to GPU (0 = CPU only)
- **Split Mode**: How to split model across multiple GPUs
  - 0: None - Single GPU
  - 1: Layer - Split by layers
  - 2: Row - Split by tensor rows
- **Tensor Split**: Comma-separated fractions for multi-GPU split

## Advanced Parameters

### CPU Configuration
- **CPU Threads (n_threads)**: Number of CPU threads (0 = auto-detect)
- **Batch Threads (n_threads_batch)**: Threads for batch processing
- **CPU Affinity Mask**: CPU affinity mask in hexadecimal
- **Batch CPU Mask**: CPU affinity mask for batch processing
- **CPU Range**: CPU core range to use (e.g., "0-7" or "0,2,4")
- **Batch CPU Range**: CPU core range for batch processing
- **CPU Strict Mode**: CPU affinity enforcement level
  - 0: Disabled
  - 1: Enabled
  - 2: Force
- **Polling Mode**: Polling interval in milliseconds

### RoPE (Rotary Position Embedding)
- **RoPE Scaling Type**: Type of RoPE scaling to apply
  - unspecified: Let model decide
  - none: No scaling
  - linear: Linear scaling
  - yarn: YARN scaling
- **RoPE Frequency Base**: RoPE base frequency (0 = auto)
- **RoPE Frequency Scale**: RoPE frequency scaling factor (0 = auto)
- **YARN Extension Factor**: YARN extension factor (-1 = auto)
- **YARN Attention Factor**: YARN attention scaling factor
- **YARN Beta Fast**: YARN beta fast parameter
- **YARN Beta Slow**: YARN beta slow parameter
- **YARN Original Context**: YARN original context size (0 = auto)

### Memory Management
- **NUMA Strategy**: NUMA memory allocation strategy
  - disabled: No NUMA optimizations
  - distribute: Distribute memory across NUMA nodes
  - isolate: Isolate memory to specific NUMA nodes
  - numactl: Use numactl for memory management
- **Defragmentation Threshold**: KV cache defragmentation threshold (-1 = disabled)
- **Disable GPU Acceleration**: Force CPU-only processing

### Cache Configuration
- **K-Cache Type**: Key cache quantization type
- **V-Cache Type**: Value cache quantization type
  - unspecified: Use model default
  - f16: 16-bit floating point
  - q8_0, q4_0, q4_1, iq4_nl, q5_0, q5_1: Various quantization levels

### Embedding Configuration
- **Enable Embedding Mode**: Use model for embeddings instead of text generation
- **Pooling Type**: Pooling method for embeddings
  - unspecified, none, mean, cls, last, rank
- **Attention Type**: Attention mechanism type
  - unspecified, causal, non_causal

### Advanced Features
- **Return Logits for All Tokens**: Return logits for all tokens, not just the last one
- **Use Flash Attention**: Enable Flash Attention for improved performance
- **Disable Performance Metrics**: Disable performance measurement and reporting
- **Simple I/O Mode**: Use simple I/O for compatibility
- **Use Color Output**: Enable colored output in logs
- **Special Token Processing**: Enable special token processing
- **Interactive First Mode**: Run in interactive mode initially
- **Conversation Mode**: Enable conversation mode
- **ChatML Format**: Use ChatML message format
- **Hide Prompt Display**: Don't display the prompt in output

### Sampling Parameters
- **Random Seed**: Random seed for generation (-1 = random)
- **Max Tokens to Generate**: Maximum tokens to generate (-1 = unlimited)
- **Group Attention N**: Group attention factor
- **Group Attention Width**: Group attention width

## Usage Tips

### For Performance Optimization
1. **GPU Configuration**: Start with `n_gpu_layers` set to a high value (like 100) to offload as much as possible to GPU
2. **Memory Settings**: Enable `use_mmap` for faster loading, enable `use_mlock` if you have enough RAM
3. **Batch Sizes**: Increase `n_batch` and `n_ubatch` for better throughput if you have sufficient VRAM
4. **CPU Threads**: Set `n_threads` to match your CPU cores for optimal CPU performance

### For Multi-GPU Setups
1. Set `split_mode` to 1 (Layer) for most cases
2. Use `tensor_split` to specify memory allocation ratios (e.g., "0.6,0.4" for 60%/40% split)
3. Ensure `n_gpu_layers` is high enough to utilize all GPUs

### For Large Context Models
1. Increase `n_ctx` to the desired context size
2. Consider using RoPE scaling for contexts larger than the model's training context
3. Monitor VRAM usage and adjust `n_gpu_layers` accordingly

### For Embedding Models
1. Enable `embedding` mode
2. Set appropriate `pooling_type` (usually "mean" or "cls")
3. Set `attention` to "non_causal" for bidirectional attention

## Troubleshooting

### Out of Memory Errors
- Reduce `n_gpu_layers`
- Decrease `n_batch` and `n_ubatch`
- Reduce `n_ctx`
- Enable `use_mmap` and disable `use_mlock`

### Slow Performance
- Increase `n_gpu_layers` if you have VRAM available
- Increase `n_batch` for better throughput
- Enable `cont_batching` for server workloads
- Check CPU thread allocation with `n_threads`

### Model Loading Issues
- Verify the model path is correct
- Check if the model format is supported (GGUF)
- Ensure sufficient disk space and memory
- Try disabling advanced features first

## API Integration

When using the API, these parameters are sent in the `loading_parameters` object of the model loading request:

```json
{
  "model_id": "my-model",
  "model_path": "./models/model.gguf",
  "model_type": "llm",
  "load_immediately": true,
  "main_gpu_id": 0,
  "loading_parameters": {
    "n_ctx": 4096,
    "n_gpu_layers": 100,
    "use_mmap": true,
    "rope_scaling_type": "linear",
    // ... other parameters
  }
}
```

Refer to the backend API documentation for the complete parameter structure and validation rules.