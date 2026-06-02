{
	"patcher" : {
		"fileversion" : 1,
		"appversion" : { "major" : 8, "minor" : 5, "revision" : 0, "architecture" : "x64", "modernui" : 1 },
		"classnamespace" : "box",
		"rect" : [ 80.0, 100.0, 640.0, 480.0 ],
		"boxes" : [
			{ "box" : { "id" : "obj-1", "maxclass" : "newobj", "text" : "udpreceive 9000", "numinlets" : 0, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 40.0, 40.0, 110.0, 22.0 ] } },
			{ "box" : { "id" : "obj-2", "maxclass" : "newobj", "text" : "route /kendama/ken/accel", "numinlets" : 1, "numoutlets" : 2, "outlettype" : [ "", "" ], "patching_rect" : [ 40.0, 80.0, 168.0, 22.0 ] } },
			{ "box" : { "id" : "obj-3", "maxclass" : "newobj", "text" : "unpack 0 0. 0. 0.", "numinlets" : 1, "numoutlets" : 4, "outlettype" : [ "int", "float", "float", "float" ], "patching_rect" : [ 40.0, 120.0, 120.0, 22.0 ] } },
			{ "box" : { "id" : "obj-4", "maxclass" : "newobj", "text" : "* 35.", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 60.0, 160.0, 45.0, 22.0 ] } },
			{ "box" : { "id" : "obj-5", "maxclass" : "newobj", "text" : "+ 500.", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 60.0, 200.0, 50.0, 22.0 ] } },
			{ "box" : { "id" : "obj-6", "maxclass" : "newobj", "text" : "cycle~", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "signal" ], "patching_rect" : [ 60.0, 300.0, 50.0, 22.0 ] } },
			{ "box" : { "id" : "obj-7", "maxclass" : "newobj", "text" : "* 0.02", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 180.0, 160.0, 50.0, 22.0 ] } },
			{ "box" : { "id" : "obj-8", "maxclass" : "newobj", "text" : "+ 0.06", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 180.0, 200.0, 50.0, 22.0 ] } },
			{ "box" : { "id" : "obj-9", "maxclass" : "newobj", "text" : "*~", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "signal" ], "patching_rect" : [ 60.0, 340.0, 40.0, 22.0 ] } },
			{ "box" : { "id" : "obj-10", "maxclass" : "newobj", "text" : "plugout~", "numinlets" : 2, "numoutlets" : 0, "patching_rect" : [ 60.0, 400.0, 70.0, 22.0 ] } },
			{ "box" : { "id" : "obj-c", "maxclass" : "comment", "text" : "Kendama accel -> tone.  ax tilt -> pitch (150-850 Hz),  ay tilt -> volume.  Receives /kendama/ken/accel (,ifff: seq is skipped) on UDP 9000 from the bridge.", "numinlets" : 1, "numoutlets" : 0, "patching_rect" : [ 250.0, 40.0, 360.0, 60.0 ] } }
		],
		"lines" : [
			{ "patchline" : { "source" : [ "obj-1", 0 ], "destination" : [ "obj-2", 0 ] } },
			{ "patchline" : { "source" : [ "obj-2", 0 ], "destination" : [ "obj-3", 0 ] } },
			{ "patchline" : { "source" : [ "obj-3", 1 ], "destination" : [ "obj-4", 0 ] } },
			{ "patchline" : { "source" : [ "obj-4", 0 ], "destination" : [ "obj-5", 0 ] } },
			{ "patchline" : { "source" : [ "obj-5", 0 ], "destination" : [ "obj-6", 0 ] } },
			{ "patchline" : { "source" : [ "obj-3", 2 ], "destination" : [ "obj-7", 0 ] } },
			{ "patchline" : { "source" : [ "obj-7", 0 ], "destination" : [ "obj-8", 0 ] } },
			{ "patchline" : { "source" : [ "obj-6", 0 ], "destination" : [ "obj-9", 0 ] } },
			{ "patchline" : { "source" : [ "obj-8", 0 ], "destination" : [ "obj-9", 1 ] } },
			{ "patchline" : { "source" : [ "obj-9", 0 ], "destination" : [ "obj-10", 0 ] } },
			{ "patchline" : { "source" : [ "obj-9", 0 ], "destination" : [ "obj-10", 1 ] } }
		]
	}
}
