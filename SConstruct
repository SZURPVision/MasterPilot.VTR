#!/usr/bin/env python
import os
import shutil
import subprocess
import sys
from typing import List
from SCons.Script import SConscript, Glob, Default
from SCons.Environment import Environment as SConsEnvironment

# 路径感知：默认在当前目录找 godot-cpp，也可通过环境变量覆盖
godot_cpp_path = os.environ.get("GODOT_CPP_PATH", "godot-cpp")

if not os.path.exists(os.path.join(godot_cpp_path, "SConstruct")):
    print(f"Error: godot-cpp not found at {godot_cpp_path}. Please check NIX_CONTEXT or submodules.")
    sys.exit(1)

# --- 核心：一致性编译 ---
# 直接调用官方 SConscript，它会自动应用 Godot 所需的所有宏定义、ABI 设置和优化参数
# 由于我们会在 Nix 环境中注入预编译好的 .o 文件，这里运行会非常快
env: SConsEnvironment = SConscript(os.path.join(godot_cpp_path, "SConstruct")) # type: ignore
env["ENV"].update(os.environ)

env.Append(CPPPATH=["src"])
env.Append(CCFLAGS=["-std=c++23", "-fPIC"])

# --- FFmpeg Linking ---
env.ParseConfig("pkg-config --cflags --libs libavcodec libavformat libavutil libswscale libavdevice")

# --- 源码与目标 ---
sources = Glob("src/*.cpp")
target_name = "addons/vtr_texture/bin/vtrtexture{}{}".format(
    env["suffix"], env["SHLIBSUFFIX"] # type: ignore
)

library = env.SharedLibrary(target=target_name, source=sources)

# --- 编译数据库 ---
# 在 Nix 构建沙盒中跳过，因为路径是临时的
if hasattr(env, "CompilationDatabase") and os.environ.get("VTR_NIX_BUILD") != "1":
    compile_db = env.CompilationDatabase(target="compile_commands.json") # type: ignore
    Default(library, compile_db)
else:
    Default(library)
