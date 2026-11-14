-- Embedded as the second child of the existing 84x84 lower-circle Widget.
-- Evaluating this file never loads a module or starts NFC. Only a manual tap
-- can copy/load the modules. The ordinary Widget remains the app launcher.
local lvgl=require("lvgl")
local UI_VERSION="catalog_ui_v4\n"
local CORE_VERSION="catalog_v1\n"
local function read(path,n)
 local f=io.open(path,"rb");if not f then return nil end
 local value=f:read(n or 128);f:close();return value
end
local function mark(path,value)
 local f=io.open(path,"wb");if not f then return false end
 local ok=f:write(value);local closed=f:close();return ok and closed
end
local function execute(command)
 local a,_,code=os.execute(command)
 return a==true or a==0 or code==0
end
local function prop(key,path)
 if not execute("getprop "..key.." > "..path) then return "" end
 return (read(path) or ""):match("^%s*(.-)%s*$")
end
local function copy(resource,target)
 local src=io.open(SCRIPT_PATH..resource,"rb")
 if not src then return false,"SRC",SCRIPT_PATH..resource end
 local dst=io.open(target,"wb")
 if not dst then src:close();return false,"DST",target end
 local ok=true
 while true do
  local chunk=src:read(4096);if not chunk then break end
  if not dst:write(chunk) then ok=false;break end
 end
 src:close();local closed=dst:close()
 if not ok then return false,"WRITE",target end
 if not closed then return false,"CLOSE",target end
 return true
end
local function load(target,name)
 local attempted="/tmp/"..name.."_attempted"
 if read(attempted) then return false end
 if not mark(attempted,"1\n") then return false end
 return execute("insmod "..target.." "..name)
end
local root=lvgl.Object(nil,{})
-- The theme's default Object style has a visible border. Remove it before
-- setting our dimensions: remove_style_all also removes local size styles.
root:remove_style_all()
root:set{w=84,h=84,bg_opa=0}
root:clear_flag(lvgl.FLAG.SCROLLABLE)
root:clear_flag(lvgl.FLAG.EVENT_BUBBLE)
local status=nil
local function show(text)
 if not status then
  root:set{bg_color=0x141414,bg_opa=lvgl.OPA(100),radius=42}
  -- Content width, like the original Recorder INSTALL label: centering an
  -- 80px-wide object leaves its short text left-aligned within that object.
  status=lvgl.Label(root,{text=text,text_color=0xFFFFFF,
   text_font=lvgl.Font("MiSans-Demibold",14,"normal"),align=lvgl.ALIGN.CENTER})
 else status:set{text=text} end
end
local function failure(code,detail)
 mark("/tmp/tag_bootstrap_diagnostic",code.."\n"..(detail or "").."\n")
 show(code)
end
local busy=false
local ready_timer=nil
local waiting=false
local ready_ticks=0
local function ready()
 return read("/tmp/nfc_tags_catalog_ui_ready")==UI_VERSION
end
local function pass_through()
 if ready_timer then ready_timer:pause() end
 waiting=false
 if status then status:set{text=""} end
 root:set{bg_opa=0}
 root:add_flag(lvgl.FLAG.EVENT_BUBBLE)
 root:clear_flag(lvgl.FLAG.CLICKABLE)
end
local function watch_ready()
 -- Same Timer API used by the original recorder.face installer. Bound the
 -- registration wait; an insmod success must never be mistaken for readiness.
 ready_ticks=30
 if not ready_timer then
  ready_timer=lvgl.Timer{period=100,paused=true,cb=function(t)
   if ready() then pass_through();return end
   ready_ticks=ready_ticks-1
   if ready_ticks<=0 then t:pause();waiting=false;show("REBOOT") end
  end}
 end
 waiting=true;ready_timer:resume()
end
if ready() then
 -- The native Widget handles ordinary taps after initialization, including
 -- after leaving this face and coming back. No polling timer is needed.
 pass_through()
else
 root:add_flag(lvgl.FLAG.CLICKABLE)
 -- onPressed is firmware event1; the Widget launch is SHORT_CLICKED event4.
 -- This also recovers a late READY after our bounded wait expired, before the
 -- same touch's release/short-click travels to the native parent.
 root:onPressed(function()if ready() then pass_through() end end)
 root:onClicked(function()
  if busy then return end
  if ready() then pass_through();return end
  busy=true
  local model=prop("product.model","/tmp/tag_bootstrap_model")
  local version=prop("ro.build.version","/tmp/tag_bootstrap_version"):gsub("_",".")
  if model~="miwear.watch.p67glt" or version~="3.201.026" then
   show("VERSION");busy=false;return
  end
  local ui=read("/tmp/nfc_tags_ui_loaded")
  local resident_ready=read("/tmp/nfc_tags_catalog_ui_ready")
  local core=read("/tmp/tag_catalog_v1_loaded")
  if (ui and ui~=UI_VERSION) or (resident_ready and resident_ready~=UI_VERSION) or
     (core and core~=CORE_VERSION) or
     ((read("/tmp/apdu_lab_loaded") or read("/tmp/tag_catalog_loaded")) and core~=CORE_VERSION) then
   show("REBOOT");busy=false;return
  end
  if ui==UI_VERSION then
   show(waiting and "WAIT" or "REBOOT");busy=false;return
  end
  if read("/tmp/NFC_Tags_Catalog_UI_V1_attempted") or
     read("/tmp/NFC_Tags_Catalog_UI_V2_attempted") or
     read("/tmp/NFC_Tags_Catalog_UI_V3_attempted") or
     read("/tmp/NFC_Tags_Catalog_UI_V4_attempted") then
   show("REBOOT");busy=false;return
  end
  show("LOAD")
  -- Exact .026 NSH mkdir -p returns -1/EEXIST for an existing final directory.
  -- Verify actual writability instead of treating that return as a failure.
  execute("mkdir -p /data/tag_catalog")
  local probe="/data/tag_catalog/.bootstrap_probe"
  local probe_value="nfc_catalog_bootstrap\n"
  if not mark(probe,probe_value) or read(probe)~=probe_value then
   failure("DIR",probe);busy=false;return
  end
  os.remove(probe)
  -- Preserve both ELFs and all audited UI4 artwork before attempting insmod.
  local core_copied=copy("catalog_core_v1.elf","/data/tag_catalog/core-catalog.elf")
  local ui_copied,ui_error,ui_path=copy("catalog_ui_v4.elf","/data/tag_catalog/ui-catalog-v4.elf")
  if not ui_copied then failure("UI "..ui_error,ui_path);busy=false;return end
  local artwork={"card-v4.bin","controls-v4.bin","status-ok-v4.bin","status-fail-v4.bin",
   "status-busy0-v4.bin","status-busy1-v4.bin","status-busy2-v4.bin","status-busy3-v4.bin",
   "left-v4.bin","right-v4.bin","left-off-v4.bin","right-off-v4.bin"}
  for _,name in ipairs(artwork) do
   if not copy(name,"/data/tag_catalog/"..name) then
    failure("UI ART","/data/tag_catalog/"..name);busy=false;return
   end
  end
  if core~=CORE_VERSION and core_copied and not read("/tmp/Tag_Catalog_V1_attempted") then
   if load("/data/tag_catalog/core-catalog.elf","Tag_Catalog_V1") then
    mark("/tmp/tag_catalog_v1_loaded",CORE_VERSION)
   end
  end
  -- A failed core must still permit the native page to report FAIL. The UI
  -- consumes this request on its registration timer, after this Lua callback.
  if not mark("/tmp/nfc_tags_ui_open_request",UI_VERSION) then
   failure("UI REQ","/tmp/nfc_tags_ui_open_request");busy=false;return
  end
  if not load("/data/tag_catalog/ui-catalog-v4.elf","NFC_Tags_Catalog_UI_V4") then
   show("REBOOT");busy=false;return
  end
  if not mark("/tmp/nfc_tags_ui_loaded",UI_VERSION) then
   show("REBOOT");busy=false;return
  end
  -- Keep intercepting until the registration timer publishes exact READY.
  -- Failed registration leaves a visible REBOOT state, not a dead shortcut.
  show("WAIT");watch_ready()
  busy=false
 end)
 if read("/tmp/nfc_tags_ui_loaded")==UI_VERSION then
  show("WAIT");watch_ready()
 end
end
