extends TextureRect

var vtr: VTRTexture = VTRTexture.new()

func _ready():
	vtr.port = 3334
	vtr.active = true
	texture = vtr
	
	var mat = ShaderMaterial.new()
	mat.shader = load("res://yuv_nv12.gdshader")
	material = mat
