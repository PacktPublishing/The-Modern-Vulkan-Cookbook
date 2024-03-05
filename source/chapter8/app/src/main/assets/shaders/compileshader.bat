cd /D "%~dp0"
glslangvalidator -Od -g --target-env vulkan1.1 indirectdrawMV.vert -o indirectdrawMVvert.spv
glslangvalidator -Od -g --target-env vulkan1.1 indirectdrawMV.frag -o indirectdrawMVfrag.spv