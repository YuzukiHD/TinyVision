<img src="https://i0.wp.com/www.oshwa.org/wp-content/uploads/2014/03/oshw-logo-100-px.png" align="right" width=120 />

# TinyVision

TinyVision - A Tiny Linux Board / IPC / Server / Router / And so on...

[OS Release](https://github.com/YuzukiHD/TinyVision/releases) — [Documents](https://yuzukihd.top/TinyVision/#/) — [Tools](https://github.com/YuzukiHD/TinyVision/blob/main/docs/tools/) 

![image](https://github.com/YuzukiHD/TinyVision/assets/12003087/2ace3c9f-f9e0-4670-8d4c-b11dd3ef262a)

## Feature

Introducing TinyVision, the ultimate all-in-one solution for your Linux board, IPC, server, router, and more. Powered by the advanced Allwinner V851se or V851s3 processors, TinyVision offers exceptional performance and versatility in a compact form factor.

With its Cortex-A7 core running at up to 1200MHz and RISC-V E907GC@600MHz, TinyVision delivers powerful processing capabilities while maintaining energy efficiency. The integrated 0.5Tops@int8 NPU enables efficient AI inference for various applications.

TinyVision comes with built-in memory options of 64M DDR2 (V851se) or 128M DDR3L (V851s3), ensuring smooth and seamless operations. The TF card slot supports UHS-SDR104, providing expandable storage for your data needs. Additionally, the onboard SD NAND and USB&UART Combo interfaces offer convenient connectivity options.

Enhance your vision-based applications with TinyVision's support for one 2-lane MIPI CSI input, allowing seamless integration of advanced camera functionalities. The individual ISP enables high-resolution image processing, supporting resolutions of up to 2560 x 1440.

Enjoy immersive video experiences with TinyVision's H.264/H.265 decoding capabilities at up to 4096x4096 resolution. Capture and encode stunning moments with the H.264/H.265 encoder, supporting resolutions up to 3840x2160@20fps. And with online video encoding support, you can easily share and stream your content.

For reliable real-time operating system support, TinyVision harnesses the power of the RISC-V E907 RTOS based on RT-Thread + RTOS-HAL, ensuring optimal performance and stability.

Unlock endless possibilities with TinyVision, the compact yet powerful solution for your Linux board, IPC, server, router, and beyond. Experience unrivaled performance, enhanced capabilities, and seamless integration. Choose TinyVision and transform the way you innovate.

- Based on Allwinner V851se / V851s3 
- Cortex-A7 Core up to 1200MHz 
- RISC-V E907GC@600MHz
- 0.5Tops@int8 NPU
- Built in 64M DDR2 (V851se) / 128M DDR3L (V851s3) memory
- One TF Card Slot, Support UHS-SDR104
- On board SD NAND
- On board USB&UART Combo
- Supports one 2-lane MIPI CSI inputs
- Supports 1 individual ISP, with maximum resolution of 2560 x 1440
- H.264/H.265 decoding at 4096x4096
- H.264/H.265 encoder supports 3840x2160@20fps
- Online Video encode
- RISC-V E907 RTOS Support, Based on RT-Thread + RTOS-HAL

## Hardware 

### Camera Support

- GC2053
- GC2083

## Software Support

TinyVision supports a variety of kernels to choose from, and the following is a list of support options:

### Linux Release

- OpenWrt 22.03
- Buildroot

### Kernel Support

| Kernel Version     | Target ON                               | Core           | Path                |
| ------------------ | --------------------------------------- | -------------- | ------------------- |
| 4.9.191            | CV, Camera, NPU, MP, Video Encode, RTSP | Cortex-A7 Core | `kernel\linux-4.9`  |
| 5.15.138           | IoT, NPU, Router                        | Cortex-A7 Core | `kernel\linux-5.15` |
| 6.1.62             | IoT                                     | Cortex-A7 Core | `kernel\linux-6.1`  |
| Mainline Linux 6.7 | Mainline                                | Cortex-A7 Core | `kernel\linux-6.7`  |
| RT-Thread          | Real-Time Control, Fast                 | RISC-V E907    | `kernel\rtos`       |
| SyterKit           | Baremetal ASM Code                      | Cortex-A7 Core | `kernel\SyterKit`   |

## NPU

### NPU Operators

For machine learning, 0.5Tops NPU is provided as a computational accelerator, supporting the following 140+ operators

```
abs, add, addn, argmin, argmax, batch2space, batchnormalize, cast, clipbyvalue, concat, conv1d, convolution, conv3d, cropsandresize, deconvolution, depth2space, equal, exp, elu, embedding_lookup, erf, eltwise(MAX), floor, fullconnect, floor_div, gathernd, gather, gelu, gru, gru_cell, greater, greater_equal, image_resize, instancenormalize, localresponsenormalization_ tf, l2normalize, lstm, lstm_unit, less, less_equal, logical_or, logical_and, logical_xor, leakyrelu, multiply, moments, minimum, matmul, not_equal, neg, one_hot, pad, permute, pooling, poolwithargmax, pool3d, pow, reduceany, reducemax, reducemean, reducesum, reverse, relu, relun, rsqrt, real_div, repeat, reshape, round, stridedslice, sqrt, square, subtract, scatternd, stack, sigmoid, signalframe, slice, softmax, space2batch, space2depth, split, swish, tile, tanh, unstack, where, batch_matmul, expand_broadcast, cumsum, dequantize, divide, expanddims, hard_swish, localresponsenormalization, lstmunit, l2pooling, logical_not, log_softmax, maximum, nms, prelu, rank, reducemin, reverse_sequence, reverse_v2, segment_sum, shapelayer, sin, sparse_to_dense, svdf, squared_difference, topk, unique, conv2d_lstm, depthwise_convolution, relu_keras, keras_rnn_lstm, lstm_keras, ceil, celu, dropout, einsum, gather_elements, hard_sigmoid, log, roipooling, eltwise(MEAN), eltwise(MIN), mish, mod, nonzero, onehot, quantize, abs + reducesum, reducesum + multiply + sqrt, reducesum + log, exp + reducesum + log, reduceprod, multiply + reducesum, variable + divide, scatter_nd_update, sign, size, softrelu, abs + add + divide + variable, squeeze, eltwise(SUM), a_times_b_plus_c, eltwise, flatten, l2normalizescale, premute, priorbox, proposal, reorg, shuffle, region, upsampling, yolo
```



