{
	"patcher" : {
		"fileversion" : 1,
		"appversion" : { "major" : 8, "minor" : 5, "revision" : 0, "architecture" : "x64", "modernui" : 1 },
		"classnamespace" : "box",
		"rect" : [ 60.0, 80.0, 740.0, 560.0 ],
		"boxes" : [
			{ "box" : { "id" : "obj-1", "maxclass" : "newobj", "text" : "udpreceive 9000", "numinlets" : 0, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 40.0, 40.0, 110.0, 22.0 ] } },
			{ "box" : { "id" : "obj-2", "maxclass" : "newobj", "text" : "route /kendama/ken/accel", "numinlets" : 1, "numoutlets" : 2, "outlettype" : [ "", "" ], "patching_rect" : [ 40.0, 80.0, 168.0, 22.0 ] } },
			{ "box" : { "id" : "obj-3", "maxclass" : "newobj", "text" : "unpack 0 0. 0. 0.", "numinlets" : 1, "numoutlets" : 4, "outlettype" : [ "int", "float", "float", "float" ], "patching_rect" : [ 40.0, 120.0, 120.0, 22.0 ] } },

			{ "box" : { "id" : "obj-4", "maxclass" : "newobj", "text" : "* 35.", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 40.0, 170.0, 45.0, 22.0 ] } },
			{ "box" : { "id" : "obj-5", "maxclass" : "newobj", "text" : "+ 500.", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 40.0, 210.0, 50.0, 22.0 ] } },
			{ "box" : { "id" : "obj-6", "maxclass" : "newobj", "text" : "cycle~", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "signal" ], "patching_rect" : [ 40.0, 300.0, 50.0, 22.0 ] } },
			{ "box" : { "id" : "obj-7", "maxclass" : "newobj", "text" : "* 0.02", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 120.0, 170.0, 50.0, 22.0 ] } },
			{ "box" : { "id" : "obj-8", "maxclass" : "newobj", "text" : "+ 0.06", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 120.0, 210.0, 50.0, 22.0 ] } },
			{ "box" : { "id" : "obj-9", "maxclass" : "newobj", "text" : "*~", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "signal" ], "patching_rect" : [ 40.0, 340.0, 40.0, 22.0 ] } },
			{ "box" : { "id" : "obj-10", "maxclass" : "newobj", "text" : "plugout~", "numinlets" : 2, "numoutlets" : 0, "patching_rect" : [ 40.0, 400.0, 70.0, 22.0 ] } },

			{ "box" : { "id" : "obj-11", "maxclass" : "newobj", "text" : "* 15.", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 300.0, 170.0, 45.0, 22.0 ] } },
			{ "box" : { "id" : "obj-12", "maxclass" : "newobj", "text" : "+ 128.", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 300.0, 210.0, 50.0, 22.0 ] } },
			{ "box" : { "id" : "obj-13", "maxclass" : "newobj", "text" : "clip 0. 255.", "numinlets" : 3, "numoutlets" : 1, "outlettype" : [ "float" ], "patching_rect" : [ 300.0, 250.0, 75.0, 22.0 ] } },
			{ "box" : { "id" : "obj-14", "maxclass" : "newobj", "text" : "* 15.", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 400.0, 170.0, 45.0, 22.0 ] } },
			{ "box" : { "id" : "obj-15", "maxclass" : "newobj", "text" : "+ 128.", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 400.0, 210.0, 50.0, 22.0 ] } },
			{ "box" : { "id" : "obj-16", "maxclass" : "newobj", "text" : "clip 0. 255.", "numinlets" : 3, "numoutlets" : 1, "outlettype" : [ "float" ], "patching_rect" : [ 400.0, 250.0, 75.0, 22.0 ] } },
			{ "box" : { "id" : "obj-17", "maxclass" : "newobj", "text" : "* 12.", "numinlets" : 2, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 500.0, 170.0, 45.0, 22.0 ] } },
			{ "box" : { "id" : "obj-18", "maxclass" : "newobj", "text" : "clip 0. 255.", "numinlets" : 3, "numoutlets" : 1, "outlettype" : [ "float" ], "patching_rect" : [ 500.0, 250.0, 75.0, 22.0 ] } },
			{ "box" : { "id" : "obj-19", "maxclass" : "newobj", "text" : "pack 0 0 0", "numinlets" : 3, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 300.0, 300.0, 90.0, 22.0 ] } },
			{ "box" : { "id" : "obj-20", "maxclass" : "newobj", "text" : "prepend /kendama/ken/led/rgb", "numinlets" : 1, "numoutlets" : 1, "outlettype" : [ "" ], "patching_rect" : [ 300.0, 340.0, 200.0, 22.0 ] } },
			{ "box" : { "id" : "obj-21", "maxclass" : "newobj", "text" : "udpsend 127.0.0.1 9001", "numinlets" : 1, "numoutlets" : 0, "patching_rect" : [ 300.0, 380.0, 160.0, 22.0 ] } },

			{ "box" : { "id" : "obj-c", "maxclass" : "comment", "text" : "Left: accel -> tone (ax pitch, ay volume). Right: accel -> RGB -> /kendama/ken/led/rgb out UDP 9001 (bridge down-path -> ken DotStar). One gesture -> sound + light. (OSC carries seq as a leading int; unpack skips it.)", "numinlets" : 1, "numoutlets" : 0, "patching_rect" : [ 40.0, 460.0, 560.0, 60.0 ] } }
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
			{ "patchline" : { "source" : [ "obj-9", 0 ], "destination" : [ "obj-10", 1 ] } },

			{ "patchline" : { "source" : [ "obj-3", 1 ], "destination" : [ "obj-11", 0 ] } },
			{ "patchline" : { "source" : [ "obj-11", 0 ], "destination" : [ "obj-12", 0 ] } },
			{ "patchline" : { "source" : [ "obj-12", 0 ], "destination" : [ "obj-13", 0 ] } },
			{ "patchline" : { "source" : [ "obj-13", 0 ], "destination" : [ "obj-19", 0 ] } },
			{ "patchline" : { "source" : [ "obj-3", 2 ], "destination" : [ "obj-14", 0 ] } },
			{ "patchline" : { "source" : [ "obj-14", 0 ], "destination" : [ "obj-15", 0 ] } },
			{ "patchline" : { "source" : [ "obj-15", 0 ], "destination" : [ "obj-16", 0 ] } },
			{ "patchline" : { "source" : [ "obj-16", 0 ], "destination" : [ "obj-19", 1 ] } },
			{ "patchline" : { "source" : [ "obj-3", 3 ], "destination" : [ "obj-17", 0 ] } },
			{ "patchline" : { "source" : [ "obj-17", 0 ], "destination" : [ "obj-18", 0 ] } },
			{ "patchline" : { "source" : [ "obj-18", 0 ], "destination" : [ "obj-19", 2 ] } },
			{ "patchline" : { "source" : [ "obj-19", 0 ], "destination" : [ "obj-20", 0 ] } },
			{ "patchline" : { "source" : [ "obj-20", 0 ], "destination" : [ "obj-21", 0 ] } }
		]
	}
}
