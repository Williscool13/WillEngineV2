# Minimum Compute Renderer
A snapshot of the engine's renderer to test new systems if they are able to run minimum vulkan properties.
#### This snapshot renders a background with a compute shader and a color-shifting triangle with a vertex + fragment shader.

This snapshot uses the following features and extensions:
- 1.3 Features
  - dynamicRendering
  - synchronization2
- 1.2 Features
  - bufferDeviceAddress
  - descriptorIndexing
- Other Features
  - multiDrawIndirect (not used, but loaded)
  - descriptorBuffer

