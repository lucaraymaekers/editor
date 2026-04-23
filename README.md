# Editor

Editor is my personal codebase written from scratch in C using very few libraries.
It runs on Linux and Windows (OpenGL) with nice debug features such as code profiling, hot reloading, loop editing, saving/loading replays and state to/from disk. 

### Demo's

A "midi editor".
![Watch the demo](https://github.com/lucaraymaekers/editor/blob/main/data/showcase_muze_260423.gif)

A "text editor".
![Watch the demo](https://github.com/lucaraymaekers/editor/blob/main/data/showcase_editor_260404.gif)

### Building 
This will ultimately run "cling", a self-hosted C build script that is also acts as the metaprogram when required.

On **Linux**, you will need [clang](https://clang.llvm.org/get_started.html).

```sh
./code/build.sh
```

On **Windows**, you will need [msvc](https://visualstudio.microsoft.com/downloads/) (I recommend installing the toolchain using this [gist](https://gist.github.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977)).

```bat
.\code\build.bat
```

