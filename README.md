# Oxylus Engine
![](/Resources/Branding/OXLogoBanner.png)     
## About   
Render and Game engine built with Vulkan and C++. It is focused on  stylistic and realistic 3D rendering.        
This is my hobby project that I work on in my spare time to learn more about graphics programming and engine architectures. Also to produce some games including my dream game in the future.

Currently Windows, Linux and Mac (with MoltenVK) is supported.

## Features:     
- Editor with features like Projects, Scenes and Saving/Loading Scenes, Content
Browser, Prefabs, Shader Hot Reloading, Entity Parenting, and other entity
manipulations, Inspector Panel which draws any component with its properties,
Asset Manager, Material System&Editor, In-Editor Console, and a lot more QOL
features...
- Abstracted Vulkan renderer with Render Graph built from scratch with ECS
- Modern rendering features; Clustered Forward IBL PBR, SSAO, SSR, PCF Shadows,
Bloom, Depth Of Field also various Post Processing techniques like HDR
Tonemapping, Chromatic Aberration, Film Grain, Vignette, Sharpen
- Multithreaded physics with Jolt.   
- Game scripting API with ECS events and ECS systems. Which has been used to
build multiple games to test the API and engine in general.
 Read more: [(Creating a Game With Oxylus)](https://hatrickek.github.io/blog/oxylus-first-game)
- 3D Audio with miniaudio

## Showcase
![PostProcessing](https://cdn.discordapp.com/attachments/882355531463938078/1101916100414931066/image.png)
PBR Testing scene with Depth Of Field, SSR, SSAO, Bloom, Vignette, Film Grain, Chromatic Aberration, Sharpen
![SSR](https://cdn.discordapp.com/attachments/1012357737256058924/1093471555679432815/image.png)
[![SSR](https://cdn.discordapp.com/attachments/1012357737256058924/1095085960858976387/image.png)](https://youtu.be/nu4_uiTNB5Q)    
Sponza scene with IBL PBR, SSAO, SSR and Directional Shadows

## Building
### CMake
- Run the root CMake script with a command like this:       
`cmake -S . -B ./Build/ -G "Visual Studio 17 2022" -A x64` to generate Visual Studio files (or with any generator you like) into `Build` folder.   
Or with `cmake -B ./Build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++` to generate for clang.    
- Then run this command to build it with CMake:   
`cmake --build ./Build -j 2`   
- Vulkan SDK must be installed and VULKAN_SDK env path must be set in order to build the project (It is most of the time automatically set by the Vulkan SDK installer).  

## Dependencies
- [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)
- [GLFW](https://github.com/glfw/glfw)
- [entt](https://github.com/skypjack/entt)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [GLM](https://github.com/g-truc/glm)
- [Jolt](https://github.com/jrouwe/JoltPhysics)
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
