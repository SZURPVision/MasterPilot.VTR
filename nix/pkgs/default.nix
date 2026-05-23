{ self, lib, ... }: {
  perSystem = {
    pkgs,
    system,
    inputs',
    self',
    ...
  }: let
    # 1. Centralized Dependency Selection
    ffmpeg-vtr = pkgs.ffmpeg_7; 
    
    godot-cpp = pkgs.callPackage ./godot-cpp {
      src = self.inputs.godot-cpp;
    };

    # 2. Source Filter Logic
    vtrSrc = lib.cleanSourceWith {
      src = ../../.;
      filter = path: type: 
        let
          base = baseNameOf (toString path);
          relPath = lib.removePrefix (toString ../../.) (toString path);
          inFilteredDir = lib.hasPrefix "/src" relPath || lib.hasPrefix "/addons" relPath;
        in
          (type == "directory" && (base == "src" || base == "addons" || inFilteredDir)) ||
          (inFilteredDir && (
            lib.hasSuffix ".cpp" base || 
            lib.hasSuffix ".h" base || 
            lib.hasSuffix ".hpp" base ||
            lib.hasSuffix ".gdextension" base ||
            lib.hasSuffix ".uid" base
          )) ||
          (base == "SConstruct");
    };

    # 3. Build Factory
    mkVtr = args: pkgs.callPackage ./vtr ({
      inherit godot-cpp;
      src = vtrSrc;
      ffmpeg = ffmpeg-vtr;
    } // args);

  in {
    packages = {
      inherit godot-cpp ffmpeg-vtr;

      # Combined build (Debug + Release)
      vtr = mkVtr { withDebug = true; withRelease = true; devBuild = true; };
      default = self'.packages.vtr;

      # Individual variants
      release = mkVtr { withDebug = false; withRelease = true; };
      debug = mkVtr { withDebug = true; withRelease = false; devBuild = true; };
    };
  };
}
