-- Embedded as the second child of the 84x84 lower-circle Find Phone Widget.
-- Evaluating this file never loads a module or starts NFC. The first tap
-- installs and reboots; later boots run the same verified copy/insmod path
-- once from a bounded UI timer, so the launcher entries return automatically.
-- After the first install the overlay is transparent and never runs at render
-- time except for that timer.
local lvgl=require("lvgl")
local UI_VERSION="catalog_ui_v4\n"
local CORE_VERSION="catalog_v1\n"
local AUTO_DELAY_MS=2000
local AUTO_MARKER="/data/tag_catalog/auto_load"
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
local function blocked()
 -- Any earlier attempt in this boot must be resolved manually: a partial
 -- load is never retried silently, and a resident module is never unloaded.
 return read("/tmp/nfc_tags_ui_loaded") or
        read("/tmp/nfc_tags_catalog_ui_ready") or
        read("/tmp/tag_bootstrap_auto") or
        read("/tmp/NFC_Tags_Catalog_UI_V1_attempted") or
        read("/tmp/NFC_Tags_Catalog_UI_V2_attempted") or
        read("/tmp/NFC_Tags_Catalog_UI_V3_attempted") or
        read("/tmp/NFC_Tags_Catalog_UI_V4_attempted") or
        read("/tmp/tag_catalog_loaded")
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
local auto_timer=nil
local waiting=false
local ready_quiet=false
local pending_reboot=false
local ready_ticks=0
local function ready()
 return read("/tmp/nfc_tags_catalog_ui_ready")==UI_VERSION
end
local function pass_through()
 if ready_timer then ready_timer:pause() end
 if auto_timer then auto_timer:pause() end
 waiting=false
 -- A successful registration is the one-time proof that this build runs on
 -- the device. Only then do later boots attempt the silent auto-load.
 mark(AUTO_MARKER,UI_VERSION)
 if pending_reboot then
  pending_reboot=false
  show("REBOOT")
  -- Confirmed static path: NSH shutdown --reboot -> boardctl/board_reset.
  if not execute("shutdown --reboot") then
   show("NO REBOOT")
   root:add_flag(lvgl.FLAG.CLICKABLE)
  end
  return
 end
 if status then status:set{text=""} end
 root:set{bg_opa=0}
 root:add_flag(lvgl.FLAG.EVENT_BUBBLE)
 root:clear_flag(lvgl.FLAG.CLICKABLE)
end
local function watch_ready(quiet)
 -- Same Timer API used by the original recorder.face installer. Bound the
 -- registration wait; an insmod success must never be mistaken for readiness.
 ready_ticks=30;ready_quiet=quiet
 if not ready_timer then
  ready_timer=lvgl.Timer{period=100,paused=true,cb=function(t)
   if ready() then pass_through();return end
   ready_ticks=ready_ticks-1
   if ready_ticks<=0 then
    t:pause();waiting=false
    -- A boot-time attempt stays silent; the manual path reports REBOOT and
    -- must not reboot later on a stale ready.
    if not ready_quiet then pending_reboot=false;show("REBOOT") end
   end
  end}
 end
 waiting=true;ready_timer:resume()
end
local function install(automatic)
 if busy then return end
 if ready() then pass_through();return end
 busy=true
 local function stop(code,detail)
  if automatic then
   mark("/tmp/tag_bootstrap_auto",code.."\n"..(detail or "").."\n")
  else
   failure(code,detail)
  end
  busy=false
 end
 local model=prop("product.model","/tmp/tag_bootstrap_model")
 local version=prop("ro.build.version","/tmp/tag_bootstrap_version"):gsub("_",".")
 if model~="miwear.watch.p67glt" or version~="3.201.026" then
  if not automatic then show("VERSION") end
  busy=false;return
 end
 local ui=read("/tmp/nfc_tags_ui_loaded")
 local resident_ready=read("/tmp/nfc_tags_catalog_ui_ready")
 local core=read("/tmp/tag_catalog_v1_loaded")
 if (ui and ui~=UI_VERSION) or (resident_ready and resident_ready~=UI_VERSION) or
    (core and core~=CORE_VERSION) or
    ((read("/tmp/apdu_lab_loaded") or read("/tmp/tag_catalog_loaded")) and core~=CORE_VERSION) then
  if not automatic then show("REBOOT") end
  busy=false;return
 end
 if ui==UI_VERSION then
  if not automatic then show(waiting and "WAIT" or "REBOOT") end
  busy=false;return
 end
 if read("/tmp/NFC_Tags_Catalog_UI_V1_attempted") or
    read("/tmp/NFC_Tags_Catalog_UI_V2_attempted") or
    read("/tmp/NFC_Tags_Catalog_UI_V3_attempted") or
    read("/tmp/NFC_Tags_Catalog_UI_V4_attempted") then
  if not automatic then show("REBOOT") end
  busy=false;return
 end
 if not automatic then show("LOAD") end
 -- Exact .026 NSH mkdir -p returns -1/EEXIST for an existing final directory.
 -- Verify actual writability instead of treating that return as a failure.
 execute("mkdir -p /data/tag_catalog")
 local probe="/data/tag_catalog/.bootstrap_probe"
 local probe_value="nfc_catalog_bootstrap\n"
 if not mark(probe,probe_value) or read(probe)~=probe_value then
  stop("DIR",probe);return
 end
 os.remove(probe)
 -- Preserve both ELFs and all audited UI4 artwork before attempting insmod.
 local core_copied=copy("catalog_core_v1.elf","/data/tag_catalog/core-catalog.elf")
 local ui_copied,ui_error,ui_path=copy("catalog_ui_v4.elf","/data/tag_catalog/ui-catalog-v4.elf")
 if not ui_copied then stop("UI "..ui_error,ui_path);return end
 local artwork={"card-v4.bin","controls-v4.bin","status-ok-v4.bin","status-fail-v4.bin",
  "status-busy0-v4.bin","status-busy1-v4.bin","status-busy2-v4.bin","status-busy3-v4.bin",
  "left-v4.bin","right-v4.bin","left-off-v4.bin","right-off-v4.bin"}
 for _,name in ipairs(artwork) do
  if not copy(name,"/data/tag_catalog/"..name) then
   stop("UI ART","/data/tag_catalog/"..name);return
  end
 end
 if core~=CORE_VERSION and core_copied and not read("/tmp/Tag_Catalog_V1_attempted") then
  if load("/data/tag_catalog/core-catalog.elf","Tag_Catalog_V1") then
   mark("/tmp/tag_catalog_v1_loaded",CORE_VERSION)
  end
 end
 -- The first tap only installs and reboots; the app itself is opened from the
 -- launcher menu afterwards. The automatic path only restores registration.
 if not load("/data/tag_catalog/ui-catalog-v4.elf","NFC_Tags_Catalog_UI_V4") then
  stop("REBOOT");return
 end
 if not mark("/tmp/nfc_tags_ui_loaded",UI_VERSION) then
  stop("REBOOT");return
 end
 pending_reboot=not automatic and read(AUTO_MARKER)~=UI_VERSION
 -- Keep intercepting until the registration timer publishes exact READY, but
 -- a boot-time wait stays silent. Failed registration leaves a visible state
 -- for the manual path, not a dead shortcut.
 if automatic then
  watch_ready(true)
 else
  show("WAIT");watch_ready(false)
 end
 busy=false
end
local confirmed=read(AUTO_MARKER)==UI_VERSION
if ready() or confirmed then
 -- After the first confirmed install the overlay is transparent from the
 -- first frame: the native circle keeps its stock action, and the silent
 -- auto-load only restores the launcher entries.
 pass_through()
 if confirmed and not ready() then
  auto_timer=lvgl.Timer{period=AUTO_DELAY_MS,cb=function(t)
   t:pause()
   if busy or ready() or blocked() then return end
   install(true)
  end}
 end
else
 root:add_flag(lvgl.FLAG.CLICKABLE)
 -- onPressed is firmware event1; the Widget launch is SHORT_CLICKED event4.
 -- This also recovers a late READY after our bounded wait expired, before the
 -- same touch's release/short-click travels to the native parent.
 root:onPressed(function()if ready() then pass_through() end end)
 root:onClicked(function()
  if busy then return end
  if ready() then pass_through();return end
  install(false)
 end)
 if read("/tmp/nfc_tags_ui_loaded")==UI_VERSION then
  show("WAIT");watch_ready(false)
 end
end
