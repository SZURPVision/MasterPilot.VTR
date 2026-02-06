#!/usr/bin/env python
import os
import sys
from typing import List
# Explicitly import SCons symbols to satisfy VS Code / Pylance
from SCons.Script import SConscript, Glob, Default
from SCons.Environment import Environment as SConsEnvironment

# SCons is used to build the godot-cpp bindings first.
# We type-hint 'env' so IntelliSense knows available methods (Append, ParseConfig, etc.)
env: SConsEnvironment = SConscript("godot-cpp/SConstruct") # type: ignore

# --- Configuration ---

env.Append(CPPPATH=["src"])
env.Append(CCFLAGS=["-std=c++23", "-fPIC","-fvisibility=hidden"])
env.Append(LINKFLAGS=["-fvisibility=hidden"])

# --- Platform: Linux ---
# Accessing dictionary keys on 'env' usually returns generic types, so we check carefully.
platform: str = env["platform"] # type: ignore

if platform == "linux":
    # Use pkg-config to find FFmpeg libraries (Robust method)
    # We check for the existence of the libraries first
    if os.system("pkg-config --exists libavcodec libavutil libswscale") == 0:
        env.ParseConfig("pkg-config --cflags --libs libavcodec libavutil libswscale")
    else:
        print("Error: FFmpeg libraries not found via pkg-config.")
        print("Please install: libavcodec-dev libavutil-dev libswscale-dev")
        sys.exit(1)

# --- Sources ---
sources = Glob("src/*.cpp")

# --- Build ---
# Create the shared library
target_name: str = "bin/vtrtexture{}{}".format(
    env["suffix"], env["SHLIBSUFFIX"] # type: ignore
)

library = env.SharedLibrary(
    target=target_name,
    source=sources,
)

# --- Compilation Database ---
# Generates compile_commands.json for LSP support (clangd/VSCode)
# Note: CompilationDatabase is a method available in modern SCons environments
compile_db = env.CompilationDatabase(target="compile_commands.json") # type: ignore

Default(library, compile_db)