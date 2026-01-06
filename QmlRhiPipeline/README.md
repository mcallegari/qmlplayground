# rhipipeline

QRhi-based deferred renderer skeleton with a modular pass architecture.

## Goals
- Cross-backend via QRhi
- Deferred PBR shading with efficient many-light support (tiled/clustered)
- Shadows (CSM for directional, spot shadows), IBL ready
- Assimp-based mesh loading

## Structure
- `src/core`: QRhi context, render graph, render targets, shader cache
- `src/renderer`: passes (depth, gbuffer, light culling, lighting, shadows, post)
- `src/scene`: camera, scene graph, meshes, materials, lights, assimp loader
- `shaders`: Vulkan-style GLSL

This is a skeleton; pass implementations are TODO placeholders.
