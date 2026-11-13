local lvgl=require("lvgl")
local function prop(key,path)
 os.execute("getprop "..key.." > "..path)
 local f=io.open(path,"r");local s=f and f:read("*l") or ""
 if f then f:close() end
 return s:match("^%s*(.-)%s*$")
end
local model=prop("product.model","/tmp/tag_model")
local version=prop("ro.build.version","/tmp/tag_version"):gsub("_",".")
local supported=model=="miwear.watch.p67glt" and version=="3.201.026"
local function show_tags()
local root=lvgl.Object(nil,{w=336,h=480,bg_color=0x07111F,bg_opa=lvgl.OPA(100)})
root:clear_flag(lvgl.FLAG.SCROLLABLE)
local function label(text,size,y,h)
 return lvgl.Label(root,{text=text,width=300,height=h,text_color=0xFFFFFF,
 text_font=lvgl.Font("MiSans-Demibold",size,"normal"),
 align={type=lvgl.ALIGN.CENTER,x_ofs=0,y_ofs=y}})
end
label("NFC Tags v3",28,-195,40)
local title=label("No tag selected",24,-140,60)
local details=label(supported and "Install, then select a tag\non Pixel." or
 ("Unsupported device\n"..model.."\n"..version),17,-45,140)
local footer=label("Pixel must stay connected.\nOriginal UID is not emulated.",14,213,35)
local function button(text,y,callback)
 local b=lvgl.Object(root,{w=300,h=44,bg_color=0x14508A,bg_opa=lvgl.OPA(100),radius=16,
 align={type=lvgl.ALIGN.CENTER,x_ofs=0,y_ofs=y}})
 b:add_flag(lvgl.FLAG.CLICKABLE)
 lvgl.Label(b,{text=text,text_color=0xFFFFFF,text_font=lvgl.Font("MiSans-Demibold",20,"normal"),align=lvgl.ALIGN.CENTER})
 b:onClicked(callback)
end
local function exists(path)
 local f=io.open(path,"r");if f then f:close();return true end
 return false
end
local loaded=exists("/tmp/tag_catalog_v3_loaded")
-- Rendering the selected-tag screen is separate from module installation so
-- a later launcher/widget entry can reuse this view without starting NFC.
local function refresh()
 local f=io.open("/tmp/tag_catalog_state","r")
 if not f then details:set{text="No module state yet.\nTap INSTALL TAGS."};return end
 local s=f:read(256) or "";f:close()
 local lines={};for line in s:gmatch("(.-)\n") do lines[#lines+1]=line end
 if #lines~=7 or lines[1]~="TAG1" or lines[7]~="END" then
  details:set{text="State updating.\nTap REFRESH again."};return
 end
 title:set{text=lines[2]}
 local kind=lines[5]=="1" and "NDEF sharing supported" or "Display only: unsupported"
 details:set{text="UID: "..(lines[3]~="" and lines[3] or "none").."\n"..lines[4].."\n"..kind.."\n"..lines[6]}
end
local busy=false
button("INSTALL TAGS",55,function()
 if not supported or busy then return end
 if loaded then refresh();return end
 if exists("/tmp/apdu_lab_loaded") then
  details:set{text="Another module is resident.\nRestart the band first."};return
 end
 busy=true
 os.execute("mkdir -p /data/tag_catalog")
 local src=io.open(SCRIPT_PATH.."tag_bridge_v03_3.201.026.elf","rb")
 local dst=src and io.open("/data/tag_catalog/probe-v3.elf","wb")
 if not src or not dst then
  if src then src:close() end
  details:set{text="Cannot copy module."};busy=false;return
 end
 local ok,name,marker,tail=true,false,false,""
 while true do
  local chunk=src:read(4096);if not chunk then break end
  local scan=tail..chunk;tail=scan:sub(-16)
  name=name or scan:find("MB-TAGS",1,true)
  marker=marker or scan:find("[TAG_CATALOG]",1,true)
  if not dst:write(chunk) then ok=false;break end
 end
 src:close();if not dst:close() then ok=false end
 if not ok then details:set{text="Copy failed."};busy=false;return end
 if not name or not marker then
  details:set{text="Wrong module identity."};busy=false;return
 end
 local a,_,code=os.execute("insmod /data/tag_catalog/probe-v3.elf Tag_Catalog_v3")
 if a==true or a==0 or code==0 then
  loaded=true
  for _,p in ipairs({"/tmp/tag_catalog_v3_loaded","/tmp/tag_catalog_loaded","/tmp/apdu_lab_loaded"}) do
   local m=io.open(p,"w");if m then m:write("tag_catalog\n");m:close() end
  end
  details:set{text="ELF loaded. Open Pixel app,\nselect a tag and sync it.\nThen tap REFRESH."}
 else details:set{text="Module load failed.\nRestart before retrying."} end
 busy=false
end)
button("REFRESH",110,refresh)
button("DIAGNOSTICS",165,function()
 local f=io.open("/tmp/tag_catalog_diag","r")
 local s=f and f:read(256) or "No native diagnostic file."
 if f then f:close() end
 details:set{text=s or "Empty native diagnostic."}
end)
if loaded then refresh() end
return root
end
show_tags()
