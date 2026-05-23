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
	  export GODOT_CPP_PATH="${self'.packages.godot-cpp}"
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
    };
  };
}
