{...}: {
  perSystem = {
    self',
    ...
  }: {
    checks = {
      # Basic check to see if the packages build
      godot-cpp = self'.packages.godot-cpp;
      rmmock = self'.packages.rmmock;
    };
  };
}
