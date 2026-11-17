#!/usr/bin/env python3
"""Host-only Lua fault injection; requires lupa, never touches a device."""
from pathlib import Path
import re
import unittest
from lupa.lua54 import LuaRuntime

PRELUDE=r'''
files={};commands={};objects={};timers={};SCRIPT_PATH="/face/app/lua/"
files[SCRIPT_PATH.."catalog_core_v1.elf"]="core bytes"
files[SCRIPT_PATH.."catalog_ui_v4.elf"]="ui bytes"
files[SCRIPT_PATH.."card-v4.bin"]="card bytes"
files[SCRIPT_PATH.."left.bin"]="left icon"
files[SCRIPT_PATH.."right.bin"]="right icon"
files[SCRIPT_PATH.."refresh.bin"]="refresh icon"
model="miwear.watch.p67glt";version="3.201.026"
local function object(parent,attrs)
 local o={attrs={border_width=2},clickable=true,style_resets=0}
 for k,v in pairs(attrs)do o.attrs[k]=v end
 function o:remove_style_all() self.attrs={};self.style_resets=self.style_resets+1 end
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
function tick(period) for _,t in ipairs(timers)do if not t.paused and (period==nil or t.period==period) then t.cb(t) end end end
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
 if fail_ui and cmd:match("^insmod.*NFC_Tags_Catalog_UI_V4$") then return nil,"exit",1 end
 if fail_reboot and cmd=="shutdown --reboot" then return nil,"exit",1 end
 return true,"exit",0
end}
'''

class BootstrapTest(unittest.TestCase):
    def make(self,setup=""):
        lua=LuaRuntime();lua.execute(PRELUDE)
        paths=re.findall(r'"/data/tag_catalog/([a-z0-9-]+[.]bin)"',(Path(__file__).parents[1]/"launcher/catalog_assets.h").read_text())
        for name in paths:lua.globals().files["/face/app/lua/"+name]="artwork "+name
        lua.execute(setup)
        lua.execute(Path(__file__).with_name("main.lua").read_text())
        self.assertEqual(len(lua.globals().commands),0,"Face render must not execute shell commands")
        root=lua.globals().objects[1]
        self.assertEqual(root.style_resets,1)
        self.assertIsNone(root.attrs.border_width,"The theme border must be removed")
        self.assertEqual((root.attrs.w,root.attrs.h),(84,84),"Size must be set after removing styles")
        return lua
    def tap(self,lua):lua.globals().objects[1].callback()
    def timer(self,lua,period):
        return lua.eval('function(p) for _,t in ipairs(timers) do if t.period==p then return t end end end')(period)
    def insmods(self,lua):return [c for c in lua.globals().commands.values() if c.startswith("insmod ")]
    def reboots(self,lua):return [c for c in lua.globals().commands.values() if c=="shutdown --reboot"]
    def test_cold_manual_load(self):
        lua=self.make();self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertTrue(self.insmods(lua)[0].endswith("Tag_Catalog_V1"))
        self.assertIsNone(lua.globals().files["/tmp/nfc_tags_ui_open_request"])
        self.assertEqual(lua.globals().files["/data/tag_catalog/core-catalog.elf"],"core bytes")
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog-v4.elf"],"ui bytes")
        self.assertEqual(lua.globals().files["/data/tag_catalog/right-off-v4.bin"],"artwork right-off-v4.bin")
        self.assertTrue(self.insmods(lua)[1].endswith("NFC_Tags_Catalog_UI_V4"))
        self.assertTrue(lua.globals().objects[1].clickable,"Loaded is not registered")
        self.assertFalse(lua.globals().objects[1].bubble)
        lua.globals().files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v4\n"
        lua.globals().tick()
        self.assertTrue(lua.globals().objects[1].clickable,"Reboot is requested from the same touch")
        self.assertEqual(self.reboots(lua),["shutdown --reboot"])
        self.assertEqual(lua.globals().files["/data/tag_catalog/auto_load"],"catalog_ui_v4\n")
        self.assertTrue(self.timer(lua,100).paused)
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
        self.assertIsNone(lua.globals().objects[1].attrs.border_width)
    def test_warm_passes_to_native(self):
        lua=self.make(r'files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v4\n"')
        self.assertFalse(lua.globals().objects[1].clickable)
        self.assertIsNone(lua.globals().objects[1].callback)
        self.assertEqual(len(lua.globals().timers),0)
    def test_loaded_without_ready_is_not_a_dead_shortcut(self):
        lua=self.make(r'files["/tmp/nfc_tags_ui_loaded"]="catalog_ui_v4\n"')
        self.assertTrue(lua.globals().objects[1].clickable)
        for _ in range(30):lua.globals().tick()
        self.assertTrue(self.timer(lua,100).paused)
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
        self.assertIsNone(lua.globals().objects[2].attrs.width)
        self.assertEqual(lua.globals().objects[2].attrs.align,0)
        self.assertEqual((lua.globals().objects[1].attrs.w,lua.globals().objects[1].attrs.h),(84,84))
        self.assertEqual(lua.globals().objects[1].attrs.radius,42)
        self.assertIsNone(lua.globals().objects[1].attrs.border_width)
        self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertTrue(lua.globals().objects[1].clickable)
    def test_late_ready_recovers_before_short_click(self):
        lua=self.make(r'files["/tmp/nfc_tags_ui_loaded"]="catalog_ui_v4\n"')
        for _ in range(30):lua.globals().tick()
        lua.globals().files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v4\n"
        lua.globals().objects[1].pressed()
        self.assertFalse(lua.globals().objects[1].clickable)
        self.assertTrue(lua.globals().objects[1].bubble)
        self.assertEqual(self.insmods(lua),[])
    def test_failed_core_still_loads_ui(self):
        lua=self.make("fail_core=true");self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertIsNone(lua.globals().files["/tmp/tag_catalog_v1_loaded"])
        self.assertEqual(lua.globals().files["/tmp/Tag_Catalog_V1_attempted"],"1\n")
        self.assertEqual(lua.globals().files["/tmp/nfc_tags_ui_loaded"],"catalog_ui_v4\n")
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
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog-v4.elf"],"ui bytes")
        self.assertIsNone(lua.globals().files["/data/tag_catalog/.bootstrap_probe"])
    def test_unwritable_directory_stops_before_modules(self):
        lua=self.make('fail_mkdir=true;fail_open="/data/tag_catalog/.bootstrap_probe"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"DIR")
    def test_missing_ui_source_is_distinct(self):
        lua=self.make('files[SCRIPT_PATH.."catalog_ui_v4.elf"]=nil');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"UI SRC")
        self.assertIn("catalog_ui_v4.elf",lua.globals().files["/tmp/tag_bootstrap_diagnostic"])
    def test_ui_target_failure_is_distinct(self):
        lua=self.make('fail_open="/data/tag_catalog/ui-catalog-v4.elf"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"UI DST")
    def test_reboot_failure_stays_visible_and_clickable(self):
        lua=self.make("fail_reboot=true");self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        lua.globals().files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v4\n"
        lua.globals().tick(100)
        self.assertEqual(lua.globals().objects[2].attrs.text,"NO REBOOT")
        self.assertTrue(lua.globals().objects[1].clickable)
    def test_v1_loaded_ui_requires_reboot(self):
        lua=self.make(r'files["/tmp/nfc_tags_ui_loaded"]="catalog_ui_v1\n"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertTrue(lua.globals().objects[1].clickable)
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
    def test_v1_partial_attempt_without_markers_requires_reboot(self):
        lua=self.make(r'files["/tmp/NFC_Tags_Catalog_UI_V1_attempted"]="1\n"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
        self.assertIsNone(lua.globals().files["/tmp/NFC_Tags_Catalog_UI_V4_attempted"])
    def test_v1_ready_without_loaded_marker_cannot_pass_through(self):
        lua=self.make(r'files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v1\n"')
        lua.globals().objects[1].pressed();self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertTrue(lua.globals().objects[1].clickable)
        self.assertFalse(lua.globals().objects[1].bubble)
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
    def test_v2_loaded_ui_requires_reboot(self):
        lua=self.make(r'files["/tmp/nfc_tags_ui_loaded"]="catalog_ui_v2\n"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertTrue(lua.globals().objects[1].clickable)
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
    def test_v2_partial_attempt_without_markers_requires_reboot(self):
        lua=self.make(r'files["/tmp/NFC_Tags_Catalog_UI_V2_attempted"]="1\n"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
        self.assertIsNone(lua.globals().files["/tmp/NFC_Tags_Catalog_UI_V4_attempted"])
    def test_v2_ready_without_loaded_marker_cannot_pass_through(self):
        lua=self.make(r'files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v2\n"')
        lua.globals().objects[1].pressed();self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertTrue(lua.globals().objects[1].clickable)
        self.assertFalse(lua.globals().objects[1].bubble)
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
    def test_v4_keeps_persisted_older_files_unchanged(self):
        lua=self.make('files["/data/tag_catalog/ui-catalog.elf"]="v1 bytes";files["/data/tag_catalog/ui-catalog-v2.elf"]="v2 bytes";files["/data/tag_catalog/ui-catalog-v3.elf"]="v3 bytes"');self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog.elf"],"v1 bytes")
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog-v2.elf"],"v2 bytes")
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog-v4.elf"],"ui bytes")
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog-v3.elf"],"v3 bytes")

    def test_v3_resident_or_partial_requires_reboot(self):
        for state in [r'files["/tmp/nfc_tags_ui_loaded"]="catalog_ui_v3\n"',r'files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v3\n"',r'files["/tmp/NFC_Tags_Catalog_UI_V3_attempted"]="1\n"']:
            lua=self.make(state);self.tap(lua)
            self.assertEqual(self.insmods(lua),[])
            self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")

    def test_boot_timer_loads_entries_without_opening(self):
        lua=self.make(r'files["/data/tag_catalog/auto_load"]="catalog_ui_v4\n"')
        self.assertEqual(len(lua.globals().timers),1)
        lua.globals().tick(2000)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertIsNone(lua.globals().files["/tmp/nfc_tags_ui_open_request"])
        self.assertEqual(lua.globals().files["/tmp/nfc_tags_ui_loaded"],"catalog_ui_v4\n")
        self.assertFalse(lua.globals().objects[1].clickable,"Overlay is transparent from the first frame")
        lua.globals().files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v4\n"
        lua.globals().tick(100)
        self.assertFalse(lua.globals().objects[1].clickable)
        self.assertTrue(lua.globals().objects[1].bubble)
        self.assertEqual(lua.globals().objects[1].attrs.bg_opa,0)

    def test_boot_timer_failure_stays_silent_for_manual_retry(self):
        lua=self.make(r'files["/data/tag_catalog/auto_load"]="catalog_ui_v4\n";fail_ui=true')
        lua.globals().tick(2000)
        self.assertEqual(len(lua.globals().objects),1,"No visible status at boot")
        self.assertEqual(lua.globals().files["/tmp/tag_bootstrap_auto"],
                         "REBOOT\n\n")
        self.assertFalse(lua.globals().objects[1].clickable,"No retry overlay after confirmation")
        before=len(self.insmods(lua))
        self.assertIsNone(lua.globals().objects[1].callback)
        self.assertEqual(len(self.insmods(lua)),before,"No manual retry path")

    def test_boot_timer_skips_partial_state(self):
        lua=self.make(r'files["/data/tag_catalog/auto_load"]="catalog_ui_v4\n";files["/tmp/nfc_tags_ui_loaded"]="catalog_ui_v4\n"')
        lua.globals().tick(2000)
        self.assertEqual(self.insmods(lua),[])
        self.assertFalse(lua.globals().objects[1].clickable)
        for _ in range(30):lua.globals().tick(100)
        self.assertEqual(len(lua.globals().objects),1,"Partial state stays silent")

    def test_boot_timer_does_not_run_for_ready_face(self):
        lua=self.make(r'files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v4\n"')
        self.assertEqual(len(lua.globals().timers),0)

    def test_first_boot_has_no_auto_timer_and_records_confirmation(self):
        lua=self.make()
        self.assertEqual(len(lua.globals().timers),0,"No auto-load before one confirmed install")
        self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        lua.globals().files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v4\n"
        lua.globals().tick(100)
        self.assertEqual(lua.globals().files["/data/tag_catalog/auto_load"],"catalog_ui_v4\n")
        self.assertEqual(self.reboots(lua),["shutdown --reboot"])

    def test_card_asset_required(self):
        lua=self.make('files[SCRIPT_PATH.."card-v4.bin"]=nil');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"UI ART")

    def test_all_artwork_required_before_any_insmod(self):
        paths=re.findall(r'"/data/tag_catalog/([a-z0-9-]+[.]bin)"',(Path(__file__).parents[1]/"launcher/catalog_assets.h").read_text())
        self.assertEqual(len(paths),12)
        for name in paths:
            for target in ["/face/app/lua/", "/data/tag_catalog/"]:
                lua=self.make('fail_open="'+target+name+'"');self.tap(lua)
                self.assertEqual(self.insmods(lua),[])
                self.assertEqual(lua.globals().objects[2].attrs.text,"UI ART")
                self.assertIn(name,lua.globals().files["/tmp/tag_bootstrap_diagnostic"])

if __name__=="__main__":unittest.main()
