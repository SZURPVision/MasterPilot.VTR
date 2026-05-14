{...}: {
  perSystem = {
    pkgs,
    self',
    ...
  }: 
  let
    # 自动更新编译数据库的逻辑
    updateDB = ''
      if command -v scons >/dev/null; then
        echo "Checking/Updating VTR compile_commands.json..."
        scons compile_commands.json --silent || echo "Warning: scons failed to generate compile_commands.json"
      fi
    '';
  in
  {
    devShells = {
      # 1. 最小构建环境
      minimal = pkgs.mkShell {
        name = "vtr-minimal";
        nativeBuildInputs = with pkgs; [
          scons
          pkg-config
          patchelf
          gcc
        ];
        buildInputs = with pkgs; [
          self'.packages.ffmpeg-vtr
          self'.packages.godot-cpp
        ];
        
        GEN_GODOT_CPP_DB = "0";
        
        shellHook = ''
          echo "VTR Minimal Environment"
          
          # 优先使用本地 godot-cpp 目录
          if [ -d "./godot-cpp" ]; then
            export GODOT_CPP_PATH="./godot-cpp"
            echo "Using local godot-cpp submodule."
          else
            echo "Warning: No local godot-cpp directory found."
            echo "If you want to use the Nix-provided one, note that it's READ-ONLY."
            echo "Suggested: git submodule update --init"
            export GODOT_CPP_PATH="${self'.packages.godot-cpp}"
          fi
        '';
      };

      # 2. 默认开发环境
      default = self'.devShells.minimal.overrideAttrs (old: {
        name = "vtr-dev";
        nativeBuildInputs = old.nativeBuildInputs ++ (with pkgs; [ 
			godot_4
			clang-tools
		]);
        GEN_GODOT_CPP_DB = "1"; 
        
        shellHook = old.shellHook + ''
          echo "LSP (clangd) support enabled"
          ${updateDB}
        '';
      });

      # 3. 完整环境
      rm-mock = self'.devShells.default.overrideAttrs (old: {
        nativeBuildInputs = old.nativeBuildInputs ++ [ self'.packages.rmmock ];
        shellHook = old.shellHook + ''
          echo "RM-Mock integration enabled"
        '';
      });
    };
  };
}
