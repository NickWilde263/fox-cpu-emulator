#!/bin/lua5.3
local input = io.open("rom._asm")
local output = io.open("_rom.exe", "w+")
local tokens = {}
local tmp = nil
local processString = false
local stringPos = 0

local j = 0
for line in input:lines() do
  j = j + 1
  tmp = ""
  line = line.." "
  local iterator = string.gmatch(line, ".")
  for char in iterator do
    if char == ";" then
      break
    end
    
    if (char == " " or char == "\n" or char == ",") and processString == false then
      --if processString == false then
        if tmp ~= "" then
          table.insert(tokens, tmp)
          tmp = ""
          if char == "\n" then
            table.insert(tokens, "\n")
          end
        end
      --end
    elseif char == "\"" then
      local tmpB = ""
      for char in iterator do
        if char == "\"" then
          break
        end
        tmpB = tmpB..char
      end
      table.insert(tokens, "\""..tmpB)
    else  
      tmp = tmp..char
    end
  end
  table.insert(tokens, "\n")
end

if processString == true then
  print("Reached EOF when parsing string at line #"..tostring(stringPos))
end

local symbolTable = {}
local ip = 0
local orgDefine = 0
local i = 1
local tokenI = 1

function raise(...)
  print("Error at line #"..tostring(tokenI).." instruction "..tokens[i])
  print(table.concat({...}))
  os.exit()
end

function warn(...)
  print("Warn at line #"..tostring(tokenI).." instruction "..tokens[i])
  print(table.concat({...}))
end

local executable = ""
while i <= #tokens do
  token = tokens[i] --Do not lower it
  
  if tokens[i] == "\n" then 
    tokenI = tokenI + 1
  end
  if false then--token == "org" then
    --[[if tonumber(tokens[i + 1]) == nil then
      raise("Expect any number")
    elseif orgDefine > 0 then
      raise("Redefine origin initially defined at line #"..tostring(orgDefine))
    else
      orgDefine = tonumber(tokens[i + 1])
      if orgDefine > 255 or orgDefine < 0 then
        raise("Origin must be in range of 0 - 255")
      end
    end
    i = i + 1]]
  elseif token == "#exit" then
    break
  elseif token == "mov" then --16-bit move
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 8 then
        raise("Operand #1 is out of range 0 - 8")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 7 then
          raise("Operand #2 is out of range 0 - 7")
        end
        executable = executable.."3,"..tostring(regA)..","..tostring(regB)..","
        ip = ip + 3
      elseif string.gmatch(tokens[i + 2], ".")() == "\"" then
        local regB = tokens[i + 2]
        if string.len(regB) > 3 then
          warn("String too long stripping rest of the string")
        elseif string.len(regB) == 0 then
          raise("Empty string")
        end
        regB = string.gmatch(regB, "\".+")()
        local tmp = regB:gsub("^\"", ""):gmatch(".")
        executable = executable.."4,"..tostring(regA)..","..tostring(string.byte(tmp()))..","..tostring(string.byte(tmp() or "\0"))..","
        ip = ip + 4
      elseif string.gmatch(tokens[i + 2], ".")() == ":" then
        executable = executable.."4,"..tostring(regA)..","..tokens[i + 2]..","
        ip = ip + 4
      else
        local regol = regB
        local regB = tonumber(tokens[i + 2] or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 65535 then
          raise("Operand #2 is out of range 0 - 65,535")
        end
        executable = executable.."4,"..tostring(regA)..","..tostring(math.floor(regB / 256) % 256)..","..tostring(regB % 256)..","
        ip = ip + 4
      end
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    i = i + 3
  elseif token == "prn" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."254,"..tostring(regA)..","
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    i = i + 1
    ip = ip + 2
  elseif token == "tele" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 15 then
        raise("Operand #1 is out of range 0 - 15")
      end
      executable = executable.."252,"..tostring(regA)..","
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    i = i + 1
    ip = ip + 2
  elseif token == "movb" then --8-bit move
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 15 then
        raise("Operand #1 is out of range 0 - 15")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 15 then
          raise("Operand #2 is out of range 0 - 15")
        end
        executable = executable.."8,"..tostring(regA)..","..tostring(regB)..","
        i = i + 2
        ip = ip + 3
      elseif string.gmatch(tokens[i + 2], ".")() == "\"" then
        local regB = tokens[i + 2]
        if string.len(regB) > 2 then
          warn("String too long stripping rest of the string")
          tmp = tokens[i + 2]:gsub("^\"", "")
          print("For string ", tmp)
        elseif string.len(regB) == 0 then
          raise("Empty string")
        end
        --print(string.gmatch(regB, "\".+")())
        local tmp = string.gmatch(string.gsub(string.gmatch(regB, "\".+")(), "^\"", ""), ".")
        executable = executable.."9,"..tostring(regA)..","..tostring(string.byte(tmp()))..","
        i = i + 2
        ip = ip + 3
      elseif string.gmatch(tokens[i + 2], ".S$")() ~= nil then
        local regB = string.gmatch(tokens[i + 2], ".S$")():gmatch(".")():gmatch(".")()
        if regB == "C" then
          executable = executable.."39,"..tostring(regA)..","
        elseif regB == "S" then
          executable = executable.."40,"..tostring(regA)..","
        elseif regB == "D" then
          executable = executable.."41,"..tostring(regA)..","
        elseif regB == "E" then
          executable = executable.."42,"..tostring(regA)..","
        else
          raise("Invalid operand 2")
        end
        i = i + 3
        ip = ip + 2
      elseif string.gmatch(tokens[i + 2], ".P$")() ~= nil then
        local regB = string.gmatch(tokens[i + 2], ".P$")():gmatch(".")():gmatch(".")()
        if regB == "B" then
          executable = executable.."44,"..tostring(regB)..","
        elseif regB == "S" then
          executable = executable.."43,"..tostring(regA)..","
        else
          raise("Invalid operand 2")
        end
        i = i + 3
        ip = ip + 2
      elseif string.gmatch(tokens[i + 2], ".I$")() ~= nil then
        local regB = string.gmatch(tokens[i + 2], ".I$")():gmatch(".")():gmatch(".")()
        
        if regB == "D" then
          executable = executable.."45,"..tostring(regA)..","
        elseif regB == "S" then
          executable = executable.."46,"..tostring(regA)..","
        else
          raise("Invalid operand 2")
        end
        i = i + 3
        ip = ip + 2
      else
        local regB = tonumber(tokens[i + 2] or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 255 then
          raise("Operand #2 is out of range 0 - 255")
        end
        executable = executable.."9,"..tostring(regA)..","..tostring(regB)..","
        i = i + 2
        ip = ip + 3
      end
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif token == "push" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."28,"..tostring(regA)..","
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    ip = ip + 2
    i = i + 1
  elseif token == "pop" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."27,"..tostring(regA)..","
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    ip = ip + 2
    i = i + 1
  elseif token == "pushb" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 15 then
        raise("Operand #1 is out of range 0 - 15")
      end
      executable = executable.."26,"..tostring(regA)..","
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    i = i + 2
    ip = ip + 2
  elseif token == "popb" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 15 then
        raise("Operand #1 is out of range 0 - 15")
      end
      executable = executable.."25,"..tostring(tostring(regA))..","
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    i = i + 2
    ip = ip + 2
  elseif token == "short_out" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 8 then
        raise("Operand #1 is out of range 0 - 8")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 7 then
          raise("Operand #2 is out of range 0 - 7")
        end
        executable = executable.."47,"..tostring(regA)..","..tostring(regB)..","
        ip = ip + 3
      else
        raise("Invalid operand 2") 
      end
    else
      raise("Invalid operand 1")
    end
    i = i + 2
  elseif token == "short_in" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 8 then
        raise("Operand #1 is out of range 0 - 8")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 7 then
          raise("Operand #2 is out of range 0 - 7")
        end
        executable = executable.."48,"..tostring(regA)..","..tostring(regB)..","
        ip = ip + 3
      else
        raise("Invalid operand 2") 
      end
    else
      raise("Invalid operand 1")
    end
    i = i + 2
  elseif token == "clf" then
    executable = executable.."12,"
    ip = ip + 1
  elseif token == "ret" then
    executable = executable.."49,"
    ip = ip + 1
  elseif token == "call" then
    if string.gmatch(tokens[i + 1], ".")() == ":" then --Mean labeled call
      local tmp = ""
      local j = 0
      while tokens[i + j + 1] ~= "\n" do
        tmp = tmp..tokens[i + j + 1]
        j = j + 1
      end
      executable = executable.."50,"..tmp..","
      ip = ip + 3
    elseif tonumber(tokens[i + 1]) ~= nil and tonumber(tokens[i + 1]) <= 65535 and tonumber(tokens[i + 1]) >= 0 then
      executable = executable.."50,"..tostring(math.floor(tonumber(tokens[i + 1]) / 256) % 256)..","..tostring(tonumber(tokens[i + 1]) % 256)..","
      ip = ip + 3
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    i = i + 1
  elseif token == "incb" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 15 then
        raise("Operand #1 is out of range 0 - 15")
      end
      executable = executable.."35,"..tostring(tostring(regA))..","
      ip = ip + 2
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    i = i + 1
  elseif token == "inc" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."36,"..tostring(tostring(regA))..","
      ip = ip + 2
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    i = i + 1
  elseif token == "decb" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 15 then
        raise("Operand #1 is out of range 0 - 15")
      end
      executable = executable.."37,"..tostring(tostring(regA))..","
      ip = ip + 2
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
    i = i + 1
  elseif token == "dec" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."38,"..tostring(tostring(regA))..","
      ip = ip + 2
      i = i + 1
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif token == "popa" then
    executable = executable.."30,"
    ip = ip + 1
  elseif token == "pusha" then
    executable = executable.."29,"
    ip = ip + 1
  elseif token == "hlt" then
    executable = executable.."255,"
    ip = ip + 1
  elseif token == "nop" then
    executable = executable.."0,"
    ip = ip + 1
  elseif token == "add" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 7 then
          raise("Operand #2 is out of range 0 - 7")
        end
        executable = executable.."1,"..tostring(regA)..","..tostring(regB)..","
        i = i + 2
        ip = ip + 3
      else
        local regB = tonumber(tokens[i + 2] or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 65535 then
          raise("Operand #2 is out of range 0 - 65,535")
        end
        executable = executable.."31,"..tostring(regA)..","..tostring(math.floor(regB / 256) % 256)..","..tostring(regB % 256)..","
        i = i + 2
        ip = ip + 4
      end 
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif token == "sub" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 7 then
          raise("Operand #2 is out of range 0 - 7")
        end
        executable = executable.."2,"..tostring(regA)..","..tostring(regB)..","
        i = i + 2
        ip = ip + 3
      else
        local regB = tonumber(tokens[i + 2] or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 65535 then
          raise("Operand #2 is out of range 0 - 65,535")
        end
        executable = executable.."32,"..tostring(regA)..","..tostring(math.floor(regB / 256) % 256)..","..tostring(regB % 256)..","
        i = i + 2
        ip = ip + 4
      end
    end
  elseif token == "addb" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 15 then
        raise("Operand #1 is out of range 0 - 15")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 15 then
          raise("Operand #2 is out of range 0 - 15")
        end
        executable = executable.."6,"..tostring(regA)..","..tostring(regB)..","
        i = i + 2
        ip = ip + 3
      else
        local regB = tonumber(tokens[i + 2] or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 255 then
          raise("Operand #2 is out of range 0 - 255")
        end
        executable = executable.."33,"..tostring(regA)..","..tostring(regB)..","
        i = i + 2
        ip = ip + 3
      end 
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif token == "subb" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 15 then
        raise("Operand #1 is out of range 0 - 15")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 15 then
          raise("Operand #2 is out of range 0 - 15")
        end
        executable = executable.."7,"..tostring(regA)..","..tostring(regB)..","
        i = i + 2
        ip = ip + 3
      else
        local regB = tonumber(tokens[i + 2] or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 255 then
          raise("Operand #2 is out of range 0 - 255")
        end
        executable = executable.."34,"..tostring(regA)..","..tostring(regB)..","
        i = i + 2
        ip = ip + 3
      end 
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif string.gmatch(tokens[i], ".")() == ":" then
    local tmp = ""
    local j = 0
    while tokens[i + j] ~= "\n" do
      tmp = tmp..tokens[i + j]
      j = j + 1
    end
    symbolTable[tmp] = ip
  elseif tokens[i] == "jz" then
    if string.gmatch(tokens[i + 1], ".")() == ":" then --Mean labeled jump
      local tmp = ""
      local j = 0
      while tokens[i + j + 1] ~= "\n" do
        tmp = tmp..tokens[i + j + 1]
        j = j + 1
      end
      executable = executable.."18,"..tmp..","
      i = i + 1
      ip = ip + 3
    elseif string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."24,"..tostring(regA)..","
      i = i + 1
      ip = ip + 1
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif tokens[i] == "js" then
    if string.gmatch(tokens[i + 1], ".")() == ":" then --Mean labeled jump
      local tmp = ""
      local j = 0
      while tokens[i + j + 1] ~= "\n" do
        tmp = tmp..tokens[i + j + 1]
        j = j + 1
      end
      executable = executable.."15,"..tmp..","
      i = i + 1
      ip = ip + 3
    elseif string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."21,"..tostring(regA)..","
      i = i + 1
      ip = ip + 2
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif tokens[i] == "jg" then
    if string.gmatch(tokens[i + 1], ".")() == ":" then --Mean labeled jump
      local tmp = ""
      local j = 0
      while tokens[i + j + 1] ~= "\n" do
        tmp = tmp..tokens[i + j + 1]
        j = j + 1
      end
      executable = executable.."16,"..tmp..","
      i = i + 1
      ip = ip + 3
    elseif string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."22,"..tostring(regA)..","
      i = i + 1
      ip = ip + 2
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif tokens[i] == "je" then
    if string.gmatch(tokens[i + 1], ".")() == ":" then --Mean labeled jump
      local tmp = ""
      local j = 0
      while tokens[i + j + 1] ~= "\n" do
        tmp = tmp..tokens[i + j + 1]
        j = j + 1
      end
      executable = executable.."14,"..tmp..","
      i = i + 1
      ip = ip + 3
    elseif string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."20,"..tostring(regA)..","
      i = i + 1
      ip = ip + 2
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif tokens[i] == "jc" then
    if string.gmatch(tokens[i + 1], ".")() == ":" then --Mean labeled jump
      local tmp = ""
      local j = 0
      while tokens[i + j + 1] ~= "\n" do
        tmp = tmp..tokens[i + j + 1]
        j = j + 1
      end
      executable = executable.."17,"..tmp..","
      i = i + 1
      ip = ip + 3
    elseif string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."23,"..tostring(regA)..","
      i = i + 1
      ip = ip + 2
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif tokens[i] == "jmp" then
    if string.gmatch(tokens[i + 1], ".")() == ":" then --Mean labeled jump
      local tmp = ""
      local j = 0
      while tokens[i + j + 1] ~= "\n" do
        tmp = tmp..tokens[i + j + 1]
        j = j + 1
      end
      executable = executable.."13,"..tmp..","
      i = i + 1
      ip = ip + 3
    elseif string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      executable = executable.."19,"..tostring(regA)..","
      i = i + 1
      ip = ip + 2
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif tokens[i] == "mulb" then
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 15 then
        raise("Operand #1 is out of range 0 - 15")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 15 then
          raise("Operand #2 is out of range 0 - 15")
        end
        if string.gmatch(tokens[i + 3], ".")() == "r" then
          local regC = tonumber(tokens[i + 3]:gsub("r", "") or "H", 10)
          if tokens[i + 3] == "\n" then
            raise("Missing operand 2")
          elseif regC == nil then
            raise("Invalid operand 2") 
          elseif regC < 0 or regC > 7 then
            raise("Operand #2 is out of range 0 - 7")
          end
          executable = executable.."5,"..tostring(regA)..","..tostring(regB)..","..tostring(regC)..","
          i = i + 3
          ip = ip + 4
        else
          raise("Invalid operand 3")
        end
      else
        raise("Invalid operand 2")
      end 
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif tokens[i] == "cmp" then --16-bit compare
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 7")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 7 then
          raise("Operand #2 is out of range 0 - 7")
        end
        executable = executable.."11,"..tostring(regA)..","..tostring(regB)..","
        i = i + 2
        ip = ip + 3
      else
        raise("Invalid operand 1")
      end 
    end
  elseif tokens[i] == "cmpb" then --8-bit compare
    if string.gmatch(tokens[i + 1], ".")() == "r" then
      local regA = tonumber(tokens[i + 1]:gsub("r", ""), 10)
      if regA == nil then
        raise("Operand #1 is not a number")
      elseif regA < 0 or regA > 7 then
        raise("Operand #1 is out of range 0 - 15")
      end
      if string.gmatch(tokens[i + 2], ".")() == "r" then --Reg to reg
        local regB = tonumber(tokens[i + 2]:gsub("r", "") or "H", 10)
        if tokens[i + 2] == "\n" then
          raise("Missing operand 2")
        elseif regB == nil then
          raise("Invalid operand 2") 
        elseif regB < 0 or regB > 7 then
          raise("Operand #2 is out of range 0 - 15")
        end
        executable = executable.."10,"..tostring(regA)..","..tostring(regB)..","
        i = i + 2
        ip = ip + 3
      else
        raise("Invalid operand 1")
      end
    elseif tokens[i + 1] == "\n" then
      raise("Missing operand 1")
    else
      raise("Invalid operand 1")
    end
  elseif tokens[i] ~= "\n" then
    raise("Unknown instruction "..tokens[i])
  end
  
  if ip > 65535 then
    raise("Too many instructions out of 65,536 instructions limit")
  end
  i = i + 1
end

print(executable)

local tmp = ""
local execute = ""
for char in string.gmatch(executable, ".") do
  if char == "," then
    if tonumber(tmp) ~= nil then
      execute = execute..string.char(tonumber(tmp))
    else
      if symbolTable[tmp] == nil then
        print("Undefined label '"..tmp:gsub(":", "", 1).."'")
        os.exit()
      end
      --print(symbolTable[tmp])
      execute = execute..string.char(math.floor(symbolTable[tmp] / 256) % 256)..string.char(symbolTable[tmp] % 256)
    end
    
    tmp = ""
    i = i + 1
  elseif char == "\n" then
    execute = execute..string.char(tonumber(tmp))
    --print(tmp)
    tmp = ""
    i = i + 1
  else
    tmp = tmp..char
  end
end

print("Symbols:")
for k, v in pairs(symbolTable) do
  print(k, string.format("\t0x%04x", v))
end

output:write(execute)






