# The Modern Vulkan Cookbook

This is the code repository for [The Modern Vulkan Cookbook](https://www.packtpub.com/product/the-modern-vulkan-cookbook/9781803239989), published by Packt.

## What is this book about?
Vulkan is a graphics API that gives the program total control of the GPU, allowing the GPU to be used to its full potential. This book is designed to guide you through the intricacies of Vulkan, starting with a solid foundation of basic concepts. You'll learn how to implement a basic graphics engine from scratch, giving you hands-on experience with the API's core features. As you progress through the chapters, you'll gain a deep understanding of Vulkan's architecture and how to leverage its capabilities effectively. Once you've mastered the fundamentals, the book delves into advanced techniques that will take your graphics programming skills to the next level.

This cookbook will uncover following useful techniques:

* Using Programmable Vertex Pulling & Multidraw Indirect
* GPU Driven rendering
* Implementing basic deferred renderer along with shadows & reflections (using screen space techniques)
* Implementing various OIT techniques such as Depth Peeling, Linked-List OIT, WOIT
* Exploring various Anti aliasing techniques such as DLSS, TAA, FXAA, MSAA.
* Implementing a PBR Ray Tracer
* Using OpenXR for AR/VR apps as well as using static/dynamic foveated rendering

## Build Instructions

The code has been tested with the following software:
- Visual Studio 2022 Community Edition 16.11.8 (Windows)
- CMake 3.27.2
- Vulkan SDK 1.3.268.0

Only windows platform is tested. The code was tested with following Nvidia GPUs

- GTX 1060
- GTX 1080
- RTX 3050
- RTX 4060

## Syncing code

We recommend that you clone the repo instead of directly downloading it, this can be done using `git clone https://github.com/PacktPublishing/The-Modern-Vulkan-Cookbook.git`

If you encounter an error related to the LFS (Large File Storage) file Bistro.glb while cloning the repository, the error might look like this: Error downloading object: source/common/resources/assets/Bistro.glb: Smudge error

1.) set GIT_LFS_SKIP_SMUDGE=1 on terminal
2.) git clone https://github.com/PacktPublishing/The-Modern-Vulkan-Cookbook.git
3.) Download Bistro.glb from https://1drv.ms/u/s!AsOHFR8SHZcxhONARRHaUFqKd8BXOA?e=KsdErS and copy it in source/common/resources/assets, replace existing Bistro.glb with the new one you downloaded. 

By following these steps, you should be able to clone the repository and resolve any LFS-related errors with the Bistro.glb file.

After cloning, you can open the Project in Visual Studio 2022: Launch Visual Studio 2022. Navigate to File | Open | Folder and select the folder where you cloned the repository. This action will load the project into Visual Studio. Click Build all to build all project, you should be able to select an executable for each chapter from this single solution. 
