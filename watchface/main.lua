local lvgl = require("lvgl")
local function property(key, path)
  os.execute("getprop " .. key .. " > " .. path)
  local f = io.open(path, "r")
  local s = f and f:read("*l") or ""
  if f then f:close() end
  return s:match("^%s*(.-)%s*$")
end
local model = property("product.model", "/tmp/apdu_lab_model")
local version = property("ro.build.version", "/tmp/apdu_lab_version"):gsub("_", ".")
local supported = model == "miwear.watch.p67glt" and version == "3.201.026"
local root = lvgl.Object(nil, {w=336,h=480,bg_color=0x07111F,bg_opa=lvgl.OPA(100)})
root:clear_flag(lvgl.FLAG.SCROLLABLE)
lvgl.Label(root, {text="APDU Lab v6", text_color=0xFFFFFF,
 text_font=lvgl.Font("MiSans-Demibold",30,"normal"),
 align={type=lvgl.ALIGN.CENTER,x_ofs=0,y_ofs=-180}})
local status = lvgl.Label(root, {text=supported and
 "p67glt / 3.201.026\nTest NFC + Bluetooth\nNo payment cards" or
 ("Unsupported device\n" .. model .. "\n" .. version),
 width=300,height=160,text_color=0xBFD9FF,
 text_font=lvgl.Font("MiSans-Demibold",20,"normal"),
 align={type=lvgl.ALIGN.CENTER,x_ofs=0,y_ofs=-65}})
local button = lvgl.Object(root,{w=300,h=65,bg_color=0x14508A,
 bg_opa=lvgl.OPA(100),radius=16,
 align={type=lvgl.ALIGN.CENTER,x_ofs=0,y_ofs=50}})
button:add_flag(lvgl.FLAG.CLICKABLE)
lvgl.Label(button,{text="INSTALL LAB",text_color=0xFFFFFF,
 text_font=lvgl.Font("MiSans-Demibold",24,"normal"),align=lvgl.ALIGN.CENTER})
local diag=lvgl.Object(root,{w=300,h=50,bg_color=0x14508A,
 bg_opa=lvgl.OPA(100),radius=16,
 align={type=lvgl.ALIGN.CENTER,x_ofs=0,y_ofs=120}})
diag:add_flag(lvgl.FLAG.CLICKABLE)
lvgl.Label(diag,{text="STATUS",text_color=0xFFFFFF,
 text_font=lvgl.Font("MiSans-Demibold",22,"normal"),align=lvgl.ALIGN.CENTER})
diag:onClicked(function()
  local f=io.open("/tmp/apdu_lab_state","r")
  local s=f and f:read(256) or "No native status file.\nInitialization not confirmed."
  if f then f:close() end
  local log=io.open("/dev/kmsg","rb")
  local ipc="IPC log: unavailable"
  if log then
    ipc="IPC log: no matching line"
    local ok,data=pcall(function() return log:read(65536) end)
    log:close()
    if ok and data then
      for line in data:gmatch("[^\n]+") do
        local ret,err=line:match("bt socket send ret:%s*([%-]?%d+)%s+error:%s*(%d+)")
        if ret then ipc="IPC send="..ret.." errno="..err end
      end
    elseif not ok then
      ipc="IPC log: read failed"
    end
  end
  s=(s or "Empty native status.").."\n"..ipc
  status:set{text=s or "Empty native status.\nTap STATUS again."}
end)
lvgl.Label(root,{text="Start and stop NFC from Pixel.\nKeep this face visible.\nModule stays until restart.",
 width=300,height=70,text_color=0xBFD9FF,
 text_font=lvgl.Font("MiSans-Demibold",14,"normal"),
 align={type=lvgl.ALIGN.CENTER,x_ofs=0,y_ofs=195}})
local busy, loaded = false, false
local f = io.open("/tmp/apdu_lab_loaded", "r")
if f then loaded=true; f:close(); status:set{text="Module already loaded.\nOpen APDU Lab on Pixel."} end
local function copy_elf()
  local src = io.open(SCRIPT_PATH .. "apdu_lab_probe_3.201.026.elf", "rb")
  if not src then return false end
  local dst = io.open("/data/apdu_lab/probe.elf", "wb")
  if not dst then src:close(); return false end
  local ok=true
  while true do
    local chunk=src:read(4096)
    if not chunk then break end
    if not dst:write(chunk) then ok=false; break end
  end
  src:close(); dst:close(); return ok
end
button:onClicked(function()
  if not supported or busy or loaded then return end
  busy=true
  status:set{text="Installing test module..."}
  os.execute("mkdir -p /data/apdu_lab")
  if not copy_elf() then
    status:set{text="Cannot copy module."}; busy=false; return
  end
  local a, _, code=os.execute("insmod /data/apdu_lab/probe.elf APDU_Lab")
  if a==true or a==0 or code==0 then
    loaded=true
    local mark=io.open("/tmp/apdu_lab_loaded","w")
    if mark then mark:write("loaded\n"); mark:close() end
    status:set{text="ELF loaded.\nTap STATUS to check startup.\nThen open APDU Lab on Pixel."}
  else
    status:set{text="Load failed.\nVersion check or loader error.\nDo not retry another firmware."}
  end
  busy=false
end)
