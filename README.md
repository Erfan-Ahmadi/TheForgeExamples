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
  - [x] [Deferred Lighting](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/07_DefferedLighting)
  - [x] [Stencil Buffer Toon Shading](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/08_ToonShading)
  - [x] [Tessellation](https://github.com/Erfan-Ahmadi/the_forge_learn/tree/master/src/09_Tessellation)
    - [x] Passthrough
    - [x] PN-Triangles
  - [x] Bloom
  - [x] HDR Rendering
  - [x] Gamma Correction
  - [ ] Dynamic Tone Mapping / HDR
  - [ ] Environmental Mapping
    - [ ] Cube Map Reflections
    - [ ] Spherical Map Reflections
  - [ ] Normal Mapping
  - [ ] Parallax Mapping
  - [ ] Shadow Mapping
  - [ ] [Ambient Occlusion](https://github.com/Erfan-Ahmadi/AmbientOcclusion)
  - [ ] LOD
  - [ ] Occlusion Query
  - [ ] Motion Blur
  
### ScreenShots

#### 1. Instancing
<p align="center">
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/instancing.gif" alt="" width="600" height="400" />
</p>

#### 2. Blinn-Phong Lightmapping
Using Blinn-Phong Lighting model with 3 types of lights: **1.Point Light 2.Directional Light 3.Spot Light** and Specular Maps to highlight the outer edges of the cubes. [Blinn-Phong LearnOpenGL](https://learnopengl.com/Lighting/Multiple-lights)
<p align="center">
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/lightmapping.gif" alt="" width="600" height="400" />
</p>

#### 3. Deferred Lighting
Blinn-Phong Deferred Rendering. It writes to normal/position/color map in the 1st Pass, Then uses the Lights Data Passed to GPU as Uniform and Structured Buffers to calculate the colors using these textures.

Deferred Rendering doesn't work too well on High Resolution (**2K**/**4K**) devices such as consoles due to it's memory and It's suggested to use [Visibility Buffers](http://jcgt.org/published/0002/02/04/) instead.

Also on Mobile devices it's good to uses subpasses for tile-based rendering instead of Render Passes, again because of high memory size.

<p align="center">
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/deffered.gif" alt="" width="600" height="400" />
</p>

#### 4. Toon Shading
Simple Toon Shading with outlining. It uses **Stencil Buffers** to draw the cool outlines.
<p align="center">
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/toonshade.gif" alt="" width="600" height="400" />
</p>

#### 5. Tessellation (Passthrough and PN Triangles)
Tessellation using Hull/Control and Domain/Evaluation Shader in HLSL and GLSL. PN-Triangles is a method using **Bezier Triangles** and does not require a heightmap to improve quality of the geometry unlike the first technique which is simple and passthrough tessellation
<p align="center">
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/tessellation_passthrough.gif" alt="" width="600" height="400" />
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/tessellation_pntriangles_suzanne.gif" alt="" width="600" height="400" />
</p>

#### 6. Bloom
Bloom gives noticeable visual cues about the brightness of objects as bloom tends to give the illusion objects are really bright.

The technique used is to render the bloom-affected objects to a 256x256 FBO and then blurred and added to the final scene.
Technique based on **GPU Pro 2 : Post-Processing Effects on Mobile Devices** and [SachaWilliens Vulkan Example](https://github.com/SaschaWillems/Vulkan/tree/master/examples/bloom)

The Final pass is **tonemapping/exposure control** and all the frame buffers before that are rendered as HDR (R16G16B16A16)

<p align="center">
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/bloom.gif" alt="" width="600"/>
  <img src="https://github.com/Erfan-Ahmadi/TheForgeExamples/raw/master/screenshots/bloom2.gif" alt="" width="600"/>
</p>

### Build
  - [x] Visual Studio 2017:
    * Put Repository folder next to cloned [The-Forge API](https://github.com/ConfettiFX/The-Forge) repository folder
    * Build [The-Forge API](https://github.com/ConfettiFX/The-Forge) Renderer and Tools and place library files in **build/VisualStudio2017/$(Configuration)** for example build/VisualStudio2017/ReleaseDx
    * Assimp: 
      - Build Assimp for Projects involving Loading Models
      - Don't change the directory of assimp.lib, the Default is: 
         - **The-Forge\Common_3\ThirdParty\OpenSource\assimp\4.1.0\win64\Bin**
