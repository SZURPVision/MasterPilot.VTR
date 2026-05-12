{ ... }: {
  perSystem = { pkgs, self', ... }: {
    packages = pkgs.lib.optionalAttrs pkgs.stdenv.isLinux {
      vtr-fhs = pkgs.buildFHSEnv {
        name = "vtr-fhs";
        targetPkgs = pkgs: with pkgs; [
          scons
          pkg-config
          self'.packages.ffmpeg-vtr
          gcc
          patchelf
          python3
          glibc.dev
          zlib
          alsa-lib
        ];
        runScript = "bash";
      };
    };
  };
}
