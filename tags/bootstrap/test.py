#!/usr/bin/env python3
"""Host-only Lua fault injection; requires lupa, never touches a device."""
from pathlib import Path
import unittest
from lupa.lua54 import LuaRuntime

PRELUDE=r'''
files={};commands={};objects={};timers={};SCRIPT_PATH="/face/app/lua/"
files[SCRIPT_PATH.."catalog_core_v1.elf"]="core bytes"
files[SCRIPT_PATH.."catalog_ui_v2.elf"]="ui bytes"
files[SCRIPT_PATH.."left.bin"]="left icon"
files[SCRIPT_PATH.."right.bin"]="right icon"
files[SCRIPT_PATH.."refresh.bin"]="refresh icon"
model="miwear.watch.p67glt";version="3.201.026"
local function object(parent,attrs)
 local o={attrs=attrs,clickable=true}
 function o:clear_flag(flag) if flag==2 then self.clickable=false elseif flag==0x4000 then self.bubble=false end end
 function o:add_flag(flag) if flag==2 then self.clickable=true elseif flag==0x4000 then self.bubble=true end end
 function o:set(attrs) for k,v in pairs(attrs)do self.attrs[k]=v end end
 function o:onClicked(fn) self.callback=fn end
 function o:onPressed(fn) self.pressed=fn end
 table.insert(objects,o);return o
end
local function timer(attrs)
 function attrs:pause() self.paused=true end
 function attrs:resume() self.paused=false end
 table.insert(timers,attrs);return attrs
end
function tick() for _,t in ipairs(timers)do if not t.paused then t.cb(t) end end end
package.preload.lvgl=function() return {Object=object,Label=object,Timer=timer,OPA=function(x)return x end,
 FLAG={SCROLLABLE=16,CLICKABLE=2,EVENT_BUBBLE=0x4000},Font=function()return 1 end,ALIGN={CENTER=0}} end
io={open=function(path,mode)
 if path==fail_open then return nil end
 if mode=="rb" and files[path]==nil then return nil end
 local pos=1;local buffer=mode=="wb" and "" or files[path]
 return {read=function(_,n) if pos>#buffer then return nil end local v=buffer:sub(pos,pos+n-1);pos=pos+#v;return v end,
 write=function(_,s)buffer=buffer..s;return true end,
 close=function()if mode=="wb" then files[path]=buffer end;return true end}
end}
os={remove=function(path)files[path]=nil;return true end,execute=function(cmd)
 table.insert(commands,cmd)
 if cmd:match("^getprop product.model") then files["/tmp/tag_bootstrap_model"]=model.."\n" end
 if cmd:match("^getprop ro.build.version") then files["/tmp/tag_bootstrap_version"]=version.."\n" end
 if fail_mkdir and cmd:match("^mkdir ") then return nil,"exit",1 end
 if fail_core and cmd:match("^insmod.*Tag_Catalog_V1$") then return nil,"exit",1 end
 if fail_ui and cmd:match("^insmod.*NFC_Tags_Catalog_UI_V2$") then return nil,"exit",1 end
 return true,"exit",0
end}
'''

class BootstrapTest(unittest.TestCase):
    def make(self,setup=""):
        lua=LuaRuntime();lua.execute(PRELUDE);lua.execute(setup)
        lua.execute(Path(__file__).with_name("main.lua").read_text())
        self.assertEqual(len(lua.globals().commands),0,"Face render must not execute shell commands")
        return lua
    def tap(self,lua):lua.globals().objects[1].callback()
    def insmods(self,lua):return [c for c in lua.globals().commands.values() if c.startswith("insmod ")]
    def test_cold_manual_load(self):
        lua=self.make();self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertTrue(self.insmods(lua)[0].endswith("Tag_Catalog_V1"))
        self.assertEqual(lua.globals().files["/tmp/nfc_tags_ui_open_request"],"catalog_ui_v2\n")
        self.assertEqual(lua.globals().files["/data/tag_catalog/core-catalog.elf"],"core bytes")
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog-v2.elf"],"ui bytes")
        self.assertEqual(lua.globals().files["/data/tag_catalog/refresh.bin"],"refresh icon")
        self.assertTrue(self.insmods(lua)[1].endswith("NFC_Tags_Catalog_UI_V2"))
        self.assertTrue(lua.globals().objects[1].clickable,"Loaded is not registered")
        self.assertFalse(lua.globals().objects[1].bubble)
        lua.globals().files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v2\n"
        lua.globals().tick()
        self.assertFalse(lua.globals().objects[1].clickable)
        self.assertTrue(lua.globals().timers[1].paused)
    def test_warm_passes_to_native(self):
        lua=self.make(r'files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v2\n"')
        self.assertFalse(lua.globals().objects[1].clickable)
        self.assertIsNone(lua.globals().objects[1].callback)
        self.assertEqual(len(lua.globals().timers),0)
    def test_loaded_without_ready_is_not_a_dead_shortcut(self):
        lua=self.make(r'files["/tmp/nfc_tags_ui_loaded"]="catalog_ui_v2\n"')
        self.assertTrue(lua.globals().objects[1].clickable)
        for _ in range(30):lua.globals().tick()
        self.assertTrue(lua.globals().timers[1].paused)
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
        self.assertIsNone(lua.globals().objects[2].attrs.width)
        self.assertEqual(lua.globals().objects[2].attrs.align,0)
        self.assertEqual((lua.globals().objects[1].attrs.w,lua.globals().objects[1].attrs.h),(84,84))
        self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertTrue(lua.globals().objects[1].clickable)
    def test_late_ready_recovers_before_short_click(self):
        lua=self.make(r'files["/tmp/nfc_tags_ui_loaded"]="catalog_ui_v2\n"')
        for _ in range(30):lua.globals().tick()
        lua.globals().files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v2\n"
        lua.globals().objects[1].pressed()
        self.assertFalse(lua.globals().objects[1].clickable)
        self.assertTrue(lua.globals().objects[1].bubble)
        self.assertEqual(self.insmods(lua),[])
    def test_failed_core_still_loads_ui(self):
        lua=self.make("fail_core=true");self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertIsNone(lua.globals().files["/tmp/tag_catalog_v1_loaded"])
        self.assertEqual(lua.globals().files["/tmp/Tag_Catalog_V1_attempted"],"1\n")
        self.assertEqual(lua.globals().files["/tmp/nfc_tags_ui_loaded"],"catalog_ui_v2\n")
    def test_old_core_requires_reboot(self):
        lua=self.make('files["/tmp/tag_catalog_loaded"]="tag_catalog"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
    def test_partial_ui_never_reloads(self):
        lua=self.make("fail_ui=true");self.tap(lua);self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertIsNone(lua.globals().files["/tmp/nfc_tags_ui_loaded"])
    def test_wrong_firmware_cannot_load(self):
        lua=self.make('version="3.201.999"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"VERSION")
    def test_existing_writable_directory_ignores_mkdir_eexist(self):
        lua=self.make("fail_mkdir=true");self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog-v2.elf"],"ui bytes")
        self.assertIsNone(lua.globals().files["/data/tag_catalog/.bootstrap_probe"])
    def test_unwritable_directory_stops_before_modules(self):
        lua=self.make('fail_mkdir=true;fail_open="/data/tag_catalog/.bootstrap_probe"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"DIR")
    def test_missing_ui_source_is_distinct(self):
        lua=self.make('files[SCRIPT_PATH.."catalog_ui_v2.elf"]=nil');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"UI SRC")
        self.assertIn("catalog_ui_v2.elf",lua.globals().files["/tmp/tag_bootstrap_diagnostic"])
    def test_ui_target_failure_is_distinct(self):
        lua=self.make('fail_open="/data/tag_catalog/ui-catalog-v2.elf"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"UI DST")
    def test_ui_request_failure_is_distinct(self):
        lua=self.make('fail_open="/tmp/nfc_tags_ui_open_request"');self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),1)
        self.assertEqual(lua.globals().objects[2].attrs.text,"UI REQ")
    def test_v1_loaded_ui_requires_reboot(self):
        lua=self.make(r'files["/tmp/nfc_tags_ui_loaded"]="catalog_ui_v1\n"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertTrue(lua.globals().objects[1].clickable)
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
    def test_v1_partial_attempt_without_markers_requires_reboot(self):
        lua=self.make(r'files["/tmp/NFC_Tags_Catalog_UI_V1_attempted"]="1\n"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
        self.assertIsNone(lua.globals().files["/tmp/NFC_Tags_Catalog_UI_V2_attempted"])
    def test_v1_ready_without_loaded_marker_cannot_pass_through(self):
        lua=self.make(r'files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v1\n"')
        lua.globals().objects[1].pressed();self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertTrue(lua.globals().objects[1].clickable)
        self.assertFalse(lua.globals().objects[1].bubble)
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
    def test_v2_keeps_persisted_v1_file_unchanged(self):
        lua=self.make('files["/data/tag_catalog/ui-catalog.elf"]="old ui bytes"');self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog.elf"],"old ui bytes")
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog-v2.elf"],"ui bytes")

if __name__=="__main__":unittest.main()
