# Oxylus Engine
![](/resources/branding/OXLogoBanner.png)     
## About   
Render and Game engine using Vulkan. It is focused on realistic and stylistic 3D rendering.    
This is my hobby project that I work on in my spare time to learn more about graphics programming and actually make my dream game with it.      
    
Windows and Mac support with MoltenVK.

## Features:     
- Full blown editor with asset browser.  
- Forward+ Rendering with PBR, Skybox, Bloom, SSR, SSAO, Shadows etc. Using own Vulkan API.
- 3D Model and Material workflow. (Prefabs etc.)
- Physics with Jolt.   
- Most of the engine including renderer is built with ECS using entt.
- Game API with events and ecs systems. [(Read more..)](https://hatrickek.github.io/blog/oxylus-first-game)
- 3D Audio with miniaudio

## Showcase
![SSR](https://cdn.discordapp.com/attachments/1012357737256058924/1093471555679432815/image.png)
Sponza scene with SSAO, SSR and Directional Shadows

## Building
### Cmake
- Directly run the root CMake script with a command like this:    
`cmake -S . -B ./build/ -G "Visual Studio 17 2022" -A x64` to generate Visual Studio files in a `build` folder or open the project in CLion.

## Special Mentions and Thanks To
- Yan Chernikov aka [The Cherno](https://www.youtube.com/channel/UCQ-W1KE9EYfdxhL6S4twUNw) for his great video series, streams.
- [Cem Yuksel](https://www.youtube.com/@cem_yuksel/videos) for his great videos about graphics programming.
- Jason Gregory for his [Game Engine Architecture](https://www.gameenginebook.com/) book.
- [SaschaWillems](https://github.com/SaschaWillems/Vulkan) for his Vulkan examples and help. 