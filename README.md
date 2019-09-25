# Learning and Testing [The Forge Rendering API](https://github.com/ConfettiFX/The-Forge)

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
  - [x] [**Deferred Lighting**](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/07_DefferedLighting)
  - [ ] Environmental Mapping
    - [ ] Cube Map Reflections
    - [ ] Spherical Map Reflections
  - [ ] Tessellation
  - [ ] Normal Mapping
  - [ ] Parallax Mapping
  - [ ] Stencil Buffer Toon Shading
  - [ ] Shadow Mapping
  - [ ] [Ambient Occlusion](https://github.com/Erfan-Ahmadi/AmbientOcclusion)
  - [ ] LOD
  - [ ] Occlusion Query
  - [ ] Bloom
  - [ ] Motion Blur
  - [ ] Gamma Correction
  - [ ] Tone Mapping / HDR
  
### ScreenShots

#### 1. Instancing
<p align="center">
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/instancing.gif" alt="" width="300" height="200" />
</p>

#### 2. Blinn-Phon Lightmapping
<p align="center">
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/lightmapping.gif" alt="" width="300" height="200" />
</p>

#### 3. Deffered Lighting
<p align="center">
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/deffered.gif" alt="" width="300" height="200" />
</p>

### Build
  - [x] Visual Studio 2017:
    * Put Repository folder next to cloned [The-Forge API](https://github.com/ConfettiFX/The-Forge) repository folder
    * Build [The-Forge API](https://github.com/ConfettiFX/The-Forge) Renderer and Tools and place library files in **build/VisualStudio2017/$(Configuration)** for example build/VisualStudio2017/ReleaseDx
    * Assimp: 
      - Build Assimp for Projects involving Loading Models
      - Don't change the directory of assimp.lib, the Default is: 
         - **The-Forge\Common_3\ThirdParty\OpenSource\assimp\4.1.0\win64\Bin**
