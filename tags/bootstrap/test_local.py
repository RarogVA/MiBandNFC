#!/usr/bin/env python3
"""Host-only Lua checks for the local NDEF bootstrap; never touches a device."""
from pathlib import Path
import importlib.util
import re
import unittest
from lupa.lua54 import LuaRuntime

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("bootstrap_test_base",Path(__file__).with_name("test.py"))
base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)

PRELUDE=(base.PRELUDE
 .replace("catalog_core_v1.elf","catalog_core_v2_local.elf")
 .replace("catalog_ui_v4.elf","catalog_ui_v5.elf")
 .replace("Tag_Catalog_V1$","Tag_Catalog_Local_V2$")
 .replace("NFC_Tags_Catalog_UI_V4$","NFC_Tags_Catalog_UI_V5$"))

class LocalBootstrapTest(unittest.TestCase):
    def make(self,setup=""):
        lua=LuaRuntime();lua.execute(PRELUDE)
        paths=re.findall(r'"/data/tag_catalog/([a-z0-9-]+[.]bin)"',(ROOT/"tags/launcher/catalog_assets.h").read_text())
        for name in paths:lua.globals().files["/face/app/lua/"+name]="artwork "+name
        lua.execute(setup)
        lua.execute((Path(__file__).with_name("main_local.lua")).read_text())
        self.assertEqual(len(lua.globals().commands),0,"Face render must not execute shell commands")
        return lua
    def tap(self,lua):lua.globals().objects[1].callback()
    def insmods(self,lua):return [c for c in lua.globals().commands.values() if c.startswith("insmod ")]
    def test_cold_install(self):
        lua=self.make();self.tap(lua)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertTrue(self.insmods(lua)[0].endswith("Tag_Catalog_Local_V2"))
        self.assertTrue(self.insmods(lua)[1].endswith("NFC_Tags_Catalog_UI_V5"))
        self.assertEqual(lua.globals().files["/data/tag_catalog/core-catalog-local.elf"],"core bytes")
        self.assertEqual(lua.globals().files["/data/tag_catalog/ui-catalog-v5.elf"],"ui bytes")
        lua.globals().files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v5\n"
        lua.globals().tick()
        self.assertEqual([c for c in lua.globals().commands.values() if c=="shutdown --reboot"],["shutdown --reboot"])
        self.assertEqual(lua.globals().files["/data/tag_catalog/auto_load"],"catalog_ui_v5\n")
    def test_warm_pass_through(self):
        lua=self.make(r'files["/tmp/nfc_tags_catalog_ui_ready"]="catalog_ui_v5\n"')
        self.assertFalse(lua.globals().objects[1].clickable)
        self.assertIsNone(lua.globals().objects[1].callback)
        self.assertEqual(len(lua.globals().timers),0)
    def test_old_core_requires_reboot(self):
        lua=self.make(r'files["/tmp/tag_catalog_v1_loaded"]="catalog_v1\n"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
    def test_wrong_firmware(self):
        lua=self.make('version="3.201.999"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"VERSION")
    def test_missing_ui_source(self):
        lua=self.make(r'files[SCRIPT_PATH.."catalog_ui_v5.elf"]=nil');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"UI SRC")
    def test_v5_attempt_blocks_reload(self):
        lua=self.make(r'files["/tmp/NFC_Tags_Catalog_UI_V5_attempted"]="1\n"');self.tap(lua)
        self.assertEqual(self.insmods(lua),[])
        self.assertEqual(lua.globals().objects[2].attrs.text,"REBOOT")
    def test_auto_path_after_reboot(self):
        lua=self.make(r'files["/data/tag_catalog/auto_load"]="catalog_ui_v5\n"')
        lua.globals().tick(2000)
        self.assertEqual(len(self.insmods(lua)),2)
        self.assertIsNone(lua.globals().files["/tmp/nfc_tags_ui_open_request"])

if __name__=="__main__":unittest.main()
