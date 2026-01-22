extends TextureRect

var vtr:VTRTexture = VTRTexture.new()

func _ready():
	vtr.port = 3334
	vtr.active = true
	texture = vtr
