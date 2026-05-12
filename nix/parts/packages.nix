{inputs, ...}: {
  perSystem = {
    pkgs,
    system,
    ...
  }: {
    packages = {
      # 当前使用的 FFmpeg 版本，方便在 7 (Debian 13 兼容) 和最新版之间切换
      ffmpeg-vtr = pkgs.ffmpeg_7; # 回退到最新版只需改为 pkgs.ffmpeg

      rmmock = inputs.rmmock.packages.${system}.default;
      godot-cpp = pkgs.callPackage ../pkgs/godot-cpp {
        src = inputs.godot-cpp;
      };
    };
  };
}
