# Oxylus Engine
![](/resources/branding/OXLogoBanner.png)     
## About   
Render and Game engine using Vulkan. It is focused on realistic and stylistic 3D rendering.    
This is my hobby project that I work on in my spare time to learn more about graphics programming and engine architectures. Also to produce some games including my dream game in the future.

Currently Windows and Mac (with MoltenVK) is supported.

## Features:     
- Full blown editor with asset browser.  
- Clustered Forward Rendering with IBL PBR, Skybox, Bloom, SSR, SSAO, Shadows etc. Using own Vulkan API created from scratch.
- 3D Model and Material workflow. (Prefabs etc.)
- Physics with Jolt.   
- Most of the engine including renderer is built with ECS using entt.
- Game API with events and ecs systems. Read more: [(Creating a Game With Oxylus)](https://hatrickek.github.io/blog/oxylus-first-game)
- 3D Audio with miniaudio

## Showcase
![SSR](https://cdn.discordapp.com/attachments/1012357737256058924/1093471555679432815/image.png)
[![SSR](https://cdn.discordapp.com/attachments/1012357737256058924/1095085960858976387/image.png)](https://youtu.be/nu4_uiTNB5Q)    
Sponza scene with IBL PBR, SSAO, SSR and Directional Shadows

## Building
### Cmake
- Directly run the root CMake script with a command like this:    
`cmake -S . -B ./build/ -G "Visual Studio 17 2022" -A x64` to generate Visual Studio files in a `build` folder or open the project in a CMake supported environment.
- Vulkan SDK must be installed and VULKAN_SDK env path in Windows and MacOS must be set in order to build the project (It is most of the time automatically set by the Vulkan SDK installer).  

## Dependencies
- [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)
- [GLFW](https://github.com/glfw/glfw)
- [entt](https://github.com/skypjack/entt)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [GLM](https://github.com/g-truc/glm)
- [MiniAudio](https://github.com/mackron/miniaudio)
- [Tracy](https://github.com/wolfpld/tracy)
- [ryml](https://github.com/biojppm/rapidyaml)
- [KTX](https://github.com/KhronosGroup/KTX-Software)
- [NFD](https://github.com/btzy/nativefiledialog-extended)
- [fmt](https://github.com/fmtlib/fmt)
- [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)

## Special Mentions and Thanks To
- Yan Chernikov aka [The Cherno](https://www.youtube.com/channel/UCQ-W1KE9EYfdxhL6S4twUNw) for his great video series, streams.
- [Cem Yuksel](https://www.youtube.com/@cem_yuksel/videos) for his great videos about graphics programming.
- Jason Gregory for his [Game Engine Architecture](https://www.gameenginebook.com/) book.
- [SaschaWillems](https://github.com/SaschaWillems/Vulkan) for his Vulkan examples and help. 