# Learning [The Forge Rendering API](https://github.com/ConfettiFX/The-Forge)

### Implementations
  - [x] [Draw Textured Quad](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/01_HelloQuad)
  - [x] [Instancing](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/02_Instancing)
  - [x] [Phong Shading](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/03_PhongShading)
  - [x] [Light Maps](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/04_LightMapping)
  - [x] [Lighting (Blinn-Phong)](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/04_LightMapping)
    - [x] Directional Light
    - [x] Point Light
    - [x] Spot Light
  - [x] [Load Model](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/05_LoadingModel)
  - [x] [Gooch Shading](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/06_GoochShading)
  - [x] Profiling and UI
  - [ ] **Deferred Lighting**
  - [ ] [Ambient Occlusion](https://github.com/Erfan-Ahmadi/AmbientOcclusion)
  - [ ] Gamma Correction
  - [ ] Tone Mapping / HDR
### Build
  - [x] Visual Studio 2017:
    * Put Repository folder next to cloned [The-Forge API](https://github.com/ConfettiFX/The-Forge) repository folder
    * Build [The-Forge API](https://github.com/ConfettiFX/The-Forge) Renderer and Tools and place library files in **build/VisualStudio2017/$(Configuration)** for example build/VisualStudio2017/ReleaseDx
    * Assimp: 
      - Build Assimp for Projects involving Loading Models
      - Don't change the directory of assimp.lib, the Default is: 
         - **The-Forge\Common_3\ThirdParty\OpenSource\assimp\4.1.0\win64\Bin**
