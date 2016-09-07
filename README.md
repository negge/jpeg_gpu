GPU Accelerated JPEG Rendering

JPEG is the de facto standard for natural image compression on the web.
Most web browsers use the open source libjpeg-turbo for decoding JPEG files
 because a) it is libre software and b) there has been significant investment
 in SIMD optimization.
However, in applications where the resulting image is immediately uploaded to
 the GPU for compositing (like in modern web browsers) this approach has two
 drawbacks:

 - The full uncompressed image must be uploaded to the GPU
 - Time spent decoding on the CPU robs computation from other tasks

These issues are exacerbated as more content is provided in high definition.

The jpeg_gpu project aims to solve both these problems by shifting the decode
 computation as early as possible to the GPU.
Partially decoded JPEG data is uploaded as soon as the entropy coded symbols
 are read, and a set of pre-defined shaders complete the rest of the decode
 into a GPU texture.
Depending on the resolution of the image and how much it is compressed, the
 speed-up can be significant.
