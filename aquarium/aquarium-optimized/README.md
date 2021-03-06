#  Native Aquarium
Native Aquarium is a native implementation of [WebGL Aquarium](https://github.com/WebGLSamples/WebGLSamples.github.io). The goal of this project is to compare the performance of WebGL version to its native counterparts, such as OpenGL, D3D, Vulkan, Metal, Angle and Dawn.

# Progress
<table>
  <tr align=center>
    <td><strong>OS</td>
    <td><strong>Backend</td>
    <td><strong>Supported</td>
  </tr>
  <tr align=left>
    <td>Linux</td>
    <td>OpenGL</td>
    <td>Y</td>
  </tr>
  <tr align=left>
    <td>macOS</td>
    <td>OpenGL</td>
    <td>Y</td>
  </tr>
  <tr align=left>
    <td>Windows</td>
    <td>OpenGL</td>
    <td>Y</td>
  </tr>
  <tr align=left>
    <td>Windows</td>
    <td>Angle</td>
    <td>Y</td>
  </tr>
  <tr align=left>
    <td>Linux</td>
    <td>Dawn</td>
    <td>Y</td>
  </tr>
  <tr align=left>
    <td>macOS</td>
    <td>Dawn</td>
    <td>Y</td>
  </tr>
  <tr align=left>
    <td>Windows</td>
    <td>Dawn</td>
    <td>Y</td>
  </tr>
  <tr align=left>
    <td>*</td>
    <td>D3D</td>
    <td>N</td>
  </tr>
  <tr align=left>
    <td>*</td>
    <td>Vulkan</td>
    <td>N</td>
  </tr>
  <tr align=left>
    <td>*</td>
    <td>Metal</td>
    <td>N</td>
  </tr>
</table>


## Build OpenGL version

Native Aquarium uses cmake to build on Linux, macOS and Windows.
```sh
# cd the repo
cd aquarium-optimized

# get submodules
git submodule init && git submodule update

# make build directory
mkdir build && cd build

# build on Windows
cmake -G "Visual Studio 15 2017 Win64" ..
open build/Aquarium.sln using visual studio, set Aquarium as StartUp project and build

# build on Linux or macOS
cmake ..
make
```

## Build Dawn version
First build dawn by gn, then link libs to Aquarium and build by cmake.
```sh
# cd the repo
cd aquarium-optimized

# get submodules
git submodule init && git submodule update

#build dawn
#If you want to enable opengl backend of dawn, please add DAWN_ENABLE_BACKEND_OPENGL to args.gn
cd thirdparty/dawn
cp scripts/standalone.gclient .gclient
gclient sync
gn gen out/Debug --args="is_debug=true is_clang=false"
ninja -C out/Debug CppHelloTriangle
gn gen out/Release --args="is_debug=false is_clang=false"
ninja -C out/Release CppHelloTriangle
```
#build aquarium
```sh
# make build directory
mkdir build && cd build

# build on Windows
# Change Runtime Library in project properties to MT if dawn libs can't be linked
cmake -G "Visual Studio 15 2017 Win64" .. -Dangle=false -Ddawn=true
open build/Aquarium.sln using visual studio, set Aquarium as StartUp project and build
copy libdawn.dll, libdawn_native.dll, libdawn_wire.dll, libshaderc.dll to folder build

# build on Linux or macOS
cmake .. -Dangle=false -Ddawn=true
make

#build on macOS by xcode
#The resource path need one more "../", please revise in Aquarium::updateUrls
cmake -G xcode .. -Dangle=false -Ddawn=true
build project by xcode
```

## Build Angle version
First build angle by gn, then link libs to Aquarium and build by cmake.
```sh
# cd the repo
cd aquarium-optimized

# get submodules
git submodule init && git submodule update

#build dawn
cd thirdparty/angle
gclient sync
gn gen out/Debug --args="is_debug=true is_clang=false"
ninja -C out/Debug libEGL libGLESv2
gn gen out/Release --args="is_debug=false is_clang=false"
ninja -C out/Release libEGL libGLESv2
```
#build aquarium
```sh
# make build directory
mkdir build && cd build

# build on Windows
cmake -G "Visual Studio 15 2017 Win64" .. -Dangle=true -Ddawn=false
open build/Aquarium.sln using visual studio, set Aquarium as StartUp project and build
copy libEGL.dll libGLESv2.dll to folder build

# build on Linux or macOS
cmake .. -Dangle=true -Ddawn=false
make
```

# Run
```sh
# "--num-fish": specifies how many fishes will be rendered
# "--backend" : specifies running a certain backend, 'opengl', 'dawn_d3d12', 'dawn_vulkan', 'dawn_metal', 'dawn_opengl'
# running angle dynamic backend is on todo list. Currently go through angle path by option 'opengl' if angle is linked into the project
# MSAA is disabled by default. To Enable MSAA of OpenGL backend, "--enable-msaa", 4 samples.
# MSAA of angle or dawn backend is not supported now.

# run on Windows
run it in Visual Studio
or
aquarium.exe --num-fish 10000 --backend opengl --enable-msaa
aquarium.exe --num-fish 10000 --backend dawn_d3d12

# run on Linux
./aquarium  --num-fish 10000 --backend opengl
./aquarium.exe --num-fish 10000 --backend dawn_vulkan

# run on macOS
./aquarium  --num-fish 10000 --backend opengl
./aquarium.exe --num-fish 10000 --backend dawn_metal
```
