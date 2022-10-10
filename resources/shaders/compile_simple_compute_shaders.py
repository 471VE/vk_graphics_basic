import os
import subprocess
import pathlib

if __name__ == '__main__':
    glslang_cmd = "glslangValidator"

    shader_list = ["simple.comp", "simplev2.comp"]

    for shader in shader_list:
        subprocess.run([glslang_cmd, "--target-env", "vulkan1.1", shader, "-o", "{}.spv".format(shader)])

