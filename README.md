# Oxylus Engine
![](/resources/branding/OXLogoBanner.png)     
## About   
Render and Game engine using Vulkan. It is focused on realistic and stylistic 3D rendering.    
This is my hobby project that I work on in my spare time to learn more about graphics programming and actually make my dream game with it.      
Currently only Windows is supported but Mac support is planned.

## Features:     
- Editor with scene editing, saving/loading, asset browser.  
- Forward+ Rendering with PBR, Skybox, Bloom and SSAO.     
- 3D Model and Texture workflow.   
- Physics with Physx.   
- ECS with entt.
- Native scripting.   

## Showcase
//TODO

## Building
### With Premake:
1. Clone the project recursively ``git clone --recursive https://github.com/Hatrickek/Oxylus-Engine``    
2. Run the [Setup.bat](https://github.com/Hatrickek/Oxylus-Engine/blob/main/scripts/Setup.bat). This will download the required prerequisites for the project if they are not present yet.    
3. One prerequisite is the Vulkan SDK. If it is not installed, the script will execute the `VulkanSDK.exe` file, and will prompt the user to install the SDK.
4. After installation, run the [Setup.bat](https://github.com/Hatrickek/Oxylus-Engine/blob/main/scripts/Setup.bat) file again. If the Vulkan SDK is installed properly, it will then download the Vulkan SDK Debug libraries. (This may take a longer amount of time)
5. After downloading and unzipping the files, the [Win-GenProjects.bat](https://github.com/Hatrickek/Oxylus-Engine/blob/main/scripts/Win-GenProjects.bat) script file will get executed automatically, which will then generate a Visual Studio solution file for user's usage.3. 

### With Cmake
1. Do the same processes from Premake for getting prerequisites.
2. Generate the project using cmake-gui or with `cmake -G "Visual Studio 17 2022` command. If visual studio is not preferred then it could be used with any other IDE.

## Special Mentions and Thanks To
- Yan Chernikov aka [The Cherno](https://www.youtube.com/channel/UCQ-W1KE9EYfdxhL6S4twUNw) for his great video series, streams and being an inspiration to me. 
- Jason Gregory for his [Game Engine Architecture](https://www.gameenginebook.com/) book.
- [SaschaWillems](https://github.com/SaschaWillems/Vulkan) for his Vulkan examples and help. 