--[[
  TiDaoji — Cheat Engine autorun plugin (Lua form UI)

  Install (CE 7.x, elevated host recommended for driver IO):
    1) sc start TiDaoji  (L1/KDU as usual)
    2) copy TiDaoji.lua  ->  <CE>\autorun\TiDaoji.lua
    3) copy tidaoji_cli.exe next to this script OR set CLI path below
    4) restart CE

  Behaviour:
    - Menu: Table / CE top menu "TiDaoji" → Show panel / Hide opened / Unhide / Status
    - onOpenProcess: optional auto-HidePid on the process CE just opened
    - Panel: PID, type bits, driver name, Hide/Unhide/UnhideAll/SoftUnload

  NOT PG-safe. Lab / research only. No dual InfinityHook with CR.
]]

local PLUGIN = "TiDaoji"
local DEFAULT_TYPE = 0xFFF  -- BIT1..BIT12
local DEFAULT_DRIVER = "TiDaoji"

-- Prefer CLI next to this script / CE autorun / common lab path
local function findCli()
  local candidates = {
    (getCheatEngineDir() or "") .. "autorun\\tidaoji_cli.exe",
    (getCheatEngineDir() or "") .. "tidaoji_cli.exe",
    "D:\\src\\TiDaoji\\tools\\tidaoji_cli.exe",
    "D:\\src\\TiDaoji\\x64\\Release\\tidaoji_cli.exe",
  }
  -- script path if available
  if getCurrentLuaFile then
    local p = getCurrentLuaFile()
    if p and p ~= "" then
      local dir = p:match("^(.*[\\/])") or ""
      table.insert(candidates, 1, dir .. "tidaoji_cli.exe")
    end
  end
  for _, c in ipairs(candidates) do
    if c and c ~= "" then
      local f = io.open(c, "rb")
      if f then f:close(); return c end
    end
  end
  return nil
end

local g = {
  cli = findCli(),
  driver = DEFAULT_DRIVER,
  typeMask = DEFAULT_TYPE,
  autoHide = true,
  form = nil,
  lastStatus = "idle",
}

local function band(a, b)
  if bit32 and bit32.band then return bit32.band(a, b) end
  if bit and bit.band then return bit.band(a, b) end
  return a & b -- Lua 5.3+
end

local TYPE_BITS = {
  { bit = 0x001, name = "ProcessDebugFlags" },
  { bit = 0x002, name = "ProcessDebugPort" },
  { bit = 0x004, name = "ProcessDebugObjectHandle" },
  { bit = 0x008, name = "DebugObject" },
  { bit = 0x010, name = "SystemDebuggerInformation" },
  { bit = 0x020, name = "NtClose" },
  { bit = 0x040, name = "ThreadHideFromDebugger" },
  { bit = 0x080, name = "NtGetContextThread" },
  { bit = 0x100, name = "NtSetContextThread" },
  { bit = 0x200, name = "NtSystemDebugControl" },
  { bit = 0x400, name = "SystemFirmwareVMScrub" },
  { bit = 0x800, name = "NtTerminateProcess" },
}

local function log(msg)
  print(string.format("[%s] %s", PLUGIN, msg))
end

local function runCli(args)
  if not g.cli then
    g.lastStatus = "CLI missing (build tools/tidaoji_cli.exe)"
    log(g.lastStatus)
    return false, g.lastStatus
  end
  local cmd = string.format('"%s" %s', g.cli, args)
  log("exec: " .. cmd)
  -- CE: shellExecute wait; fall back to os.execute
  local ok, exitCode
  if shellExecute then
    -- shellExecute(command, params, workdir, showcmd) — vary by CE version
    ok = shellExecute(g.cli, args, nil, true)
    exitCode = ok and 0 or 1
  else
    exitCode = os.execute(cmd)
    -- Lua 5.x may return true/false or integer
    if exitCode == true then exitCode = 0 end
    if type(exitCode) ~= "number" then exitCode = exitCode and 0 or 1 end
  end
  if exitCode == 0 then
    g.lastStatus = "OK: " .. args
    return true, g.lastStatus
  end
  g.lastStatus = string.format("FAIL(%s): %s", tostring(exitCode), args)
  return false, g.lastStatus
end

local function currentPid()
  if getOpenedProcessID then
    local p = getOpenedProcessID()
    if p and p ~= 0 then return p end
  end
  return 0
end

local function doHide(pid, typeMask)
  pid = pid or currentPid()
  if not pid or pid == 0 then
    g.lastStatus = "no opened process"
    log(g.lastStatus)
    return false
  end
  typeMask = typeMask or g.typeMask
  return runCli(string.format("hide %d --type 0x%X --driver %s", pid, typeMask, g.driver))
end

local function doUnhide(pid)
  pid = pid or currentPid()
  if not pid or pid == 0 then
    g.lastStatus = "no opened process"
    return false
  end
  return runCli(string.format("unhide %d --driver %s", pid, g.driver))
end

local function doUnhideAll()
  return runCli(string.format("unhide-all --driver %s", g.driver))
end

local function doSoftUnload()
  return runCli(string.format("soft-unload --driver %s", g.driver))
end

local function doStatus()
  return runCli(string.format("status --driver %s", g.driver))
end

-- CE callback: process opened
function onOpenProcess(processid)
  if not g.autoHide then return end
  if not processid or processid == 0 then return end
  log(string.format("onOpenProcess pid=%d autoHide", processid))
  doHide(processid, g.typeMask)
end

local function collectTypeFromChecks(checks)
  local m = 0
  for i, t in ipairs(TYPE_BITS) do
    if checks[i] and checks[i].Checked then
      m = m + t.bit
    end
  end
  return m
end

local function showForm()
  if g.form and (not g.form.Destroyed) then
    g.form.show()
    g.form.bringToFront()
    return
  end

  local f = createForm(false)
  f.Caption = "TiDaoji for Cheat Engine — NOT PG-safe"
  f.Width = 420
  f.Height = 520
  f.Position = "poScreenCenter"
  g.form = f

  local y = 8
  local lblStatus
  local lblCli = createLabel(f)
  lblCli.Caption = "CLI: " .. (g.cli or "(not found — place tidaoji_cli.exe in CE\\autorun)")
  lblCli.Left = 8; lblCli.Top = y; lblCli.Width = 400
  y = y + 22

  local lblDriver = createLabel(f)
  lblDriver.Caption = "Driver:"
  lblDriver.Left = 8; lblDriver.Top = y + 2
  local edtDriver = createEdit(f)
  edtDriver.Text = g.driver
  edtDriver.Left = 60; edtDriver.Top = y; edtDriver.Width = 120

  local lblPid = createLabel(f)
  lblPid.Caption = "PID:"
  lblPid.Left = 200; lblPid.Top = y + 2
  local edtPid = createEdit(f)
  edtPid.Text = tostring(currentPid())
  edtPid.Left = 230; edtPid.Top = y; edtPid.Width = 80

  local btnRefresh = createButton(f)
  btnRefresh.Caption = "↻"
  btnRefresh.Left = 320; btnRefresh.Top = y; btnRefresh.Width = 36
  btnRefresh.OnClick = function()
    edtPid.Text = tostring(currentPid())
  end
  y = y + 28

  local chkAuto = createCheckBox(f)
  chkAuto.Caption = "Auto Hide on open process (onOpenProcess)"
  chkAuto.Checked = g.autoHide
  chkAuto.Left = 8; chkAuto.Top = y; chkAuto.Width = 360
  y = y + 24

  local gb = createGroupBox(f)
  gb.Caption = "Hide Type bits"
  gb.Left = 8; gb.Top = y; gb.Width = 390; gb.Height = 280
  local checks = {}
  local cy = 18
  for i, t in ipairs(TYPE_BITS) do
    local cb = createCheckBox(gb)
    cb.Caption = string.format("0x%03X  %s", t.bit, t.name)
    cb.Checked = band(g.typeMask, t.bit) ~= 0
    cb.Left = 10; cb.Top = cy; cb.Width = 360
    checks[i] = cb
    cy = cy + 20
  end
  y = y + 290

  local function applySettingsFromUi()
    g.driver = edtDriver.Text
    if g.driver == "" then g.driver = DEFAULT_DRIVER end
    g.typeMask = collectTypeFromChecks(checks)
    g.autoHide = chkAuto.Checked
  end

  local function pidFromUi()
    applySettingsFromUi()
    local p = tonumber(edtPid.Text)
    if not p or p == 0 then p = currentPid() end
    return p or 0
  end

  local btnHide = createButton(f)
  btnHide.Caption = "Hide"
  btnHide.Left = 8; btnHide.Top = y; btnHide.Width = 70
  btnHide.OnClick = function()
    local p = pidFromUi()
    doHide(p, g.typeMask)
    if lblStatus then lblStatus.Caption = g.lastStatus end
  end

  local btnUnhide = createButton(f)
  btnUnhide.Caption = "Unhide"
  btnUnhide.Left = 86; btnUnhide.Top = y; btnUnhide.Width = 70
  btnUnhide.OnClick = function()
    local p = pidFromUi()
    doUnhide(p)
    if lblStatus then lblStatus.Caption = g.lastStatus end
  end

  local btnUnhideAll = createButton(f)
  btnUnhideAll.Caption = "UnhideAll"
  btnUnhideAll.Left = 164; btnUnhideAll.Top = y; btnUnhideAll.Width = 80
  btnUnhideAll.OnClick = function()
    applySettingsFromUi()
    doUnhideAll()
    if lblStatus then lblStatus.Caption = g.lastStatus end
  end

  local btnSoft = createButton(f)
  btnSoft.Caption = "SoftUnload"
  btnSoft.Left = 252; btnSoft.Top = y; btnSoft.Width = 80
  btnSoft.OnClick = function()
    applySettingsFromUi()
    if messageDialog("SoftUnload tears down InfinityHook. Continue?", mtWarning, mbYes, mbNo) == mrYes then
      doSoftUnload()
      if lblStatus then lblStatus.Caption = g.lastStatus end
    end
  end

  local btnStat = createButton(f)
  btnStat.Caption = "Status"
  btnStat.Left = 340; btnStat.Top = y; btnStat.Width = 60
  btnStat.OnClick = function()
    applySettingsFromUi()
    doStatus()
    if lblStatus then lblStatus.Caption = g.lastStatus end
  end
  y = y + 32

  local btnAll = createButton(f)
  btnAll.Caption = "Select All"
  btnAll.Left = 8; btnAll.Top = y; btnAll.Width = 90
  btnAll.OnClick = function()
    for _, cb in ipairs(checks) do cb.Checked = true end
  end
  local btnNone = createButton(f)
  btnNone.Caption = "Select None"
  btnNone.Left = 106; btnNone.Top = y; btnNone.Width = 90
  btnNone.OnClick = function()
    for _, cb in ipairs(checks) do cb.Checked = false end
  end
  y = y + 28

  lblStatus = createLabel(f)
  lblStatus.Caption = g.lastStatus
  lblStatus.Left = 8; lblStatus.Top = y; lblStatus.Width = 400

  f.show()
end

-- Menu: attach under main form
local function installMenu()
  if not MainForm then return end
  local mi
  -- Prefer dedicated top-level item
  if createMenuItem and MainForm.Menu then
    mi = createMenuItem(MainForm.Menu)
    mi.Caption = "TiDaoji"
    local mShow = createMenuItem(mi)
    mShow.Caption = "Show panel..."
    mShow.OnClick = function() showForm() end
    local mHide = createMenuItem(mi)
    mHide.Caption = "Hide opened process"
    mHide.OnClick = function() doHide(currentPid(), g.typeMask) end
    local mUn = createMenuItem(mi)
    mUn.Caption = "Unhide opened process"
    mUn.OnClick = function() doUnhide(currentPid()) end
    local mSt = createMenuItem(mi)
    mSt.Caption = "Driver status"
    mSt.OnClick = function() doStatus() end
    MainForm.Menu.Items.add(mi)
    log("menu installed")
  else
    log("MainForm.Menu unavailable — call showTiDaoji() from Lua engine")
  end
end

function showTiDaoji()
  showForm()
end

function TiDaojiHide(pid)
  return doHide(pid or currentPid(), g.typeMask)
end

function TiDaojiUnhide(pid)
  return doUnhide(pid or currentPid())
end

-- boot
log("loaded. CLI=" .. tostring(g.cli))
log("Type default=0x" .. string.format("%X", DEFAULT_TYPE) .. " autoHide=" .. tostring(g.autoHide))
installMenu()
